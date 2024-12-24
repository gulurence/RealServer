package redis_ttypes

import (
	"reflect"
	"sort"
	"strings"
	_ "strings"

	"tx-tools/utils"
	_ "tx-tools/utils"
)

type RedisEntityKey struct {
	KeyDataList []string
	KeyGenList  string
}

type RedisFunEntity struct {
	RedisPackage      string
	RedisClient       string
	FunStr            string
	FunKeyStr         string
	KeyDataStr        string
	KeyFuncInput      string
	KeyFuncField      string
	KeyFuncListInput  string
	KeyFuncListField  string
	KeyFuncGen        string
	DataTypeName      string
	DataTypeStr       string
	DataFuncInput     string
	DataFuncField     string
	DataFuncListInput string
	DataFuncListField string
	DataFieldName     string
	DataFuncGen       string
	DataFuncListGen   string
}

type RedisEntity struct {
	DataType          RedisDataType
	KeyEntity         *RedisEntityKey
	RedisPackage      string
	RedisClient       string
	KeyDataStr        string
	KeyStr            string
	KeyFuncInput      string
	KeyFuncListInput  string
	KeyFuncListField  string
	KeyFuncField      string
	KeyFuncGen        string
	DataTypeName      string
	DataTypeStr       string
	DataFuncType      string
	DataFuncInput     string
	DataFuncListInput string
	DataFuncField     string
	DataFuncGen       string
	DataFuncListGen   string
	DataFuncListField string
	MaxCount          int64
	RedisFunList      []*RedisFunEntity
}

type RedisData struct {
	IImports
	RedisTypeBase
	FileKey  string
	FileName string
	Entities []*RedisEntity
}

func NewRedisData(keyStr string, fConfig *TemplateFileConfig) *RedisData {
	rd := (&RedisData{
		IImports: _imports,
		RedisTypeBase: RedisTypeBase{
			RedisPackage: utils.LowerRightWord(_systemName) + "_redis",
			TypePackage:  utils.LowerRightWord(_systemName) + "_db",
			LogicPackage: _keyName + "redis_dao",
			SystemName:   _systemName,
		},
		FileKey:  keyStr,
		FileName: fConfig.FileName,
		Entities: []*RedisEntity{},
	}).InitEntities(fConfig.Configs)

	sort.Slice(rd.Entities, func(i, j int) bool {
		return strings.Compare(rd.Entities[i].DataTypeName, rd.Entities[j].DataTypeName) < 0
	})

	return rd
}

func (rd *RedisData) InitEntities(configs map[string]*TemplateConfig) *RedisData {
	for key, config := range configs {
		entity := &RedisEntity{
			RedisPackage:     rd.RedisPackage,
			RedisClient:      config.ClientType.String(),
			DataType:         config.RedisType,
			KeyEntity:        nil,
			KeyStr:           key,
			KeyFuncInput:     "",
			KeyFuncListInput: "",
			KeyFuncListField: "",
			KeyFuncGen:       "",
			DataTypeName:     "",
			DataTypeStr:      "",
			MaxCount:         config.MaxCount,
			RedisFunList:     []*RedisFunEntity{},
		}
		entity.InitFields(config)
		rd.Entities = append(rd.Entities, entity)
	}
	return rd
}

