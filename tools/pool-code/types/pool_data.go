package pool_types

import (
	"reflect"
	"sort"
	"tx-tools/utils"
)

type PoolEntity struct {
	PoolValue     string
	FiledDataType string
	FieldName     string
	FuncName      string
}

type PoolData struct {
	PoolName     string
	PoolValue    string
	LogicPackage string
	Entities     []*PoolEntity
	imports      IImports
}

func NewPoolData(key string, config *TemplateFileConfig, imports IImports) *PoolData {
	_data := &PoolData{
		PoolName:     utils.UpperRightWord(key),
		PoolValue:    utils.LowerRightWord(key),
		LogicPackage: config.NameSpace,
		Entities:     []*PoolEntity{},
		imports:      imports,
	}
	for _, field := range config.DataFields {
		entity := NewPoolEntity(field)
		entity.PoolValue = _data.PoolValue
		_data.Entities = append(_data.Entities, entity)
	}
	return _data
}

func NewPoolEntity(fType reflect.Type) *PoolEntity {
	return &PoolEntity{
		FiledDataType: fType.String(),
		FieldName:     utils.LowerRightWord(fType.Name()),
		FuncName:      utils.UpperRightWord(fType.Name()),
	}
}

func (p *PoolData) LogicImports() (list []string) {
	list = utils.AddImportsXXString(list, `"sync"`)
	list = p.imports.LogicImports(p.PoolName, list)
	sort.Strings(list)
	return list
}

func (p *PoolData) FuncImports() (list []string) {
	list = p.imports.LogicImports(p.PoolName, list)
	sort.Strings(list)
	return list
}
