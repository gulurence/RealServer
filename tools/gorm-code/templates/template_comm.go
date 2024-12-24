package templates

import (
	"bytes"
	"text/template"
	"time"
	"tx-tools/utils"

	"tx-tools/gorm-code/gorm_types"
)

func ParseGormData(parse *template.Template, data *gorm_types.GormTypeData) ([]byte, error) {
	var buf bytes.Buffer
	if err := parse.Execute(&buf, data); err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

func ParseGormPoolData(parse *template.Template, data *gorm_types.GormPoolData) ([]byte, error) {
	var buf bytes.Buffer
	if err := parse.Execute(&buf, data); err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

func InitTemplate(key, str string) (*template.Template, error) {
	return template.New(key).Funcs(template.FuncMap{
		"CamelizeStr":    utils.CamelizeStr,
		"LowerLeftWord":  utils.LowerLeftWord,
		"UpperRightWord": utils.UpperRightWord,
		"now": func() string {
			return time.Now().Format("2006-01-02 Z07:00")
		},
	}).Parse(str)
}
