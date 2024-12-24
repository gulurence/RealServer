package gorm_types

import (
	"reflect"
	"strings"
	"text/template"

	xl "tx-common/xlogger"
	"tx-tools/utils"
)

type GormTypeData struct {
	GormTypeBase
	GormFields     []GormField
	DataField      *DataField
	MySqlClient    string
	RedisClient    string
	FieldMaps      []*GormFieldData
	PrimaryKey     *GormFieldData
	HashKey        *GormFieldData
	OnlyPrimaryKey bool
	RedisCache     bool
	RedisCacheTime int64
	UniqueKey      map[string][]*GormFieldData
	IndexKey       map[string]*GormFieldData
	QueryFields    []*QueryFieldData
	NextFields     []*GormFieldData
	LikeFields     []*GormFieldData
	GroupFields    []*GormFieldData
	UnionFields    []*UnionFieldData
	unionParse     *template.Template
	PoolTableList  []string
	Imports        IImports
}

type GormDataType int

const (
	GormDataTypeNone   = GormDataType(0)
	GormDataTypeSingle = 1
	GormDataTypeList   = 2
	GormDataTypeMap    = 3
	GormDataTypeMax    = 4
)

type GormField struct {
	Key        string
	Type       reflect.Type
	FieldName  string
	FieldValue string
	FieldType  string
}

type DataField struct {
	Key    string
	GDType GormDataType
}

func (gf *GormField) Init() *GormField {
	if len(gf.Key) > 0 {
		gf.FieldName = utils.UpperRightWord(gf.Key)
		gf.FieldValue = utils.LowerRightWord(gf.Key)
		gf.FieldType = gf.Type.String()
	}
	return gf
}

type GormTypeConfig struct {
	GormField   GormField
	GormFields  []GormField
	DataField   DataField
	DataType    reflect.Type
	RdCacheTime int64
	QueryKeys   map[string][]string //联合查询
	LikeKeys    []string            //模糊查询
	GroupKeys   []string            //组查询
	NextKeys    []string            //Next序列查询
	UnionKeys   map[string][]string //唯一联合查询
}

func NewGormTypeData(config *GormTypeConfig) *GormTypeData {
	_tableName := "Base"
	if config.DataType != nil {
		_tableName = config.DataType.Name()
	}

	if config.GormFields != nil {
		for i, field := range config.GormFields {
			field.Init()
			config.GormFields[i] = field
		}
	} else {
		config.GormField.Init()
		config.GormFields = append(config.GormFields, config.GormField)
	}

	//config.DataFiled.Init()
	gormData := &GormTypeData{
		GormTypeBase: GormTypeBase{
			TableName:    _tableName,
			TypePackage:  strings.ToLower(_systemName) + "_data",
			LogicPackage: strings.ToLower(_systemName) + "_logic",
			DaoPackage:   strings.ToLower(_systemName) + "_dao",
			PoolPackage:  strings.ToLower(_systemName) + "_dbp",
			RedisPackage: strings.ToLower(_systemName) + "_redis",
			SystemName:   _systemName,
		},
		RedisClient:    strings.ToLower(_systemName) + "_cluster." + `RedisClient()`,
		MySqlClient:    strings.ToLower(_systemName) + "_cluster." + `MySqlClient(dbId)`,
		DataField:      &config.DataField,
		GormFields:     config.GormFields,
		FieldMaps:      []*GormFieldData{},
		PrimaryKey:     nil,
		OnlyPrimaryKey: false,
		RedisCache:     config.RdCacheTime > 0,
		RedisCacheTime: config.RdCacheTime,
		UniqueKey:      make(map[string][]*GormFieldData),
		IndexKey:       make(map[string]*GormFieldData),
		QueryFields:    []*QueryFieldData{},
		NextFields:     []*GormFieldData{},
		LikeFields:     []*GormFieldData{},
		GroupFields:    []*GormFieldData{},
		UnionFields:    []*UnionFieldData{},
		Imports:        _imports,
	}

	gormData.InitFieldMaps(config)

	if config.DataField.GDType == GormDataTypeMap {
		if gormData.PrimaryKey == nil {
			xl.SErrorf("NewGormTypeData error, PrimaryKey is nil KeyName:%s", config.DataField.Key)
			return nil
		}
		if gormData.PrimaryKey.Name != config.DataField.Key {
			xl.SErrorf("NewGormTypeData error, PrimaryKey:%s is nil KeyName:%s", gormData.PrimaryKey.Name, config.DataField.Key)
			return nil
		}
	}

	//    gormDataStr := xjson.MarshalString(gormData)
	//    xl.SDebugf(gormDataStr)
	return gormData
}

