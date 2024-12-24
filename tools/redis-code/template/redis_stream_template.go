package redis_template

var RedisStreamTStr = `
type Stream{{.DataTypeName}} struct {
    StreamKey   string
    Info        *{{.DataTypeStr}}
}

{{- if .KeyEntity }}

//{{.KeyDataStr}} ///////////////////////{{.KeyDataStr}}/////////////////////////////////////////////////////
type {{.KeyDataStr}} struct{
    {{- range .KeyEntity.KeyDataList}}
    {{.}}
    {{- end}}
}

{{- end}}

{{- if .KeyFuncInput }}
//gen{{.KeyDataStr}} ///////////////////////{{.KeyDataStr}}/////////////////////////////////////////////////////
func gen{{.KeyDataStr}}({{.KeyFuncInput}}) string {
    return fmt.Sprintf("{{.KeyStr}}", {{- if .KeyEntity }}{{.KeyEntity.KeyGenList}}{{- else}}{{.KeyFuncGen}}{{- end}})
}

{{- else }}
//gen{{.KeyDataStr}} ///////////////////////{{.KeyDataStr}}/////////////////////////////////////////////////////
func gen{{.KeyDataStr}}() string {
    return fmt.Sprintf("{{.KeyStr}}")
}
{{- end}}

{{- if .KeyFuncInput }}

func Add{{.DataTypeName}}({{.KeyFuncInput}}, info *{{.DataTypeStr}}) (string, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.XAdd(_client.Context(), &redis.XAddArgs{
        Stream:       gen{{.KeyDataStr}}({{.KeyFuncGen}}),
        MaxLen:       {{.MaxCount}},
        Values:       map[string]interface{}{"{{.DataTypeName}}": xjson.MarshalString(info)},
    }).Result()
}

func Add{{.DataTypeName}}BySKey({{.KeyFuncInput}}, sKey string, info *{{.DataTypeStr}}) (string, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.XAdd(_client.Context(), &redis.XAddArgs{
        Stream:       gen{{.KeyDataStr}}({{.KeyFuncGen}}),
        MaxLen:       {{.MaxCount}},
        ID:           sKey,
        Values:       map[string]interface{}{"{{.DataTypeName}}": xjson.MarshalString(info)},
    }).Result()
}

func Add{{.DataTypeName}}Maps({{.KeyFuncInput}}, maps map[string]*{{.DataTypeStr}}) (string, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    _values := make(map[string]interface{})
    for key, info := range maps {
        _values[key] = xjson.MarshalString(info)
    }
    return _client.XAdd(_client.Context(), &redis.XAddArgs{
        Stream:       gen{{.KeyDataStr}}({{.KeyFuncGen}}),
        MaxLen:       {{.MaxCount}},
        Values:       _values,
    }).Result()
}

func Add{{.DataTypeName}}MapsByKey({{.KeyFuncInput}}, sKey string, maps map[string]*{{.DataTypeStr}}) (string, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    _values := make(map[string]interface{})
    for key, info := range maps {
        _values[key] = xjson.MarshalString(info)
    }
    return _client.XAdd(_client.Context(), &redis.XAddArgs{
        Stream:       gen{{.KeyDataStr}}({{.KeyFuncGen}}),
        MaxLen:       {{.MaxCount}},
        ID:           sKey,
        Values:       _values,
    }).Result()
}

func Del{{.DataTypeName}}({{.KeyFuncInput}}, streamKey string) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.XDel(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), streamKey).Result()
}

func Del{{.DataTypeName}}List({{.KeyFuncInput}}, streamKeys []string) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.XDel(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), streamKeys...).Result()
}

func Clean{{.DataTypeName}}({{.KeyFuncInput}}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.Del(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}})).Result()
}

func Read{{.DataTypeName}}List({{.KeyFuncInput}}, streamKey string, count int64) ([]*Stream{{.DataTypeName}}, error) {
    return Read{{.DataTypeName}}ListByFunc({{.KeyFuncField}}, streamKey, count, func() *{{.DataTypeStr}} {
        return &{{.DataTypeStr}}{}
    })
}

func Read{{.DataTypeName}}ListByFunc({{.KeyFuncInput}}, streamKey string, count int64, newFunc func() *{{.DataTypeStr}}) (list []*Stream{{.DataTypeName}}, err error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    streams, err := _client.XRead(_client.Context(), &redis.XReadArgs{
        Streams: []string{gen{{.KeyDataStr}}({{.KeyFuncGen}}), streamKey},
        Count:   count,
        Block:   -1,
    }).Result()

    if err != nil {
        return list, err
    }

    //var list []*Stream{{.DataTypeName}}
    for _, stream := range streams {
        for _, message := range stream.Messages {
            for _, value := range message.Values {
                if value == nil {
                    continue
                }
                info := newFunc()
                if err = xjson.Unmarshal([]byte(value.(string)), info); err == nil {
                    list = append(list, &Stream{{.DataTypeName}}{
                        StreamKey: message.ID,
                        Info:      info,
                    })
                }
            }
        }
    }
    return list, nil
}

func Read{{.DataTypeName}}XMessages({{.KeyFuncInput}}, streamKey string, count int64) (list []redis.XMessage, err error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    streams, err := _client.XRead(_client.Context(), &redis.XReadArgs{
        Streams: []string{gen{{.KeyDataStr}}({{.KeyFuncGen}}), streamKey},
        Count:   count,
        Block:   -1,
    }).Result()

    if err != nil {
        return list, err
    }

    //var list []redis.XMessage
    for _, stream := range streams {
        for _, message := range stream.Messages {
            list = append(list, message)
        }
    }
    return list, nil
}

func Read{{.DataTypeName}}Count({{.KeyFuncInput}}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.XLen(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}})).Result()
}

func Read{{.DataTypeName}}Range({{.KeyFuncInput}}, start, stop string) ([]redis.XMessage, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.XRange(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), start, stop).Result()
}

func Read{{.DataTypeName}}RangeN({{.KeyFuncInput}}, start, stop string, count int64) ([]redis.XMessage, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.XRangeN(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), start, stop, count).Result()
}

func Read{{.DataTypeName}}RevRange({{.KeyFuncInput}}, start, stop string) ([]redis.XMessage, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.XRevRange(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), start, stop).Result()
}

func Read{{.DataTypeName}}RevRangeN({{.KeyFuncInput}}, start, stop string, count int64) ([]redis.XMessage, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.XRevRangeN(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), start, stop, count).Result()
}

{{- else }}
func Add{{.DataTypeName}}(info *{{.DataTypeStr}}) (string, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.XAdd(_client.Context(), &redis.XAddArgs{
        Stream:       gen{{.KeyDataStr}}({{.KeyFuncGen}}),
        MaxLen:       {{.MaxCount}},
        Values:       map[string]interface{}{"{{.DataTypeName}}": xjson.MarshalString(info)},
    }).Result()
}

func Add{{.DataTypeName}}BySKey(sKey string, info *{{.DataTypeStr}}) (string, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.XAdd(_client.Context(), &redis.XAddArgs{
        Stream:       gen{{.KeyDataStr}}(),
        MaxLen:       {{.MaxCount}},
        ID:           sKey,
        Values:       map[string]interface{}{"{{.DataTypeName}}": xjson.MarshalString(info)},
    }).Result()
}

func Add{{.DataTypeName}}Maps(maps map[string]*{{.DataTypeStr}}) (string, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    _values := make(map[string]interface{})
    for key, info := range maps {
        _values[key] = xjson.MarshalString(info)
    }
    return _client.XAdd(_client.Context(), &redis.XAddArgs{
        Stream:       gen{{.KeyDataStr}}(),
        MaxLen:       {{.MaxCount}},
        MaxLenApprox: {{.MaxCount}},
        Values:       _values,
    }).Result()
}

func Add{{.DataTypeName}}MapsByKey(sKey string, maps map[string]*{{.DataTypeStr}}) (string, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    _values := make(map[string]interface{})
    for key, info := range maps {
        _values[key] = xjson.MarshalString(info)
    }
    return _client.XAdd(_client.Context(), &redis.XAddArgs{
        Stream:       gen{{.KeyDataStr}}(),
        MaxLen:       {{.MaxCount}},
        MaxLenApprox: {{.MaxCount}},
        ID:           sKey,
        Values:       _values,
    }).Result()
}

func Del{{.DataTypeName}}(streamKey string) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.XDel(_client.Context(), gen{{.KeyDataStr}}(), streamKey).Result()
}

func Del{{.DataTypeName}}List(streamKeys []string) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.XDel(_client.Context(), gen{{.KeyDataStr}}(), streamKeys...).Result()
}

func Clean{{.DataTypeName}}() (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.Del(_client.Context(), gen{{.KeyDataStr}}()).Result()
}

func Read{{.DataTypeName}}List(streamKey string, count int64) ([]*Stream{{.DataTypeName}}, error) {
    return Read{{.DataTypeName}}ListByFunc(streamKey, count, func() *{{.DataTypeStr}} {
        return &{{.DataTypeStr}}{}
    })
}

func Read{{.DataTypeName}}ListByFunc(streamKey string, count int64, newFunc func() *{{.DataTypeStr}}) (list []*Stream{{.DataTypeName}}, err error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    streams, err := _client.XRead(_client.Context(), &redis.XReadArgs{
        Streams: []string{gen{{.KeyDataStr}}(), streamKey},
        Count:   count,
        Block:   -1,
    }).Result()

    if err != nil {
        return list, err
    }

    //var list []*Stream{{.DataTypeName}}
    for _, stream := range streams {
        for _, message := range stream.Messages {
            for _, value := range message.Values {
                if value == nil {
                    continue
                }
                info := newFunc()
                if err = xjson.Unmarshal([]byte(value.(string)), info); err == nil {
                    list = append(list, &Stream{{.DataTypeName}}{
                        StreamKey: message.ID,
                        Info:      info,
                    })
                }
            }
        }
    }
    return list, nil
}

func Read{{.DataTypeName}}XMessages(streamKey string, count int64) (list []redis.XMessage, err error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    streams, err := _client.XRead(_client.Context(), &redis.XReadArgs{
        Streams: []string{gen{{.KeyDataStr}}(), streamKey},
        Count:   count,
        Block:   -1,
    }).Result()

    if err != nil {
        return list, err
    }

    //var list []redis.XMessage
    for _, stream := range streams {
        for _, message := range stream.Messages {
            list = append(list, message)
        }
    }
    return list, nil
}

func Read{{.DataTypeName}}Count() (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.XLen(_client.Context(), gen{{.KeyDataStr}}()).Result()
}

func Read{{.DataTypeName}}Range(start, stop string) ([]redis.XMessage, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.XRange(_client.Context(), gen{{.KeyDataStr}}(), start, stop).Result()
}

func Read{{.DataTypeName}}RangeN(start, stop string, count int64) ([]redis.XMessage, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.XRangeN(_client.Context(), gen{{.KeyDataStr}}(), start, stop, count).Result()
}

func Read{{.DataTypeName}}RevRange(start, stop string) ([]redis.XMessage, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.XRevRange(_client.Context(), gen{{.KeyDataStr}}(), start, stop).Result()
}

func Read{{.DataTypeName}}RevRangeN(start, stop string, count int64) ([]redis.XMessage, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.XRevRangeN(_client.Context(), gen{{.KeyDataStr}}(), start, stop, count).Result()
}
{{- end}}
`
