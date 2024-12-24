package db_template

import (
	"strings"
	"text/template"

	xl "tx-common/xlogger"
	"tx-tools/gorm-code/gorm_types"
	"tx-tools/gorm-code/templates"
	"tx-tools/utils"
)

var dbParse *template.Template
var dirtyParse *template.Template
var unionParse *template.Template

func InitDataTemplate() (err error) {
	if dbParse, err = templates.InitTemplate("dataTemplate", dataTemplateStr); err != nil {
		return err
	}
	if dirtyParse, err = templates.InitTemplate("dirtyTemplate", dataDirtyFlagsTemplateStr); err != nil {
		return err
	}
	if unionParse, err = templates.InitTemplate("unionTemplate", union2TemplateStr); err != nil {
		return err
	}
	return err
}

func GenDataTemplate(gormTypes map[string]*gorm_types.GormTypeData, destDir string) {
	if dbParse == nil || dirtyParse == nil {
		xl.SError("GenDohTemplate error: dohParse is nil.")
		return
	}
	_ = saveDirtyFlagsFile(destDir)
	for _, gorm := range gormTypes {
		for _, unionField := range gorm.UnionFields {
			unionField.UnionParse = unionParse
		}
		if b, err := templates.ParseGormData(dbParse, gorm); err == nil {
			if err = saveDataGormDateFile(gorm, b, destDir); err != nil {
				xl.SErrorf("GenDohTemplate SaveFile FileName:%s error:%s", gorm.TableName, err.Error())
			}
		} else {
			xl.SErrorf("ParseGormData error:%s", err.Error())
			return
		}
	}
	return
}

func saveDataGormDateFile(data *gorm_types.GormTypeData, bytes []byte, destDir string) error {
	fileName := utils.TemplateDataFileName(data.TableName, "") + ".go"
	err := utils.SaveFile(destDir, fileName, bytes, true, true)
	if err != nil {
		return err
	}
	xl.SInfof("SaveFile FileName:%s succeed!", fileName)
	return nil
}

func saveDirtyFlagsFile(destDir string) error {
	gtData := &gorm_types.GormTypeData{
		GormTypeBase: gorm_types.GormTypeBase{
			TableName:   "DirtyFlag",
			TypePackage: strings.ToLower(gorm_types.GetSystemName()) + "_data",
			SystemName:  gorm_types.GetSystemName(),
		},
	}
	b, err := templates.ParseGormData(dirtyParse, gtData)
	if err == nil {
		return saveDataGormDateFile(gtData, b, destDir)
	}
	return err
}