func (g *GormTypeData) InitFieldMaps(config *GormTypeConfig) {
	if config.DataType == nil {
		return
	}

	queryMaps := make(map[string][]*reflect.StructField)
	var likeList []*reflect.StructField
	var nextList []*reflect.StructField
	var groupList []*reflect.StructField
	unionMaps := make(map[string][]*reflect.StructField)
	for i := 0; i < config.DataType.NumField(); i++ {
		field := config.DataType.Field(i)
		fieldData := &GormFieldData{
			GormTags: ProcessGormTags(field.Tag, i),
			Name:     field.Name,
			GoType:   field.Type.String(),
		}
		if fieldData.IsPrimaryKey {
			g.PrimaryKey = fieldData
			g.OnlyPrimaryKey = true
		}
		if field.Name == config.DataField.Key && g.PrimaryKey == nil {
			g.PrimaryKey = fieldData
		}
		if len(fieldData.UniqueKey) > 0 {
			g.UniqueKey[fieldData.UniqueKey] = append(g.UniqueKey[fieldData.UniqueKey], fieldData)
		}
		g.FieldMaps = append(g.FieldMaps, fieldData)

		for key, queries := range config.QueryKeys {
			for _, query := range queries {
				if query == field.Name {
					if list, ok := queryMaps[key]; ok {
						list = append(list, &field)
						queryMaps[key] = list
					} else {
						queryMaps[key] = []*reflect.StructField{&field}
					}
				}
			}
		}

		for _, like := range config.LikeKeys {
			if like == field.Name {
				likeList = append(likeList, &field)
			}
		}
		for _, next := range config.NextKeys {
			if next == field.Name {
				nextList = append(nextList, &field)
			}
		}
		for _, list := range config.GroupKeys {
			if list == field.Name {
				groupList = append(groupList, &field)
			}
		}
		for key, unions := range config.UnionKeys {
			for _, query := range unions {
				if query == field.Name {
					if list, ok := unionMaps[key]; ok {
						list = append(list, &field)
						unionMaps[key] = list
					} else {
						unionMaps[key] = []*reflect.StructField{&field}
					}
				}
			}
		}
	}
	for i := range g.FieldMaps {
		g.FieldMaps[i].GormTags.PrimaryName = g.PrimaryKey.Name
	}

	for key, fields := range queryMaps {
		query := &QueryFieldData{
			TypePackage: g.TypePackage,
			MySqlClient: g.MySqlClient,
			PoolPackage: g.PoolPackage,
			QueryKey:    key,
			TableName:   g.TableName,
		}
		for _, field := range fields {
			query.QueryInput += utils.LowerRightWord(field.Name) + " " + field.Type.String() + ", "
			query.QueryValue += utils.LowerRightWord(field.Name) + ", "
		}
		query.QueryInput = strings.TrimRight(query.QueryInput, ", ")
		query.QueryValue = strings.TrimRight(query.QueryValue, ", ")
		g.QueryFields = append(g.QueryFields, query)
	}

	g.LikeFields = g.FillFieldDataList(likeList)
	g.GroupFields = g.FillFieldDataList(groupList)
	g.NextFields = g.FillFieldDataList(nextList)
	for key, unions := range unionMaps {
		g.UnionFields = append(g.UnionFields, (&UnionFieldData{
			UnionParse: g.unionParse,
			UnionKeys:  config.UnionKeys[key],
			UnionName:  key,
			FieldList:  g.FillFieldDataList(unions),
		}).InitUnionStruct(utils.CamelizeStr(g.TableName, g.SystemName)))
	}
}

func (g *GormTypeData) FillFieldDataList(fields []*reflect.StructField) (list []*GormFieldData) {
	var index = 0
	for _, field := range fields {
		fieldData := &GormFieldData{
			GormTags:    ProcessGormTags(field.Tag, index),
			GormFields:  g.GormFields,
			DaoPackage:  g.DaoPackage,
			PrimaryKey:  g.PrimaryKey,
			HashKey:     g.HashKey,
			MySqlClient: g.MySqlClient,
			PoolPackage: g.PoolPackage,
			TableName:   g.TableName,
			TypePackage: g.TypePackage,
			Name:        utils.LowerRightWord(field.Name),
			GoType:      field.Type.String(),
		}
		list = append(list, fieldData)
		index++
	}
	return list
}

func (g *GormTypeData) DBImports() []string {
	hasTime := false
	for _, field := range g.FieldMaps {
		if field.GoType == "time.Time" {
			hasTime = true
		}
	}
	list := _imports.DBImports(g.GormTypeBase.TableName)
	list = append(list, `"`+g.SystemPath+`/types"`)
	list = append(list, `"tx-common/xbitset"`)
	if hasTime {
		list = append(list, `"time"`)
	}
	return list
}

