package db_template

var union2TemplateStr = `
func (data *{{.StructName}}) GetAll{{.UnionName}}() {{.UnionType}} {
    return data.Get{{.FirstFiled}}() + data.Get{{.SecondFiled}}()
}

func (data *{{.StructName}}) Dec{{.UnionName}}(dec {{.UnionType}}) ({{.UnionType}}, {{.UnionType}}, {{.UnionType}}) {
    if data.Get{{.FirstFiled}}() >= dec {
        data.Set{{.FirstFiled}}(data.Get{{.FirstFiled}}() - dec)
        return dec, 0, data.Get{{.FirstFiled}}() + data.Get{{.SecondFiled}}()
    }
    if data.Get{{.FirstFiled}}()+data.Get{{.SecondFiled}}() >= dec {
        {{.FirstFiled}} := data.Get{{.FirstFiled}}()
        {{.SecondFiled}} := dec - data.Get{{.FirstFiled}}()
        data.Set{{.SecondFiled}}(data.Get{{.SecondFiled}}() - {{.SecondFiled}})
        data.Set{{.FirstFiled}}(0)
        return {{.FirstFiled}}, {{.SecondFiled}}, data.Get{{.FirstFiled}}() + data.Get{{.SecondFiled}}()
    }
    {{.FirstFiled}} := data.Get{{.FirstFiled}}()
    {{.SecondFiled}} := data.Get{{.SecondFiled}}()
    data.Set{{.FirstFiled}}(0)
    data.Set{{.SecondFiled}}(0)
    return {{.FirstFiled}}, {{.SecondFiled}}, 0
}`
