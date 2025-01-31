package db_template

var dataTemplateStr = `// Code generated by TX.
// Template: Data Template

package {{.TypePackage}}
{{ if .DBImports }}
import (
{{- range .DBImports}}
    {{.}}
{{- end}}
){{end}}
{{$structName := CamelizeStr .TableName .SystemName}}{{$lowerName := LowerLeftWord .TableName}}
var {{$lowerName}}Flags = []string{ {{.DataFlagsString}} }

type {{$structName}} struct {
    {{.SystemName}}DirtyFlag
    {{$lowerName}} *db_types.{{.TableName}}
}

func DBNew{{$structName}}() *{{$structName}} {
    return &{{$structName}}{
        {{$lowerName}}: &db_types.{{.TableName}}{},
    }
}

func DBInit{{$structName}}(info *db_types.{{.TableName}}) *{{$structName}} {
    return &{{$structName}}{
        {{$lowerName}}: info,
    }
}

func (data *{{$structName}}) Clean() *{{$structName}} {
    data.ClearDirty(int64(data.{{$lowerName}}.{{.PrimaryKey.Name}}))
    data.UnbindDBLogic(int64(data.{{$lowerName}}.{{.PrimaryKey.Name}}))
    data.{{$lowerName}}.Clean()
    return data
}

func (data *{{$structName}}) Get{{.TableName}}() *db_types.{{.TableName}} {
    return data.{{$lowerName}}
}

func (data *{{$structName}}) Fill{{.TableName}}(info *db_types.{{.TableName}}) *{{$structName }} {
	data.ClearDirty(int64(data.{{$lowerName}}.{{.PrimaryKey.Name}}))
	data.{{$lowerName}} = info
    return data
}
{{- range .FieldMaps}}
{{- if .IsDBTypes}}

func (data *{{$structName}}) Get{{.Name}}() *{{.GoType}} {
    return &data.{{$lowerName}}.{{.Name}}
}
{{- else}}

func (data *{{$structName}}) Get{{.Name}}() {{.GoType}} {
    return data.{{$lowerName}}.{{.Name}}
}
{{- end}}

func (data *{{$structName}}) Set{{.Name}}(v {{.GoType}}) {
    data.{{$lowerName}}.{{.Name}} = v
{{- if .IsDirty }}
    data.setDirtyFlag({{.ColumnIndex}}, int64(data.{{$lowerName}}.{{.PrimaryName}}))
}{{- else}}}
{{- end}}
{{if .IsDBTypes}}
func (data *{{$structName }}) Set{{.Name}}Dirty() {
    data.setDirtyFlag({{.ColumnIndex}}, int64(data.{{$lowerName}}.{{.PrimaryName}}))
}
{{- end}}
{{- end}}

{{- range .UnionFields}}
{{.UnionFormatString }}
{{- end}}

func (data *{{$structName}}) ClearDirtyFlag() {
    data.ClearDirty(int64(data.{{$lowerName}}.{{.PrimaryKey.Name}}))
}

func (data *{{$structName}}) SetDeleteDirty(del bool) {
    data.SetDelDirty(del, int64(data.{{$lowerName}}.{{.PrimaryKey.Name}}))
}

func (data *{{$structName}}) GetDirtyFlags() (list []string) {
    if data.DirtyFlags == 0 {
        return list
    }
    for i := 1; i <= {{.DataFlagsNum}}; i++ {
        if xbitset.GetBit(data.DirtyFlags, i) {
            list = append(list, {{$lowerName}}Flags[i-1])
        }
    }
    return list
}

`
