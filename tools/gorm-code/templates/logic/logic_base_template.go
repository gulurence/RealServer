package logic

var logicBaseTemplateStr = `// Code generated by TX.
// Template: Base logic

package {{.LogicPackage}}

import "errors"

type {{.SystemName}}LogicBase struct {
    DBId        	int    
    IsInit      	bool
    UpdateMaps  	map[int64]int64
	StrUpdateMaps 	map[string]int64
}

func (logic *{{.SystemName}}LogicBase) DelUpdateKey(key int64) {
    delete(logic.UpdateMaps, key)
}

func (logic *{{.SystemName}}LogicBase) DelUpdateStrKey(key string) {
    delete(logic.StrUpdateMaps, key)
}

func (logic *{{.SystemName}}LogicBase) AddUpdateKey(key int64) {
    if logic.IsInit {
        logic.UpdateMaps[key] = 1
    }
}

func (logic *{{.SystemName}}LogicBase) AddUpdateStrKey(key string) {
    if logic.IsInit {
        logic.StrUpdateMaps[key] = 1
    }
}

type {{.SystemName}}LogicMgrBase struct {
    PlayerId int64
    SharedId int
    NickName string
    IsInit   bool
}

func (b *{{.SystemName}}LogicMgrBase) CheckValid() error {
	if b.PlayerId <= 0 {
		return errors.New("{{.SystemName}}LogicBase() error, playerId <= 0")
	}
	return nil
}

`