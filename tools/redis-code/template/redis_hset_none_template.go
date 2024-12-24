package redis_template

var RedisNoneHSetTStr = `
//Set{{.DataTypeName}} ///////////////////////{{.KeyDataStr}}/////////////////////////////////////////////////////
func Set{{.DataTypeName}}({{.DataFuncInput}}, info *{{.DataTypeStr}}) error {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HSet(_client.Context(), "{{.KeyStr}}", {{.DataFuncGen}}, xjson.MarshalString(info)).Err()
}

func Get{{.DataTypeName}}({{.DataFuncInput}}) (info *{{.DataTypeStr}}, err error) {
    return Get{{.DataTypeName}}ByFunc({{.DataFuncField}}, func() *{{.DataTypeStr}} {
        return &{{.DataTypeStr}}{}
    })
}

func Get{{.DataTypeName}}ByFunc({{.DataFuncInput}}, newFunc func() *{{.DataTypeStr}}) (info *{{.DataTypeStr}}, err error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    str, err := _client.HGet(_client.Context(), "{{.KeyStr}}", {{.DataFuncGen}}).Result()
    if err == nil {
        info = newFunc()
        if err = xjson.Unmarshal([]byte(str), info); err == nil {
            return info, nil
        }
    }
    return nil, err
}

func Get{{.DataTypeName}}Len() (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HLen(_client.Context(), "{{.KeyStr}}").Result()
}

func Del{{.DataTypeName}}({{.DataFuncInput}}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.HDel(_client.Context(), "{{.KeyStr}}", {{.DataFuncGen}}).Result()
}

func Get{{.DataTypeName}}List() ([]*{{.DataTypeStr}}, error) {
    return Get{{.DataTypeName}}ListByFunc(func() *{{.DataTypeStr}} {
        return &{{.DataTypeStr}}{}
    })
}

func Get{{.DataTypeName}}ListByFunc(newFunc func() *{{.DataTypeStr}}) (list []*{{.DataTypeStr}}, err error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    strList, err := _client.HGetAll(_client.Context(), "{{.KeyStr}}").Result()
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

func Get{{.DataTypeName}}Map() (map[string]*{{.DataTypeStr}}, error) {
    return Get{{.DataTypeName}}MapByFunc(func() *{{.DataTypeStr}} {
        return &{{.DataTypeStr}}{}
    })
}

func Get{{.DataTypeName}}MapByFunc(newFunc func() *{{.DataTypeStr}}) (maps map[string]*{{.DataTypeStr}}, err error) { 
    //maps = make(map[string]*{{.DataTypeStr}})
    _client := {{.RedisPackage}}.{{.RedisClient}}
    strList, err := _client.HGetAll(_client.Context(), "{{.KeyStr}}").Result()
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

func Get{{.DataTypeName}}ByList({{.DataFuncListInput}}, newFunc func() *{{.DataTypeStr}}) ([]*{{.DataTypeStr}}, error) {
    return Get{{.DataTypeName}}ByListFunc({{.DataFuncField}}s, func() *{{.DataTypeStr}} {
        return &{{.DataTypeStr}}{}
    })
}

func Get{{.DataTypeName}}ByListFunc({{.DataFuncListInput}}, newFunc func() *{{.DataTypeStr}}) (list []*{{.DataTypeStr}}, err error) {
    {{.DataFuncListGen}}
    _client := {{.RedisPackage}}.{{.RedisClient}}
    strList, err := _client.HMGet(_client.Context(), "{{.KeyStr}}", {{.DataFuncListField}}...).Result()
    if err != nil {
        return list, err
    }
    for _, _info := range strList {
        if _info == nil {
			continue
		}
        info := newFunc()
        if err = xjson.Unmarshal([]byte(_info.(string)), info); err == nil {
            list = append(list, info)
        } else {
            xl.SErrorf("Get{{.DataTypeName}}ByList Error, Unmarshal String:%s, Error:%s", _info, err.Error())
        }
    }
    return list, err
}

func Get{{.DataTypeName}}ByStrings(_list []string, newFunc func() *{{.DataTypeStr}}) ([]*{{.DataTypeStr}}, error) {
    return Get{{.DataTypeName}}ByStringsFunc(_list, func() *{{.DataTypeStr}} {
        return &{{.DataTypeStr}}{}
    })
}

func Get{{.DataTypeName}}ByStringsFunc(_list []string, newFunc func() *{{.DataTypeStr}}) (list []*{{.DataTypeStr}}, err error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    strList, err := _client.HMGet(_client.Context(), "{{.KeyStr}}", _list...).Result()
    if err != nil {
        return list, err
    }
    for _, _info := range strList {
        if _info == nil {
			continue
		}
        info := newFunc()
        if err = xjson.Unmarshal([]byte(_info.(string)), info); err == nil {
            list = append(list, info)
        } else {
            xl.SErrorf("Get{{.DataTypeName}}ByStrings Error, Unmarshal String:%s, Error:%s", _info, err.Error())
        }
    }
    return list, err
}
`