func (g *GormTypeData) LogicImports() (list []string) {
	if g.DataField.GDType == GormDataTypeMap {
		list = []string{
			`"errors"`,
			``}
	}
	list = append(list, `"`+g.SystemPath+`/dao"`)
	list = append(list, `"`+g.SystemPath+`/data"`)
	list = append(list, `"`+g.SystemPath+`/pool"`)
	if g.RedisCache {
		list = append(list, `"`+g.SystemPath+`/redis"`)
	}
	list = append(list, ``)
	list = append(list, `xl "tx-common/xlogger"`)
	if g.DataField.GDType == GormDataTypeMap {
		list = append(list, `"tx-common/xutil"`)
	}
	list = append(list, _imports.LogicImports(g.DataField.GDType)...)
	return list
}

func (g *GormTypeData) DaoImports() (list []string) {
	list = []string{
		`"errors"`,
		`"tx-common/xutil"`,
		``}
	list = append(list, `"`+g.SystemPath+`/cluster"`)
	list = append(list, `"`+g.SystemPath+`/data"`)
	list = append(list, `"`+g.SystemPath+`/pool"`)
	list = append(list, `"`+g.SystemPath+`/types"`)
	list = append(list, _imports.DaoImports(g.GormTypeBase.TableName)...)
	return list
}

func (g *GormTypeData) PoolImports() (list []string) {
	list = append(list, `"`+g.SystemPath+`/pool"`)
	list = append(list, `"`+g.SystemPath+`/data"`)
	list = append(list, _imports.PoolImports(g.GormTypeBase.TableName)...)
	return list
}

func (g *GormTypeData) RedisImports() (list []string) {
	if g.HasFields() {
		list = []string{
			`"errors"`,
			`"strconv"`,
			`"time"`,
			``}
	} else {
		list = []string{
			`"errors"`,
			`"time"`,
			``}
	}

	list = append(list, `"`+g.SystemPath+`/cluster"`)
	list = append(list, `"`+g.SystemPath+`/data"`)
	list = append(list, `"`+g.SystemPath+`/pool"`)
	list = append(list, ``)
	list = append(list, `"tx-common/xjson"`)
	if g.DataField.GDType == GormDataTypeMap {
		list = append(list, `xl "tx-common/xlogger"`)
		list = append(list, `"tx-common/xutil"`)
	}
	list = append(list, _imports.RedisImports(g.GormTypeBase.TableName)...)
	return list
}

func (g *GormTypeData) GormFieldsRedisKey() (str string) {
	str = `"` + g.TableName + `"`
	for _, field := range g.GormFields {
		if len(field.FieldName) > 0 {
			if field.FieldType == "String" {
				str += ` + "." + ` + field.FieldValue
			} else {
				str += ` + "." + strconv.FormatInt(int64(` + field.FieldValue + `), 10)`
			}
		}
	}
	return str
}

func (g *GormTypeData) GormFieldsInputs() (str string) {
	if g.GormFields != nil {
		for _, field := range g.GormFields {
			str += field.FieldValue + " " + field.FieldType + ", "
		}
	}
	return strings.TrimRight(str, ", ")
}

func (g *GormTypeData) GormHashInputs() (str string) {
	if g.GormFields != nil {
		for _, field := range g.GormFields {
			str += field.FieldValue + " " + field.FieldType + ", "
		}
	}
	return strings.TrimRight(str, ", ")
}

func (g *GormTypeData) HasFields() bool {
	return len(g.GormFieldsValue()) > 0
}

func (g *GormTypeData) GormFieldsValue() (str string) {
	if g.GormFields != nil {
		for _, field := range g.GormFields {
			str += field.FieldValue + ", "
		}
	}
	return strings.TrimRight(str, ", ")
}

func (g *GormTypeData) GormGetFields(inStr string) (str string) {
	for _, field := range g.GormFields {
		str += inStr + ".Get" + field.FieldName + "(), "
	}
	return strings.TrimRight(str, ", ")
}

func (g *GormTypeData) DataFlagsString() (str string) {
	for _, field := range g.FieldMaps {
		str += `"` + field.Column + `", `
	}
	return strings.TrimRight(str, ", ")
}

func (g *GormTypeData) DataFlagsNum() int {
	return len(g.FieldMaps)
}

func (g *GormTypeData) HasNextField() bool {
	return len(g.NextFields) > 0
}

func (g *GormTypeData) GormFieldsStr(inStr string) (str string) {
	for _, field := range g.GormFields {
		if len(field.FieldName) > 0 {
			str += inStr + "." + field.FieldName + `, `
		}
	}
	return strings.TrimRight(str, ", ")
}

func (g *GormTypeData) UpdateCast(inStr string) (str string) {
	if g.PrimaryKey.GoType == "int32" {
		return "int32(" + inStr + ")"
	}
	return inStr
}