func (re *RedisEntity) InitFields(config *TemplateConfig) *RedisEntity {
	if config.DataType != nil {
		re.DataTypeName = config.DataType.Name()
		re.DataTypeStr = config.DataType.String()
	}
	if len(config.DataName) > 0 {
		re.DataTypeName = config.DataName
	}
	if len(config.RedisField) <= 1 {
		for _, field := range config.RedisField {
			re.KeyFuncInput = field.FieldName + " " + field.FieldType.Name()
			re.KeyFuncField = field.FieldName
			re.KeyFuncListInput = field.FieldName + "s []" + field.FieldType.Name()
			re.KeyFuncListField = field.FieldName + "s"
			re.KeyFuncGen = field.FieldName
			re.DataFuncListInput = field.FieldName + "s []" + field.FieldType.Name()
			re.DataFuncListField = field.FieldName + "s"
		}
	} else {
		re.KeyEntity = &RedisEntityKey{
			KeyDataList: []string{},
		}
		for _, field := range config.RedisField {
			re.KeyEntity.KeyDataList = append(re.KeyEntity.KeyDataList, utils.UpperRightWord(field.FieldName)+"	"+field.FieldType.Name())
			re.KeyEntity.KeyGenList += "key." + utils.UpperRightWord(field.FieldName) + ", "
		}
		re.KeyEntity.KeyGenList = strings.TrimRight(re.KeyEntity.KeyGenList, ", ")
	}

	if len(config.DataName) > 0 {
		re.KeyDataStr = config.DataName + config.RedisType.String() + "Key"
	} else if config.DataType != nil {
		re.KeyDataStr = config.DataType.Name() + config.RedisType.String() + "Key"
	}
	re.KeyDataStr = strings.ReplaceAll(re.KeyDataStr, "_", "")
	re.KeyDataStr = strings.ReplaceAll(re.KeyDataStr, "KeyKey", "Key")

	for key, field := range config.DataField {
		funEntity := &RedisFunEntity{
			RedisPackage:      re.RedisPackage,
			RedisClient:       config.ClientType.String(),
			FunStr:            utils.UpperRightWord(key),
			FunKeyStr:         key,
			KeyDataStr:        re.KeyDataStr,
			KeyFuncField:      re.KeyFuncField,
			KeyFuncInput:      re.KeyFuncInput,
			KeyFuncListInput:  re.KeyFuncListInput,
			KeyFuncListField:  re.KeyFuncListField,
			KeyFuncGen:        re.KeyFuncGen,
			DataTypeName:      re.DataTypeName,
			DataTypeStr:       field.String(),
			DataFuncInput:     utils.LowerRightWord(key) + " " + field.String(),
			DataFuncField:     utils.LowerRightWord(key),
			DataFuncListInput: utils.LowerRightWord(key) + "s []" + field.String(),
			DataFuncGen:       utils.LowerRightWord(key),
			DataFuncListField: utils.LowerRightWord(key) + "s",
		}
		re.RedisFunList = append(re.RedisFunList, funEntity)

		if config.RedisType != RedisDataTypeZSet && config.RedisType != RedisDataTypeKeyZSet {
			if field != reflect.TypeOf("") {
				funEntity.DataFuncGen = "xutil.NumToString(" + utils.LowerRightWord(key) + ")"
				if config.RedisType != RedisDataTypeZSet && config.RedisType != RedisDataTypeKeyZSet {
					funEntity.DataFuncListField = "_list"
					funEntity.DataFuncListGen = re.genDataFuncListGenString(utils.LowerRightWord(key))
				}
			}
		} else {
			if field != reflect.TypeOf("") {
				funEntity.DataFuncGen = "xutil.NumToString(" + utils.LowerRightWord(key) + ")"
			}
			re.DataFuncListField = "_list"
			re.DataFuncListGen = re.genDataFuncListGenInterface(utils.LowerRightWord(key))
		}

		re.DataFuncInput = utils.LowerRightWord(key) + " " + field.Name()
		re.DataFuncListInput = utils.LowerRightWord(key) + "s []" + field.Name()
		re.DataFuncField = utils.LowerRightWord(key)
		re.DataFuncGen = utils.LowerRightWord(key)
		re.DataFuncListField = utils.LowerRightWord(key) + "s"
		if config.RedisType != RedisDataTypeZSet && config.RedisType != RedisDataTypeKeyZSet {
			if field != reflect.TypeOf("") {
				re.DataFuncGen = "xutil.NumToString(" + utils.LowerRightWord(key) + ")"
				if config.RedisType != RedisDataTypeZSet && config.RedisType != RedisDataTypeKeyZSet {
					re.DataFuncListField = "_list"
					re.DataFuncListGen = re.genDataFuncListGenString(utils.LowerRightWord(key))
				}
			}
		} else {
			if field != reflect.TypeOf("") {
				re.DataFuncGen = "xutil.NumToString(" + utils.LowerRightWord(key) + ")"
			}
			re.DataFuncListField = "_list"
			re.DataFuncListGen = re.genDataFuncListGenInterface(utils.LowerRightWord(key))
		}
	}

	if re.KeyEntity != nil {
		re.KeyFuncInput = "key " + re.KeyDataStr
		re.KeyFuncField = "key"
		re.KeyFuncListInput = "keys []" + re.KeyDataStr
		re.KeyFuncListField = "keys"
		re.KeyFuncGen = "key"
	}

	for i := range re.RedisFunList {
		re.RedisFunList[i].KeyDataStr = re.KeyDataStr
		if re.KeyEntity != nil {
			re.RedisFunList[i].KeyFuncInput = re.KeyFuncInput
			re.RedisFunList[i].KeyFuncGen = "key"
		}
	}

	sort.Slice(re.RedisFunList, func(i, j int) bool {
		return strings.Compare(re.RedisFunList[i].DataTypeStr, re.RedisFunList[j].DataTypeStr) < 0
	})
	return re
}

