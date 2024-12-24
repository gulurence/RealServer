package redis_template

var RedisZSetTStr = `
//{{.DataTypeName}}ZAdd ///////////////////////{{.KeyDataStr}}/////////////////////////////////////////////////////

func {{.DataTypeName}}ZAdd(z redis.Z) error {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZAdd(_client.Context(), "{{.KeyStr}}", z).Err()
}

func {{.DataTypeName}}ZAddNX(z redis.Z) error {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZAddNX(_client.Context(), "{{.KeyStr}}", z).Err()
}

func {{.DataTypeName}}ZAddXX(z redis.Z) error {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZAddXX(_client.Context(), "{{.KeyStr}}", z).Err()
}

func {{.DataTypeName}}ZAdds(list []redis.Z) error {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZAdd(_client.Context(), "{{.KeyStr}}", list...).Err()
}

func {{.DataTypeName}}ZIncrBy(increment float64, member string) (float64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZIncrBy(_client.Context(), "{{.KeyStr}}", increment, member).Result()
}

func {{.DataTypeName}}ZIncr(z redis.Z) (float64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZAddArgsIncr(_client.Context(), "{{.KeyStr}}", redis.ZAddArgs{
        Members: []redis.Z{z},
    }).Result()
}

func {{.DataTypeName}}ZIncrNX(z redis.Z) (float64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZAddArgsIncr(_client.Context(), "{{.KeyStr}}", redis.ZAddArgs{
        NX:      true,
        Members: []redis.Z{z},
    }).Result()
}

func {{.DataTypeName}}ZIncrXX(z redis.Z) (float64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZAddArgsIncr(_client.Context(), "{{.KeyStr}}", redis.ZAddArgs{
        XX:      true,
        Members: []redis.Z{z},
    }).Result()
}

func {{.DataTypeName}}ZScore({{.DataFuncInput}}) (float64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZScore(_client.Context(), "{{.KeyStr}}", {{.DataFuncGen}}).Result()
}

func {{.DataTypeName}}ZCount(min, max string) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZCount(_client.Context(), "{{.KeyStr}}", min, max).Result()
}

func {{.DataTypeName}}ZCard() (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZCard(_client.Context(), "{{.KeyStr}}").Result()
}

func {{.DataTypeName}}ZRank({{.DataFuncInput}}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRank(_client.Context(), "{{.KeyStr}}", {{.DataFuncGen}}).Result()
}

func {{.DataTypeName}}ZRange(start, stop int64) ([]string, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRange(_client.Context(), "{{.KeyStr}}", start, stop).Result()
}

func {{.DataTypeName}}ZRangeWithScore(start, stop int64) ([]redis.Z, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRangeWithScores(_client.Context(), "{{.KeyStr}}", start, stop).Result()
}

func {{.DataTypeName}}ZRangeByScore(min, max string, count int64) ([]string, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRangeByScore(_client.Context(), "{{.KeyStr}}", &redis.ZRangeBy{
        Min:    min,
        Max:    max,
        Offset: 0,
        Count:  count,
    }).Result()
}

func {{.DataTypeName}}ZRangeByScoreWithScores(min, max string, count int64) ([]redis.Z, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRangeByScoreWithScores(_client.Context(), "{{.KeyStr}}", &redis.ZRangeBy{
        Min:    min,
        Max:    max,
        Offset: 0,
        Count:  count,
    }).Result()
}

func {{.DataTypeName}}ZRevRank({{.DataFuncInput}}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRevRank(_client.Context(), "{{.KeyStr}}", {{.DataFuncGen}}).Result()
}

func {{.DataTypeName}}ZRevRange(start, stop int64) ([]string, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRevRange(_client.Context(), "{{.KeyStr}}", start, stop).Result()
}

func {{.DataTypeName}}ZRevRangeWithScore(start, stop int64) ([]redis.Z, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRevRangeWithScores(_client.Context(), "{{.KeyStr}}", start, stop).Result()
}

func {{.DataTypeName}}ZRevRangeByScore(min, max string, count int64) ([]string, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRevRangeByScore(_client.Context(), "{{.KeyStr}}", &redis.ZRangeBy{
        Min:    min,
        Max:    max,
        Offset: 0,
        Count:  count,
    }).Result()
}

func {{.DataTypeName}}ZRevRangeByScoreWithScores(min, max string, count int64) ([]redis.Z, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRevRangeByScoreWithScores(_client.Context(), "{{.KeyStr}}", &redis.ZRangeBy{
        Min:    min,
        Max:    max,
        Offset: 0,
        Count:  count,
    }).Result()
}

func {{.DataTypeName}}ZRem({{.DataFuncInput}}) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRem(_client.Context(), "{{.KeyStr}}", {{.DataFuncGen}}).Result()
}

func {{.DataTypeName}}ZRemStr(value string) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRem(_client.Context(), "{{.KeyStr}}", value).Result()
}

func {{.DataTypeName}}ZRems({{.DataFuncListInput}}) (int64, error) {
    {{.DataFuncListGen}}
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRem(_client.Context(), "{{.KeyStr}}", {{.DataFuncListField}}).Result()
}

func {{.DataTypeName}}ZRemByScore(min, max string) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRemRangeByScore(_client.Context(), "{{.KeyStr}}", min, max).Result()
}

func {{.DataTypeName}}ZRemByRank(start, stop int64) (int64, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZRemRangeByRank(_client.Context(), "{{.KeyStr}}", start, stop).Result()
}

func {{.DataTypeName}}ZPopMin(count int64) ([]redis.Z, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZPopMin(_client.Context(), "{{.KeyStr}}", count).Result()
}

func {{.DataTypeName}}ZPopMax(count int64) ([]redis.Z, error) {
    _client := {{.RedisPackage}}.{{.RedisClient}}
    return _client.ZPopMax(_client.Context(), "{{.KeyStr}}", count).Result()
}
`
