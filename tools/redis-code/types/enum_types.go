package redis_ttypes

type RedisClientType int32

const (
    ServerRedisClient   = RedisClientType(0)
    GlobalRedisClient   = ServerRedisClient + 1
    PipelineRedisClient = GlobalRedisClient + 1
    ChannelRedisClient  = PipelineRedisClient + 1
)

func (rct RedisClientType) String() (str string) {
    switch rct {
    case GlobalRedisClient:
        str = "GlobalRClient()"
    case ServerRedisClient:
        str = "ServerRClient()"
    case PipelineRedisClient:
        str = "PipelineRClient()"
    case ChannelRedisClient:
        str = "ChannelRedisClient()"
    default:
        str = "GlobalRClient()"
    }
    return str
}

type RedisDataType int32

const (
    RedisDataTypFile         = RedisDataType(0)
    RedisDataTypeKey         = RedisDataTypFile + 1
    RedisDataTypeList        = RedisDataTypeKey + 1
    RedisDataTypeHSet        = RedisDataTypeList + 1
    RedisDataTypeZSet        = RedisDataTypeHSet + 1
    RedisDataTypeKeyZSet     = RedisDataTypeZSet + 1
    RedisDataTypeStream      = RedisDataTypeKeyZSet + 1
    RedisDataTypeGeo         = RedisDataTypeStream + 1
    RedisDataTypeInt64HSet   = RedisDataTypeGeo + 1
    RedisDataTypeFloat64HSet = RedisDataTypeInt64HSet + 1
    RedisDataTypeNoneHSet    = RedisDataTypeFloat64HSet + 1
    RedisDataTypeKeyInt64    = RedisDataTypeNoneHSet + 1
    RedisDataTypeChannel     = RedisDataTypeKeyInt64 + 1
    RedisDataTypeStringHSet  = RedisDataTypeChannel + 1
    RedisDataTypeMax         = RedisDataTypeStringHSet + 1
)

func (rdt RedisDataType) String() (str string) {
    switch rdt {
    case RedisDataTypFile:
        str = "_None"
    case RedisDataTypeKey:
        str = "_Key"
    case RedisDataTypeList:
        str = "_List"
    case RedisDataTypeHSet:
        str = "_HSet"
    case RedisDataTypeInt64HSet:
        str = "_IHSet"
    case RedisDataTypeFloat64HSet:
        str = "_FHSet"
    case RedisDataTypeNoneHSet:
        str = "_NHSet"
    case RedisDataTypeZSet:
        str = "_ZSet"
    case RedisDataTypeKeyZSet:
        str = "_KZSet"
    case RedisDataTypeStream:
        str = "_Stream"
    case RedisDataTypeGeo:
        str = "_Geo"
    case RedisDataTypeKeyInt64:
        str = "_KInt"
    case RedisDataTypeChannel:
        str = "_Channel"
    case RedisDataTypeStringHSet:
        str = "_SHSet"
    default:
    }
    return str
}

type RedisKeyType int32

const (
    RedisKeyTypeNone     = RedisKeyType(0)
    RedisKeyTypeUuid     = RedisKeyTypeNone + 1
    RedisKeyTypePlayerId = RedisKeyTypeUuid + 1
    RedisKeyTypeServerId = RedisKeyTypePlayerId + 1
    RedisKeyTypeGuildId  = RedisKeyTypeServerId + 1
    RedisKeyTypeMax      = RedisKeyTypeGuildId + 1
)

func (rdt RedisKeyType) String() (str string) {
    switch rdt {
    case RedisKeyTypeNone:
        str = "None"
    case RedisKeyTypeUuid:
        str = "Uuid"
    case RedisKeyTypePlayerId:
        str = "PlayerId"
    case RedisKeyTypeServerId:
        str = "ServerId"
    case RedisKeyTypeGuildId:
        str = "GuildId"
    default:
    }
    return str
}

func (rdt RedisKeyType) KeyString() (str string) {
    switch rdt {
    case RedisKeyTypeNone:
        str = ""
    case RedisKeyTypeUuid:
        str = "uuid"
    case RedisKeyTypePlayerId:
        str = "playerId"
    case RedisKeyTypeServerId:
        str = "serverId"
    case RedisKeyTypeGuildId:
        str = "guildId"
    default:
    }
    return str
}

func (rdt RedisKeyType) KeyTypeString() (str string) {
    switch rdt {
    case RedisKeyTypeNone:
        str = ""
    case RedisKeyTypeUuid:
        str = "string"
    case RedisKeyTypePlayerId:
        str = "int64"
    case RedisKeyTypeServerId:
        str = "int32"
    case RedisKeyTypeGuildId:
        str = "int64"
    default:
    }
    return str
}
