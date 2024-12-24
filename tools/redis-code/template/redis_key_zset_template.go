package redis_template

var RedisKeyZSetTStr = `
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

func {{.DataTypeName}}ZAdd({{.KeyFuncInput}}, z redis.Z) error {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZAdd(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), z).Err()
}

func {{.DataTypeName}}ZAddNX({{.KeyFuncInput}}, z redis.Z) error {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZAddNX(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), z).Err()
}

func {{.DataTypeName}}ZAddXX({{.KeyFuncInput}}, z redis.Z) error {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZAddXX(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), z).Err()
}

func {{.DataTypeName}}ZAdds({{.KeyFuncInput}}, list []redis.Z) error {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZAdd(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), list...).Err()
}

func {{.DataTypeName}}ZIncrBy({{.KeyFuncInput}}, increment float64, member string) (float64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZIncrBy(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), increment, member).Result()
}

func {{.DataTypeName}}ZIncr({{.KeyFuncInput}}, z redis.Z) (float64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZAddArgsIncr(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), redis.ZAddArgs{
        Members: []redis.Z{z},
    }).Result()
}

func {{.DataTypeName}}ZIncrNX({{.KeyFuncInput}}, z redis.Z) (float64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZAddArgsIncr(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), redis.ZAddArgs{
        NX:      true,
        Members: []redis.Z{z},
    }).Result()
}

func {{.DataTypeName}}ZIncrXX({{.KeyFuncInput}}, z redis.Z) (float64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZAddArgsIncr(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), redis.ZAddArgs{
        XX:      true,
        Members: []redis.Z{z},
    }).Result()
}

func {{.DataTypeName}}ZScore({{.KeyFuncInput}}, {{.DataFuncInput}}) (float64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZScore(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), {{.DataFuncGen}}).Result()
}

func {{.DataTypeName}}ZCount({{.KeyFuncInput}}, min, max string) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZCount(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), min, max).Result()
}

func {{.DataTypeName}}ZCard({{.KeyFuncInput}}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZCard(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}})).Result()
}

func {{.DataTypeName}}ZRank({{.KeyFuncInput}}, {{.DataFuncInput}}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRank(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), {{.DataFuncGen}}).Result()
}

func {{.DataTypeName}}ZRange({{.KeyFuncInput}}, start, stop int64) ([]string, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRange(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), start, stop).Result()
}

func {{.DataTypeName}}ZRangeWithScore({{.KeyFuncInput}}, start, stop int64) ([]redis.Z, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRangeWithScores(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), start, stop).Result()
}

func {{.DataTypeName}}ZRangeByScore({{.KeyFuncInput}}, min, max string, count int64) ([]string, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRangeByScore(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), &redis.ZRangeBy{
        Min:    min,
        Max:    max,
        Offset: 0,
        Count:  count,
    }).Result()
}

func {{.DataTypeName}}ZRangeByScoreWithScores({{.KeyFuncInput}}, min, max string, count int64) ([]redis.Z, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRangeByScoreWithScores(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), &redis.ZRangeBy{
        Min:    min,
        Max:    max,
        Offset: 0,
        Count:  count,
    }).Result()
}

func {{.DataTypeName}}ZRevRank({{.KeyFuncInput}}, {{.DataFuncInput}}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRevRank(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), {{.DataFuncGen}}).Result()
}

func {{.DataTypeName}}ZRevRange({{.KeyFuncInput}}, start, stop int64) ([]string, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRevRange(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), start, stop).Result()
}

func {{.DataTypeName}}ZRevRangeWithScore({{.KeyFuncInput}}, start, stop int64) ([]redis.Z, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRevRangeWithScores(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), start, stop).Result()
}

func {{.DataTypeName}}ZRevRangeByScore({{.KeyFuncInput}}, min, max string, count int64) ([]string, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRevRangeByScore(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), &redis.ZRangeBy{
        Min:    min,
        Max:    max,
        Offset: 0,
        Count:  count,
    }).Result()
}

func {{.DataTypeName}}ZRevRangeByScoreWithScores({{.KeyFuncInput}}, min, max string, count int64) ([]redis.Z, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRevRangeByScoreWithScores(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), &redis.ZRangeBy{
        Min:    min,
        Max:    max,
        Offset: 0,
        Count:  count,
    }).Result()
}

func {{.DataTypeName}}ZRem({{.KeyFuncInput}}, {{.DataFuncInput}}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRem(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), {{.DataFuncGen}}).Result()
}

func {{.DataTypeName}}ZRemStr({{.KeyFuncInput}}, value string) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRem(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), value).Result()
}

func {{.DataTypeName}}ZRems({{.KeyFuncInput}}, {{.DataFuncListInput}}) (int64, error) {
    {{.DataFuncListGen}}
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRem(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), {{.DataFuncListField}}...).Result()
}

func {{.DataTypeName}}ZRemByScore({{.KeyFuncInput}}, min, max string) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRemRangeByScore(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), min, max).Result()
}

func {{.DataTypeName}}ZRemByRank({{.KeyFuncInput}}, start, stop int64) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRemRangeByRank(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), start, stop).Result()
}

func {{.DataTypeName}}ZPopMin({{.KeyFuncInput}}, count int64) ([]redis.Z, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZPopMin(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), count).Result()
}

func {{.DataTypeName}}ZPopMax({{.KeyFuncInput}}, count int64) ([]redis.Z, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZPopMax(_client.Context(), gen{{.KeyDataStr}}({{.KeyFuncGen}}), count).Result()
}
`
