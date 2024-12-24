package redis_template

var RedisInt64HSetTStr = `
{{- if .KeyEntity }}
//{{.KeyDataStr}} ///////////////////////{{.KeyDataStr}}Int64/////////////////////////////////////////////////////
type {{.KeyDataStr}} struct{
    {{- range .KeyEntity.KeyDataList}}
    {{.}}
    {{- end}}
}

func gen{{.KeyDataStr}}({{.KeyFuncInput}}) string {
    return fmt.Sprintf("{{.KeyStr}}", {{.KeyEntity.KeyGenList}})
}
{{- else }}
//gen{{.KeyDataStr}} ///////////////////////{{.KeyDataStr}}Int64/////////////////////////////////////////////////////
{{- if .KeyNoneInput }}
func gen{{.KeyDataStr}}() string {
    return "{{.KeyStr}}"
}
{{- else}}
func gen{{.KeyDataStr}}({{.KeyFuncInput}}) string {
    return fmt.Sprintf("{{.KeyStr}}", {{.KeyFuncGen}})
}
{{- end}} {{- end}}

{{- range .RedisFunList}}

func Set{{.DataTypeName}}{{.FunStr}}({{- if not .KeyNoneInput }}{{.KeyFuncInput}}, {{end}}{{.DataFuncInput}}, value int64) error {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HSet(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), {{.DataFuncGen}}, value).Err()
}

func Get{{.DataTypeName}}{{.FunStr}}({{- if not .KeyNoneInput }}{{.KeyFuncInput}}, {{end}}{{.DataFuncInput}}) (value int64, err error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HGet(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), {{.DataFuncGen}}).Int64()
}

func Del{{.DataTypeName}}{{.FunStr}}({{- if not .KeyNoneInput }}{{.KeyFuncInput}}, {{end}}{{.DataFuncInput}}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HDel(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), {{.DataFuncGen}}).Result()
}

func Del{{.DataTypeName}}{{.FunStr}}ListByStrings({{- if not .KeyNoneInput }}{{.KeyFuncInput}}, {{end}}_list []string) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HDel(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), _list...).Result()
}

func Incr{{.DataTypeName}}{{.FunStr}}By({{- if not .KeyNoneInput }}{{.KeyFuncInput}}, {{end}}{{.DataFuncInput}}, incr int64) (value int64, err error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HIncrBy(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), {{.DataFuncGen}}, incr).Result()
}
{{- end}}

func Set{{.DataTypeName}}Maps({{- if not .KeyNoneInput }}{{.KeyFuncInput}}, {{end}}values map[string]interface{}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HSet(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), values).Result()
}

func Get{{.DataTypeName}}ListByStrings({{- if not .KeyNoneInput }}{{.KeyFuncInput}}, {{end}}_list []string) (list map[string]int64, err error) {
    list = make(map[string]int64)
    _client := {{.RedisPackage}}.{{.RedisClient}}
    strMap, err := _client.HMGet(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), _list...).Result()
    if err != nil || len(strMap) != len(_list) {
        return list, err
    }
    for i, str := range strMap {
        if str == nil {
            continue
        }
        list[_list[i]] = xutil.StringToInt64(str.(string))
    }
    return list, nil
}

func Get{{.DataTypeName}}List({{.KeyFuncInput}}) (list map[string]int64, err error) {
    list = make(map[string]int64)
    _client := {{.RedisPackage}}.{{.RedisClient}}
    strMap, err := _client.HGetAll(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}})).Result()
    for i, str := range strMap {
        list[i] = xutil.StringToInt64(str)
    }
    return list, nil
}
`
