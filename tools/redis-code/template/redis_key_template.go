package redis_template

var RedisKeyTStr = `
{{- if .KeyEntity }}
//{{.KeyDataStr}} ///////////////////////{{.KeyDataStr}}/////////////////////////////////////////////////////
type {{.KeyDataStr}} struct{
    {{- range .KeyEntity.KeyDataList}}
    {{.}}
    {{- end}}
}

func gen{{.KeyDataStr}}({{.KeyFuncInput}}) string {
    return fmt.Sprintf("{{.KeyStr}}", {{.KeyEntity.KeyGenList}})
}
{{- else }}
//gen{{.KeyDataStr}} ///////////////////////{{.KeyDataStr}}/////////////////////////////////////////////////////
func gen{{.KeyDataStr}}({{.KeyFuncInput}}) string {
    return fmt.Sprintf("{{.KeyStr}}", {{.KeyFuncGen}})
}
{{- end}}

func Del{{.DataTypeName}}({{.KeyFuncInput}}) error {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.Del(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}})).Err()
}

func Set{{.DataTypeName}}({{.KeyFuncInput}}, info *{{.DataTypeStr}}) error {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.Set(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), xjson.MarshalString(info), 0).Err()
}

func Set{{.DataTypeName}}Expiration({{.KeyFuncInput}}, info *{{.DataTypeStr}}, expiration time.Duration) error {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.Set(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), xjson.MarshalString(info), expiration).Err()
}

func Get{{.DataTypeName}}({{.KeyFuncInput}}) (*{{.DataTypeStr}}, error) {
    return Get{{.DataTypeName}}ByFunc({{.KeyFuncField}}, func() *{{.DataTypeStr}} {
        return &{{.DataTypeStr}}{}
    })
}

func Get{{.DataTypeName}}ByFunc({{.KeyFuncInput}}, newFunc func() *{{.DataTypeStr}}) (info *{{.DataTypeStr}}, err error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    str, err := _client.Get(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}})).Result()
    if err == nil {
        info = newFunc()
        if err = xjson.Unmarshal([]byte(str), info); err == nil {
            return info, nil
        }
    }
    return info, err
}

func Set{{.DataTypeName}}List({{.KeyFuncListInput}}, infos []*{{.DataTypeStr}}) error {
    if len({{.KeyFuncListField}}) != len(infos) {
        return errors.New("Set{{.DataTypeName}}List len error")
    }
    _client := {{.RedisPackage}}.{{.RedisClient}}
    var _list []string
    for i := range {{.KeyFuncListField}} {
        _list = append(_list, gen{{.KeyDataStr}}({{.KeyFuncListField}}[i]))
        _list = append(_list, xjson.MarshalString(infos[i]))
    }
{{- if .PipelineType}}
    _pipeline := _client.Pipeline().(*redis.Pipeline)
    _pipeline.MSet(_client.Context(), _list)
    _, err := _pipeline.Exec(_client.Context())
    return err
{{- else}}
    return _client.MSet(_client.Context(), _list).Err()
{{- end}}    
}

func Get{{.DataTypeName}}List({{.KeyFuncListInput}}) ([]*{{.DataTypeStr}}, error) {
    return Get{{.DataTypeName}}ListByFunc({{.KeyFuncField}}s, func() *{{.DataTypeStr}} {
        return &{{.DataTypeStr}}{}
    })
}

func Get{{.DataTypeName}}ListByFunc({{.KeyFuncListInput}}, newFunc func() *{{.DataTypeStr}}) (list []*{{.DataTypeStr}}, _ error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    var _list []string
    for i := range {{.KeyFuncListField}} {
        _list = append(_list, gen{{.KeyDataStr}}({{.KeyFuncListField}}[i]))
    }
{{- if .PipelineType}}
    _pipeline := _client.Pipeline().(*redis.Pipeline)
    _cmd := _pipeline.MGet(_client.Context(), _list...)
    if _, err := _pipeline.Exec(_client.Context()); err != nil {
        return list, err
    }
    strList, err := _cmd.Result()
{{- else}}
    strList, err := _client.MGet(_client.Context(), _list...).Result()
{{- end}}    
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
            xl.SErrorf("Get{{.DataTypeName}}List Error, Unmarshal String:%s, Error:%s", str.(string), err.Error())
        }
    }
    return list, nil
}

`
