package redis_template

var RedisHSetTStr = `{{- if .KeyEntity }}
//{{.KeyDataStr}} //////////////////////{{.KeyDataStr}}/////////////////////////////////////////////////////
type {{.KeyDataStr}} struct{
    {{- range .KeyEntity.KeyDataList}}
    {{.}}
    {{- end}}
}

func gen{{.KeyDataStr}}({{.KeyFuncInput}}) string {
    return fmt.Sprintf("{{.KeyStr}}", {{.KeyEntity.KeyGenList}})
}
{{- else }}
//gen{{.KeyDataStr}} //////////////////////{{.KeyDataStr}}/////////////////////////////////////////////////////
func gen{{.KeyDataStr}}({{.KeyFuncInput}}) string {
    return fmt.Sprintf("{{.KeyStr}}", {{.KeyFuncGen}})
}
{{- end}}

{{- if .HSetDataTypeInput }}

func Set{{.DataTypeName}}({{.KeyFuncInput}}, {{.DataFuncInput}}, info *{{.DataTypeStr}}) error {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HSet(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), {{.DataFuncGen}}, xjson.MarshalString(info)).Err()
}

func Get{{.DataTypeName}}({{.KeyFuncInput}}, {{.DataFuncInput}}) (info *{{.DataTypeStr}}, err error) {
    return Get{{.DataTypeName}}ByFunc({{.KeyFuncField}}, {{.DataFuncField}}, func() *{{.DataTypeStr}} {
        return &{{.DataTypeStr}}{}
    })
}

func Get{{.DataTypeName}}ByFunc({{.KeyFuncInput}}, {{.DataFuncInput}}, newFunc func() *{{.DataTypeStr}}) (info *{{.DataTypeStr}}, err error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    if str, err := _client.HGet(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), {{.DataFuncGen}}).Result(); err == nil {
        info = newFunc()
        if err = xjson.Unmarshal([]byte(str), info); err == nil {
            return info, nil
        }
    }
    return nil, err
}

func Get{{.DataTypeName}}Len({{.KeyFuncInput}}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HLen(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}})).Result()
}

func Del{{.DataTypeName}}({{.KeyFuncInput}}, {{.DataFuncInput}}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HDel(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), {{.DataFuncGen}}).Result()
}
{{- else}}
{{- range .RedisFunList}}

func Set{{.DataTypeName}}{{.FunStr}}Info({{.KeyFuncInput}}, info *{{.DataTypeStr}}) error {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HSet(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), "{{.FunKeyStr}}", xjson.MarshalString(info)).Err()
}

func Get{{.DataTypeName}}{{.FunStr}}Info({{.KeyFuncInput}}) (*{{.DataTypeStr}}, error) {
    return Get{{.DataTypeName}}{{.FunStr}}InfoByFunc({{.KeyFuncField}}, func() *{{.DataTypeStr}} {
        return &{{.DataTypeStr}}{}
    })
}

func Get{{.DataTypeName}}{{.FunStr}}InfoByFunc({{.KeyFuncInput}}, newFunc func() *{{.DataTypeStr}}) (info *{{.DataTypeStr}}, err error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    str, err := _client.HGet(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), "{{.FunKeyStr}}").Result()
    if err == nil {
        info = newFunc()
        if err = xjson.Unmarshal([]byte(str), info); err == nil {
            return info, nil
        }        
    }   
    return nil, err
}

func Del{{.DataTypeName}}{{.FunStr}}Info({{.KeyFuncInput}}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HDel(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), "{{.FunKeyStr}}").Result()
}
{{- end}}
{{- end}}

{{- if .HSetDataTypeInput }}

func Get{{.DataTypeName}}List({{.KeyFuncInput}}) ([]*{{.DataTypeStr}}, error) {
    return Get{{.DataTypeName}}ListByFunc({{.KeyFuncField}}, func() *{{.DataTypeStr}} {
        return &{{.DataTypeStr}}{}
    })
}

func Get{{.DataTypeName}}ListByFunc({{.KeyFuncInput}}, newFunc func() *{{.DataTypeStr}}) (list []*{{.DataTypeStr}}, err error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    strList, err := _client.HGetAll(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}})).Result()
    if err != nil {
        return list, err
    }
    for _, str := range strList {
        info := newFunc()
        if err = xjson.Unmarshal([]byte(str), info); err == nil {
            list = append(list, info)
        } else {
            xl.SErrorf("Get{{.DataTypeName}}List Error, Unmarshal String:%s, Error:%s", str, err.Error())
        }
    }
    return list, err
}

func Get{{.DataTypeName}}Map({{.KeyFuncInput}}) (map[string]*{{.DataTypeStr}}, error) {
    return Get{{.DataTypeName}}MapByFunc({{.KeyFuncField}}, func() *{{.DataTypeStr}} {
        return &{{.DataTypeStr}}{}
    })
}

func Get{{.DataTypeName}}MapByFunc({{.KeyFuncInput}}, newFunc func() *{{.DataTypeStr}}) (maps map[string]*{{.DataTypeStr}}, err error) {
    //maps = make(map[string]*{{.DataTypeStr}})
    _client := {{.RedisPackage}}.{{.RedisClient}}
    strList, err := _client.HGetAll(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}})).Result()
    if err != nil {
        return maps, err
    }
    for key, str := range strList {
        info := newFunc()
        if err = xjson.Unmarshal([]byte(str), info); err == nil {
            maps[key] = info
        } else {
            xl.SErrorf("Get{{.DataTypeName}}List Error, Unmarshal String:%s, Error:%s", str, err.Error())
        }
    }
    return maps, err
}

func Get{{.DataTypeName}}ByList({{.KeyFuncInput}}, {{.DataFuncListInput}}) ([]*{{.DataTypeStr}}, error) {
    return Get{{.DataTypeName}}ByListFunc({{.KeyFuncField}}, {{.DataFuncField}}s, func() *{{.DataTypeStr}} {
        return &{{.DataTypeStr}}{}
    })
}

func Get{{.DataTypeName}}ByListFunc({{.KeyFuncInput}}, {{.DataFuncListInput}}, newFunc func() *{{.DataTypeStr}}) (list []*{{.DataTypeStr}}, err error) {
    {{.DataFuncListGen}}
    _client := {{.RedisPackage}}.{{.RedisClient}}
    strList, err := _client.HMGet(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), {{.DataFuncListField}}...).Result()
    if err != nil {
        return list, err
    }
    for _, str := range strList {
        if str == nil {
            continue
        }
        info := newFunc()
        if err = xjson.Unmarshal([]byte(str.(string)), info); err == nil {
            list = append(list, info)
        } else {
            xl.SErrorf("Get{{.DataTypeName}}ByList Error, Unmarshal String:%s, Error:%s", str, err.Error())
        }
    }
    return list, err
}

func Get{{.DataTypeName}}ByStrings({{.KeyFuncInput}}, _list []string) ([]*{{.DataTypeStr}}, error) {
    return Get{{.DataTypeName}}ByStringsFunc({{.KeyFuncField}}, _list, func() *{{.DataTypeStr}} {
        return &{{.DataTypeStr}}{}
    })
}

func Get{{.DataTypeName}}ByStringsFunc({{.KeyFuncInput}}, _list []string, newFunc func() *{{.DataTypeStr}}) (list []*{{.DataTypeStr}}, err error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    strList, err := _client.HMGet(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), _list...).Result()
    if err != nil {
        return list, err
    }
    for _, str := range strList {
        if str == nil {
            continue
        }
        info := newFunc()
        if err = xjson.Unmarshal([]byte(str.(string)), info); err == nil {
            list = append(list, info)
        } else {
            xl.SErrorf("Get{{.DataTypeName}}ByStrings Error, Unmarshal String:%s, Error:%s", str, err.Error())
        }
    }
    return list, err
}
{{- else}}

func Get{{.DataTypeName}}List({{.KeyFuncInput}}) (list map[string]string, err error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HGetAll(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}})).Result()
}

func Del{{.DataTypeName}}List({{.KeyFuncInput}}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.Del(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}})).Result()
}

func Get{{.DataTypeName}}ByStrings({{.KeyFuncInput}}, _list []string) (list []interface{}, err error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HMGet(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), _list...).Result()
}
{{- end}}
`
