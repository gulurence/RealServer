package redis_template

var RedisKeyInt64TStr = `
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

func Set{{.DataTypeName}}({{.KeyFuncInput}}, _value int64) error {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.Set(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), _value, 0).Err()
}

func Set{{.DataTypeName}}Expiration({{.KeyFuncInput}}, _value int64, expiration time.Duration) error {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.Set(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), _value, expiration).Err()
}

func Get{{.DataTypeName}}({{.KeyFuncInput}}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.Get(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}})).Int64()
}

func Incr{{.DataTypeName}}({{.KeyFuncInput}}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.Incr(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}})).Result()
}

func Incr{{.DataTypeName}}By({{.KeyFuncInput}}, incr int64) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.IncrBy(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), incr).Result()
}

func Set{{.DataTypeName}}List({{.KeyFuncListInput}}, infos []int64) error {
    if len({{.KeyFuncListField}}) != len(infos) {
        return errors.New("")
    }
    _client := {{.RedisPackage}}.{{.RedisClient}}
    var _maps map[string]interface{}
    for i := range {{.KeyFuncListField}} {
        _maps[gen{{.KeyDataStr}}({{.KeyFuncListField}}[i])] = infos[i]
    }
{{- if .PipelineType}}
    _pipeline := _client.Pipeline().(*redis.Pipeline)
    _pipeline.MSet(_client.Context(), _maps)
    _, err := _pipeline.Exec(_client.Context())
    return err
{{- else}}
    return _client.MSet(_client.Context(), _maps).Err()
{{- end}}    
}

func Get{{.DataTypeName}}List({{.KeyFuncListInput}}) (list []int64, _ error) {
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
        if str != nil {
            list = append(list, xutil.StringToInt64(str.(string)))
        }
    }
    return list, nil
}

`
