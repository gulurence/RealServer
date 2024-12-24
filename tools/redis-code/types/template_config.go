package redis_ttypes

import "reflect"

type RedisField struct {
	FieldName string
	FieldType reflect.Type
}

type TemplateConfig struct {
	ClientType RedisClientType
	RedisType  RedisDataType
	RedisField []RedisField
	DataType   reflect.Type
	DataName   string
	DataField  map[string]reflect.Type
	MaxCount   int64
}

type TemplateFileConfig struct {
	FileName string
	Configs  map[string]*TemplateConfig
}

func InitTemplateFileConfig(fileName string, configs map[string]*TemplateConfig) *TemplateFileConfig {
	return (&TemplateFileConfig{
		FileName: fileName,
		Configs:  configs,
	}).Init()
}

func (tfc *TemplateFileConfig) Init() *TemplateFileConfig {
	return tfc
}