func (re *RedisEntity) genDataFuncListGenString(key string) string {
	return `_list := make([]string, len(` + key + `s))
    for i, id := range ` + key + `s {
        _list[i] = xutil.NumToString(id)
    }`
}

func (re *RedisEntity) genDataFuncListGenInterface(key string) string {
	return `_list := make([]interface{}, len(` + key + `s))
    for i, id := range ` + key + `s {
        _list[i] = xutil.NumToString(id)
    }`
}

func (rd *RedisData) LogicImports() (list []string) {
	for _, entity := range rd.Entities {
		if entity.DataType == RedisDataTypeKey {
			if len(entity.KeyFuncInput) > 0 {
				list = utils.AddImportsXXString(list, `"fmt"`)
			}
			list = utils.AddImportsXXString(list, `"tx-common/xjson"`)
			list = utils.AddImportsXXString(list, `"time"`)
			list = utils.AddImportsXXString(list, `"errors"`)
			list = utils.AddImportsXXString(list, `xl "tx-common/xlogger"`)
		}
		if entity.DataType == RedisDataTypeHSet {
			if len(entity.KeyFuncInput) > 0 {
				list = utils.AddImportsXXString(list, `"fmt"`)
			}
			list = utils.AddImportsXXString(list, `"tx-common/xjson"`)
			if entity.HSetDataTypeInput() {
				list = utils.AddImportsXXString(list, `"tx-common/xutil"`)
				list = utils.AddImportsXXString(list, `xl "tx-common/xlogger"`)
			}
		}

		if entity.DataType == RedisDataTypeNoneHSet {
			list = utils.AddImportsXXString(list, `"tx-common/xjson"`)
			list = utils.AddImportsXXString(list, `xl "tx-common/xlogger"`)
			//list = utils.AddImportsXXString(list, `"tx-common/xutil"`)
		}
		if entity.DataType == RedisDataTypeStream {
			if len(entity.KeyFuncInput) > 0 {
				list = utils.AddImportsXXString(list, `"fmt"`)
			}
			list = utils.AddImportsXXString(list, `"tx-common/xjson"`)
			list = utils.AddImportsXXString(list, `"github.com/redis/go-redis/v9"`)
		}

		if entity.DataType == RedisDataTypeZSet {
			list = utils.AddImportsXXString(list, `"tx-common/xutil"`)
			list = utils.AddImportsXXString(list, `"github.com/redis/go-redis/v9"`)
		}
		if entity.DataType == RedisDataTypeKeyZSet {
			if len(entity.KeyFuncInput) > 0 {
				list = utils.AddImportsXXString(list, `"fmt"`)
			}
			list = utils.AddImportsXXString(list, `"tx-common/xutil"`)
			list = utils.AddImportsXXString(list, `"github.com/redis/go-redis/v9"`)
		}

		if entity.DataType == RedisDataTypeChannel {
			if len(entity.KeyFuncInput) > 0 {
				list = utils.AddImportsXXString(list, `"fmt"`)
			}
			list = utils.AddImportsXXString(list, `"tx-common/xjson"`)
		}

		if entity.DataType == RedisDataTypeInt64HSet || entity.DataType == RedisDataTypeFloat64HSet || entity.DataType == RedisDataTypeStringHSet {
			if len(entity.KeyFuncInput) > 0 {
				list = utils.AddImportsXXString(list, `"fmt"`)
			}
			list = utils.AddImportsXXString(list, `"tx-common/xutil"`)
		}

		if entity.DataType == RedisDataTypeKeyInt64 {
			list = utils.AddImportsXXString(list, `"tx-common/xutil"`)
			list = utils.AddImportsXXString(list, `"time"`)
			list = utils.AddImportsXXString(list, `"errors"`)
		}

		if entity.RedisClient == "PipelineRClient()" {
			list = utils.AddImportsXXString(list, `"github.com/redis/go-redis/v9"`)
		}
		list = _imports.LogicImports(rd.FileKey, list, entity.DataType)
	}
	sort.Strings(list)
	return list
}

func (re *RedisEntity) KeyNoneInput() bool {
	if len(re.KeyFuncInput) == 0 {
		return true
	}
	return false
}

func (re *RedisEntity) HSetDataTypeInput() bool {
	if len(re.DataTypeStr) > 0 {
		return true
	}
	return false
}

func (re *RedisEntity) PipelineType() bool {
	return re.RedisClient == "PipelineRClient()"
}

func (rfe *RedisFunEntity) KeyNoneInput() bool {
	if len(rfe.KeyFuncInput) == 0 {
		return true
	}
	return false
}
