package gorm_types

import (
    "reflect"
    "strings"
)

type GormTags struct {
    IsPrimaryKey    bool
    IsAutoIncrement bool
    ColumnIndex     int
    PrimaryName     string
    Column          string
    UniqueKey       string
    IndexKey        string
}

func ProcessGormTags(structTag reflect.StructTag, index int) GormTags {
    tagStr := structTag.Get("gorm")
    args := strings.Split(tagStr, ";")
    gorTags := GormTags{
        IsPrimaryKey:    false,
        IsAutoIncrement: false,
        ColumnIndex:     index + 1,
        Column:          "",
        UniqueKey:       "",
        IndexKey:        "",
    }
    for _, arg := range args {
        if arg == "PRIMARY_KEY" || arg == "primary_key" {
            gorTags.IsPrimaryKey = true
        }
        if arg == "AUTO_INCREMENT" || arg == "auto_increment" {
            gorTags.IsAutoIncrement = true
        }
        vl := strings.Split(arg, ":")
        if len(vl) < 2 {
            continue
        }
        if vl[0] == "column" {
            gorTags.Column = vl[1]
        } else if vl[0] == "unique_index" {
            gorTags.UniqueKey = vl[1]
        } else if vl[0] == "index" {
            gorTags.IndexKey = vl[1]
        }
    }
    return gorTags
}
