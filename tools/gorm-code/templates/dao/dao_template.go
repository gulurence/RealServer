package dao_template

import (
	"text/template"

	xl "tx-common/xlogger"
	"tx-tools/gorm-code/gorm_types"
	"tx-tools/gorm-code/templates"
	"tx-tools/utils"
)

var daoParses []*template.Template

func InitDaoTemplates() (err error) {
	daoParses = make([]*template.Template, gorm_types.GormDataTypeMax)
	//if daoParses[gorm_types.GormDataTypeNone], err = templates.InitTemplate("logicBaseTemplate", logicBaseTemplateStr); err != nil {
	//    return err
	//}
	if daoParses[gorm_types.GormDataTypeSingle], err = templates.InitTemplate("daoSingleTemplate", daoSingleTemplateStr); err != nil {
		return err
	}
	if daoParses[gorm_types.GormDataTypeList], err = templates.InitTemplate("daoListTemplate", daoListTemplate); err != nil {
		return err
	}
	if daoParses[gorm_types.GormDataTypeMap], err = templates.InitTemplate("daoMapTemplate", daoMapTemplateStr); err != nil {
		return err
	}
	return nil
}

func GenDaoTemplate(gormTypes map[string]*gorm_types.GormTypeData, destDir string) {
	for _, gorm := range gormTypes {
		if gorm.DataField.GDType == gorm_types.GormDataTypeNone {
			continue
		}
		if b, err := templates.ParseGormData(daoParses[gorm.DataField.GDType], gorm); err == nil {
			if err = saveDaoGormDateFile(gorm, b, destDir); err != nil {
				xl.SErrorf("GenDaoTemplate SaveFile FileName:%s error:%s", gorm.TableName, err.Error())
			}
		} else {
			xl.SErrorf("GenDaoTemplate ParseGormData error:%s", err.Error())
			return
		}
	}
	return
}

func saveDaoGormDateFile(data *gorm_types.GormTypeData, bytes []byte, destDir string) error {
	fileName := utils.TemplateDataFileName(data.TableName, "dao") + ".go"
	err := utils.SaveFile(destDir, fileName, bytes, true, true)
	if err != nil {
		return err
	}
	xl.SInfof("SaveFile FileName:%s succeed!", fileName)
	return nil
}
