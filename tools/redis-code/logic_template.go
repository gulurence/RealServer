package redis_code

import (
	"bytes"
	"sort"
	"strings"
	"text/template"
	"time"

	xl "tx-common/xlogger"
	redisTemplate "tx-tools/redis-code/template"
	redisTypes "tx-tools/redis-code/types"
	"tx-tools/utils"
)

var logicParses []*template.Template

func InitLogicTemplates() (err error) {
	logicParses = make([]*template.Template, redisTypes.RedisDataTypeMax)
	if logicParses[redisTypes.RedisDataTypFile], err = InitTemplate("RedisFileTStr", redisTemplate.RedisFileTStr); err != nil {
		return err
	}
	if logicParses[redisTypes.RedisDataTypeKey], err = InitTemplate("RedisKeyTStr", redisTemplate.RedisKeyTStr); err != nil {
		return err
	}
	if logicParses[redisTypes.RedisDataTypeList], err = InitTemplate("RedisListTStr", redisTemplate.RedisListTStr); err != nil {
		return err
	}
	if logicParses[redisTypes.RedisDataTypeHSet], err = InitTemplate("RedisHSetTStr", redisTemplate.RedisHSetTStr); err != nil {
		return err
	}
	if logicParses[redisTypes.RedisDataTypeZSet], err = InitTemplate("RedisZSetTStr", redisTemplate.RedisZSetTStr); err != nil {
		return err
	}
	if logicParses[redisTypes.RedisDataTypeKeyZSet], err = InitTemplate("RedisKeyZSetTStr", redisTemplate.RedisKeyZSetTStr); err != nil {
		return err
	}
	if logicParses[redisTypes.RedisDataTypeStream], err = InitTemplate("RedisStreamTStr", redisTemplate.RedisStreamTStr); err != nil {
		return err
	}
	if logicParses[redisTypes.RedisDataTypeGeo], err = InitTemplate("RedisGeoTStr", redisTemplate.RedisGeoTStr); err != nil {
		return err
	}
	if logicParses[redisTypes.RedisDataTypeInt64HSet], err = InitTemplate("RedisInt64HSetTStr", redisTemplate.RedisInt64HSetTStr); err != nil {
		return err
	}
	if logicParses[redisTypes.RedisDataTypeFloat64HSet], err = InitTemplate("RedisFloatHSetTStr", redisTemplate.RedisFloatHSetTStr); err != nil {
		return err
	}
	if logicParses[redisTypes.RedisDataTypeNoneHSet], err = InitTemplate("RedisNoneHSetTStr", redisTemplate.RedisNoneHSetTStr); err != nil {
		return err
	}
	if logicParses[redisTypes.RedisDataTypeKeyInt64], err = InitTemplate("RedisKeyIntTStr", redisTemplate.RedisKeyInt64TStr); err != nil {
		return err
	}

	if logicParses[redisTypes.RedisDataTypeStringHSet], err = InitTemplate("RedisStringHSetTStr", redisTemplate.RedisStringHSetTStr); err != nil {
		return err
	}

	if logicParses[redisTypes.RedisDataTypeChannel], err = InitTemplate("RedisChannelTStr", redisTemplate.RedisChannelTStr); err != nil {
		return err
	}
	return nil
}

func GenLogicTemplate(redisType map[string]*redisTypes.TemplateFileConfig, destDir string) {
	for key, config := range redisType {
		redisData := redisTypes.NewRedisData(key, config)
		if b, err := ParseFileRedisData(redisData); err == nil {
			if err = saveLogicGormDateFile(redisData.FileName, redisData, b, destDir); err != nil {
				xl.SErrorf("GenLogicTemplate SaveFile FileName:%s error:%s", key, err.Error())
			}
		} else {
			xl.SErrorf("ParseGormData error:%s", err.Error())
			return
		}
	}
	return
}

func ParseFileRedisData(redisData *redisTypes.RedisData) (fBytes []byte, err error) {
	if _bytes, err := ParseRedisFileData(logicParses[redisTypes.RedisDataTypFile], redisData); err != nil {
		return nil, err
	} else {
		fBytes = append(fBytes, _bytes...)
	}

	sort.Slice(redisData.Entities, func(i, j int) bool {
		return strings.Compare(redisData.Entities[i].DataTypeName, redisData.Entities[j].DataTypeName) < 0
	})
	for _, entity := range redisData.Entities {
		if _bytes, err := ParseRedisEntityData(logicParses[entity.DataType], entity); err != nil {
			return nil, err
		} else {
			fBytes = append(fBytes, _bytes...)
		}
	}

	return fBytes, err
}

func saveLogicGormDateFile(fileName string, data *redisTypes.RedisData, bytes []byte, destDir string) error {
	if err := utils.SaveFile(destDir, strings.ToLower(fileName+".go"), bytes, true, true); err != nil {
		return err
	}
	xl.SInfof("SaveFile FileName:%s succeed!", fileName)
	return nil
}

func ParseRedisFileData(parse *template.Template, data *redisTypes.RedisData) ([]byte, error) {
	var buf bytes.Buffer
	if err := parse.Execute(&buf, data); err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

func ParseRedisEntityData(parse *template.Template, entity *redisTypes.RedisEntity) ([]byte, error) {
	var buf bytes.Buffer
	if err := parse.Execute(&buf, entity); err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

func InitTemplate(key, str string) (*template.Template, error) {
	return template.New(key).Funcs(template.FuncMap{
		"CamelizeStr":    utils.CamelizeStr,
		"LowerLeftWord":  utils.LowerLeftWord,
		"LowerRightWord": utils.LowerRightWord,
		"now": func() string {
			return time.Now().Format("2006-01-02 Z07:00")
		},
	}).Parse(str)
}
