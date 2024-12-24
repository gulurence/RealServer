package pool_template

import (
    "errors"
    "sort"
    "strings"
    "text/template"

    xl "tx-common/xlogger"
    "tx-common/xutil"
    "tx-tools/gorm-code/gorm_types"
    "tx-tools/gorm-code/templates"
    "tx-tools/utils"
)

var poolParses []*template.Template

const (
    PoolServiceType = iota
    PoolFunctionType
    PoolMaxType
)

func InitPoolTemplates() (err error) {
    poolParses = make([]*template.Template, PoolMaxType)
    if poolParses[PoolServiceType], err = templates.InitTemplate("poolServiceTemplate", poolServiceTemplateStr); err != nil {
        return err
    }
    if poolParses[PoolFunctionType], err = templates.InitTemplate("poolFuncTemplate", poolFuncTemplateStr); err != nil {
        return err
    }
    return nil
}

func GenPoolTemplate(gormTypes map[string]*gorm_types.GormTypeData, destDir string) {
    data := initPoolGormData(gormTypes)
    for key, parses := range poolParses {
        if b, err := templates.ParseGormPoolData(parses, data); err == nil {
            if err := savePoolDateFile(key, b, destDir); err != nil {
                xl.SErrorf("GenLogicTemplate SaveFile Type:%d error:%s", key, err.Error())
            }
        } else {
            xl.SErrorf("ParseGormData error:%s", err.Error())
            return
        }
    }

    return
}

func initPoolGormData(gorms map[string]*gorm_types.GormTypeData) *gorm_types.GormPoolData {
    _data := &gorm_types.GormPoolData{
        GormTypeBase: gorm_types.GormTypeBase{},
        PoolFields:   []*gorm_types.GormPoolField{},
        Imports:      gorm_types.GetImports(),
    }
    for _, gorm := range gorms {
        if len(_data.GormTypeBase.TableName) <= 0 {
            _data.GormTypeBase = gorm.GormTypeBase
        }
        if gorm.GormTypeBase.TableName != "Base" {
            field := &gorm_types.GormPoolField{
                TypePackage: gorm.TypePackage,
                TableName:   gorm.TableName,
                SystemName:  gorm.SystemName,
            }
            _data.PoolFields = append(_data.PoolFields, field)
        }
    }
    sort.Slice(_data.PoolFields, func(i, j int) bool {
        return strings.Compare(_data.PoolFields[i].TableName, _data.PoolFields[j].TableName) < 0
    })
    return _data
}

func savePoolDateFile(poolType int, bytes []byte, destDir string) error {
    fileName := ".go"
    switch poolType {
    case PoolFunctionType:
        fileName = "pool_func.go"
    case PoolServiceType:
        fileName = "pool_service.go"
    default:
        return errors.New("savePoolDateFile PoolType is error:" + xutil.NumToString(poolType))
    }
    err := utils.SaveFile(destDir, fileName, bytes, true, true)
    if err != nil {
        return err
    }
    xl.SInfof("SaveFile FileName:%s succeed!", fileName)
    return nil
}
