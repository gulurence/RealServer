package redis_template

var RedisStringHSetTStr = `
{{- if .KeyEntity}}
// {{.KeyDataStr}}  ////////////////////{{.KeyDataStr}}String////////////////////
type {{.KeyDataStr}} struct {
    {{- range .KeyEntity.KeyDataList}}
    {{.}}
    {{- end}}
}

func gen{{.KeyDataStr}}({{.KeyFuncInput}}) string{
    return fmt.Sprintf("{{.KeyStr}}", {{.KeyEntity.KeyGenList}})
}

{{- else}}
//gen{{.KeyDataStr}}  //////////////////////////{{.KeyDataStr}}String//////////////
{{- if .KeyNoneInput}}
func gen{{.KeyDataStr}}() string {
    return "{{.KeyStr}}"
}
{{- else}}
func gen{{.KeyDataStr}}({{.KeyFuncInput}}) string {
    return fmt.Sprintf("{{.KeyStr}}", {{.KeyFuncGen}})
}
{{- end}} {{- end}}

{{- range .RedisFunList}}

func Set{{.DataTypeName}}{{.FunStr}}({{- if not .KeyNoneInput }}{{.KeyFuncInput}}, {{end}}{{.DataFuncInput}}, value string) error {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HSet(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), {{.DataFuncGen}}, value).Err()
}

func Get{{.DataTypeName}}{{.FunStr}}({{- if not .KeyNoneInput }}{{.KeyFuncInput}}, {{end}}{{.DataFuncInput}}) (value string, err error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HGet(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), {{.DataFuncGen}}).Result()
}

func Del{{.DataTypeName}}{{.FunStr}}({{- if not .KeyNoneInput }}{{.KeyFuncInput}}, {{end}}{{.DataFuncInput}}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HDel(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), {{.DataFuncGen}}).Result()
}
{{- end}}

func Set{{.DataTypeName}}Maps({{- if not .KeyNoneInput }}{{.KeyFuncInput}}, {{end}}values map[string]interface{}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HSet(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), values).Result()
}

func Get{{.DataTypeName}}ListByStrings({{- if not .KeyNoneInput }}{{.KeyFuncInput}}, {{end}}_list []string) (list map[string]string, err error) {
    //list = make(map[string]string)
    _client := {{.RedisPackage}}.{{.RedisClient}}
    strMap, err := _client.HMGet(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), _list...).Result()
    if err != nil || len(strMap) != len(_list) {
        return list, err
    }
    for i, str := range strMap {
        if str == nil {
            continue
        }

        value, ok := str.(string)
        if !ok {
            continue
        }
        list[_list[i]] = value
    }
    return list, nil
}

func Get{{.DataTypeName}}List({{.KeyFuncInput}}) (list map[string]string, err error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HGetAll(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}})).Result()
}
`
