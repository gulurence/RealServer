package gorm_types

import (
	"bytes"
	"strings"
	"text/template"

	"tx-tools/utils"
)

type GormFieldData struct {
	GormTags
	GormFields  []GormField
	PrimaryKey  *GormFieldData
	HashKey     *GormFieldData
	DaoPackage  string
	TypePackage string
	PoolPackage string
	MySqlClient string
	TableName   string
	Name        string
	GoType      string
}

func (f *GormFieldData) IsDBTypes() bool {
	types := strings.Split(f.GoType, ".")
	if len(types) == 2 {
		return true
	}
	return false
}

func (f *GormFieldData) IsDirty() bool {
	if len(f.Column) <= 0 {
		return false
	}
	return true
}

type QueryFieldData struct {
	TypePackage string
	MySqlClient string
	PoolPackage string
	QueryKey    string
	TableName   string
	QueryInput  string
	QueryValue  string
}

type UnionFieldData struct {
	UnionParse  *template.Template
	UnionName   string
	UnionKeys   []string
	FieldList   []*GormFieldData
	StructName  string
	UnionType   string
	FirstFiled  string
	SecondFiled string
	ThreeFiled  string
}

func (u *UnionFieldData) UnionFormatString() (str string) {
	if u.UnionParse == nil {
		return ""
	}
	var buf bytes.Buffer
	if err := u.UnionParse.Execute(&buf, u); err != nil {
		return str
	}
	return string(buf.Bytes())
}

func (u *UnionFieldData) InitUnionStruct(structName string) *UnionFieldData {
	u.StructName = structName
	if len(u.UnionKeys) == 2 {
		u.UnionType = u.FieldList[0].GoType
		u.FirstFiled = utils.UpperRightWord(u.UnionKeys[0])
		u.SecondFiled = utils.UpperRightWord(u.UnionKeys[1])
	}
	return u
}
