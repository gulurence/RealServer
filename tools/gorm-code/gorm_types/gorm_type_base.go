package gorm_types

type IImports interface {
	DBImports(table string) []string
	LogicImports(gdType GormDataType) []string
	DaoImports(table string) []string
	PoolImports(table string) []string
	RedisImports(table string) []string
}

type DefImports struct{}

func (d *DefImports) DBImports(table string) []string {
	return []string{}
}

func (d *DefImports) LogicImports(gdType GormDataType) []string {
	return []string{}
}

func (d *DefImports) DaoImports(table string) []string {
	return []string{}
}

func (d *DefImports) PoolImports(table string) []string {
	return []string{}
}

func (d *DefImports) RedisImports(table string) []string {
	return []string{}
}

type GormTypeBase struct {
	TableName    string
	TypePackage  string
	LogicPackage string
	DaoPackage   string
	PoolPackage  string
	RedisPackage string
	SystemName   string
	SystemPath   string
}

var (
	_systemName string
	_imports    IImports
)

func InitGormTypeDate(systemName string, imports IImports) {
	//    _typePackage = utils.LowerRightWord(systemName) + "_db"
	//    _logicPackage = utils.LowerRightWord(systemName) + "_logic"
	//    _daoPackage = utils.LowerRightWord(systemName) + "_dao"
	//    _poolPackage = utils.LowerRightWord(systemName) + "_pool"
	_systemName = systemName
	_imports = imports
}

func GetSystemName() string {
	return _systemName
}

func GetImports() IImports {
	return _imports
}
