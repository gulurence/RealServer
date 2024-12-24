package gorm_types

type GormPoolField struct {
	TypePackage string
	TableName   string
	SystemName  string
}

type GormPoolData struct {
	GormTypeBase
	PoolFields []*GormPoolField
	Imports    IImports
}

func (g *GormPoolData) PoolImports(_type int) (list []string) {
	list = _imports.PoolImports(g.GormTypeBase.TableName)
	list = append(list, `"`+g.GormTypeBase.SystemPath+`/data"`)
	if _type > 0 {
		list = append(list, `"sync"`)
	}
	return list
}
