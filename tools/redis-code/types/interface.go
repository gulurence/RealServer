package redis_ttypes

type IImports interface {
	LogicImports(rdFile string, list []string, dataType RedisDataType) []string
}

type RedisTypeBase struct {
	TableName    string
	RedisPackage string
	TypePackage  string
	LogicPackage string
	SystemName   string
}

var (
	_systemName string
	_keyName    string
	_imports    IImports
)

func InitRedisDate(systemName string, keyName string, imports IImports) {
	_systemName = systemName
	_keyName = keyName
	_imports = imports
}
