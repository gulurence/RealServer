package logic

import (
	"errors"
	"text/template"

	xl "tx-common/xlogger"
	"tx-common/xutil"

	"tx-tools/gorm-code/gorm_types"
	"tx-tools/gorm-code/templates"
	"tx-tools/utils"
)

var logicParses []*template.Template

func InitLogicTemplates() (err error) {
	logicParses = make([]*template.Template, gorm_types.GormDataTypeMax)
	if logicParses[gorm_types.GormDataTypeNone], err = templates.InitTemplate("logicBaseTemplate", logicBaseTemplateStr); err != nil {
		return err
	}
	if logicParses[gorm_types.GormDataTypeSingle], err = templates.InitTemplate("logicSingleTemplate", logicSingleTemplateStr); err != nil {
		return err
	}
	if logicParses[gorm_types.GormDataTypeList], err = templates.InitTemplate("logicListTemplate", logicListTemplateStr); err != nil {
		return err
	}
	if logicParses[gorm_types.GormDataTypeMap], err = templates.InitTemplate("logicMapTemplate", logicMapTemplateStr); err != nil {
		return err
	}
	return nil
}

func GenLogicTemplate(gormTypes map[string]*gorm_types.GormTypeData, destDir string) {
	gormTypes["Base"] = gorm_types.NewGormTypeData(&gorm_types.GormTypeConfig{
		DataField: gorm_types.DataField{GDType: gorm_types.GormDataTypeNone},
	})
	for _, gorm := range gormTypes {
		if b, err := templates.ParseGormData(logicParses[gorm.DataField.GDType], gorm); err == nil {
			if err = saveLogicGormDateFile(gorm, b, destDir); err != nil {
				xl.SErrorf("GenLogicTemplate SaveFile FileName:%s error:%s", gorm.TableName, err.Error())
			}
		} else {
			xl.SErrorf("ParseGormData error:%s", err.Error())
			return
		}
	}
	return
}

func saveLogicGormDateFile(data *gorm_types.GormTypeData, bytes []byte, destDir string) error {
	fileName := ".go"
	switch data.DataField.GDType {
	case gorm_types.GormDataTypeNone:
		fileName = "_logic.go"
	case gorm_types.GormDataTypeSingle:
		fileName = "_single.go"
	case gorm_types.GormDataTypeList:
		fileName = "_list.go"
	case gorm_types.GormDataTypeMap:
		fileName = "_map.go"
	default:
		return errors.New("saveLogicGormDateFile GormTypeData is error:" + xutil.NumToString(data.DataField.GDType))
	}
	fileName = utils.TemplateDataFileName(data.TableName, "") + fileName
	err := utils.SaveFile(destDir, fileName, bytes, true, true)
	if err != nil {
		return err
	}
	xl.SInfof("SaveFile FileName:%s succeed!", fileName)
	return nil
}
