package redis_template

var RedisChannelTStr = `
{{- if not .KeyNoneInput }}
func gen{{.KeyDataStr}}({{.KeyFuncInput}}) string {
    return fmt.Sprintf("{{.KeyStr}}", {{.KeyFuncGen}})
}

{{- else}}
//Publish{{.DataTypeName}} ///////////////////////////{{.KeyDataStr}}/////////////////////////////////////////////////////

{{- end}}
{{- if .KeyNoneInput }}

func Publish{{.DataTypeName}}(info *{{.DataTypeStr}}) error {
{{- else}}

func Publish{{.DataTypeName}}({{.KeyFuncInput}}, info *{{.DataTypeStr}}) error {
{{- end}}
    _client := {{.RedisPackage}}.ChannelRClient()
{{- if .KeyNoneInput }}
    return _client.Publish(_client.Context(), "{{.KeyStr}}", xjson.MarshalString(info)).Err()
{{- else}}
    return _client.Publish(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), xjson.MarshalString(info)).Err()
{{- end}}
}

func Subscribe{{.DataTypeName}}({{- if not .KeyNoneInput }}{{.KeyFuncInput}}, {{end}}subFunc func(*{{.DataTypeStr}}) error) {
    Subscribe{{.DataTypeName}}ByFunc({{- if not .KeyNoneInput }}{{.KeyFuncField}}, {{end}}func() *{{.DataTypeStr}} {
        return &{{.DataTypeStr}}{}
    }, subFunc)
}

func Subscribe{{.DataTypeName}}ByFunc({{- if not .KeyNoneInput}}{{.KeyFuncInput}}, {{end}}newFunc func() *{{.DataTypeStr}}, subFunc func(*{{.DataTypeStr}}) error) {
    _client := {{.RedisPackage}}.ChannelRClient()
    subscriber := _client.Subscribe(_client.Context(), {{- if .KeyNoneInput}}"{{.KeyStr}}"{{- else}}gen{{.KeyDataStr}}({{.KeyFuncGen}}){{- end}})
	for {
		if msg, err := subscriber.ReceiveMessage(_client.Context()); err == nil {
            info := newFunc()
            if err = xjson.Unmarshal([]byte(msg.Payload), info); err == nil {
                _ = subFunc(info)
            }
        }
	}
}
`
