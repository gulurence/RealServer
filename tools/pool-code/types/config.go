package pool_types

import "reflect"

type TemplateFileConfig struct {
    NameSpace  string
    DirPath    string
    DataFields []reflect.Type
}

func InitTemplateFileConfig(nameSpace, dirPath string, fields []reflect.Type) *TemplateFileConfig {
    return (&TemplateFileConfig{
        NameSpace:  nameSpace,
        DirPath:    dirPath,
        DataFields: fields,
    }).Init()
}

func (tfc *TemplateFileConfig) Init() *TemplateFileConfig {
    return tfc
}
