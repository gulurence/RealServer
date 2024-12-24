package redis_template

import (
	"text/template"

	xl "tx-common/xlogger"
	"tx-tools/gorm-code/gorm_types"
	"tx-tools/gorm-code/templates"
	"tx-tools/utils"
)

var redisParses []*template.Template

func InitRedisTemplates() (err error) {
	redisParses = make([]*template.Template, gorm_types.GormDataTypeMax)
	if redisParses[gorm_types.GormDataTypeNone], err = templates.InitTemplate("redisBaseTemplate", RedisBaseTStr); err != nil {
		return err
	}
	if redisParses[gorm_types.GormDataTypeSingle], err = templates.InitTemplate("redisSingleTemplate", RedisSingleTStr); err != nil {
		return err
	}
	if redisParses[gorm_types.GormDataTypeMap], err = templates.InitTemplate("redisMapTemplate", RedisMapTStr); err != nil {
		return err
	}
	return nil
}

func GenRedisTemplate(gormTypes map[string]*gorm_types.GormTypeData, destDir string) {
	for _, gorm := range gormTypes {
		if gorm.DataField.GDType == gorm_types.GormDataTypeNone {
			continue
		}
		if b, err := templates.ParseGormData(redisParses[gorm.DataField.GDType], gorm); err == nil {
			if err = saveRedisGormDateFile(gorm, b, destDir); err != nil {
				xl.SErrorf("GenRedisTemplate SaveFile FileName:%s error:%s", gorm.TableName, err.Error())
			}
		} else {
			xl.SErrorf("GenRedisTemplate ParseGormData error:%s", err.Error())
			return
		}
	}
	return
}

func saveRedisGormDateFile(data *gorm_types.GormTypeData, bytes []byte, destDir string) error {
	fileName := utils.TemplateDataFileName(data.TableName, "redis") + ".go"
	err := utils.SaveFile(destDir, fileName, bytes, true, true)
	if err != nil {
		return err
	}
	xl.SInfof("SaveFile FileName:%s succeed!", fileName)
	return nil
}
