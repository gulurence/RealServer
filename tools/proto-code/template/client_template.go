package proto_template

var clientHandlerTemplateStr = `// Code generated by TX.
// Template: Proto Server Handler

package {{.ClientPackage}}
{{ if .ClientHImports }}
import (
{{- range .ClientHImports}}
    {{.}}
{{- end}}
{{ if .ClientImports }}
{{- range .ClientImports}}
    {{.}}
{{- end}}
{{end}}
){{end}}

func {{.ServerHandler}}() {
    //-----------------Handler Register------------------
{{- range .ServerCommands}}
    txws.RegisterDialerHandler(uint16({{.ProtoName}}.{{.CmdName}}), {{.ProtoName}}.{{.ServerProto}}{}, {{.ServerPackage}}.{{.ClientFunc}}, "{{.ClientFunc}}" )
{{- end}}

    //-----------------Proto Register------------------
{{- range .ServerCommands}}
    txws.RegisterProto(uint16({{.ProtoName}}.{{.CmdName}}), {{.ProtoName}}.{{.ServerProto}}{})
{{- end}}
{{- range .ClientCommands}}
    txws.RegisterProto(uint16({{.ProtoName}}.{{.CmdName}}), {{.ProtoName}}.{{.ServerProto}}{})
{{- end}}
}
`

var clientFuncTemplateStr = `// Code generated by TX.
// Template: Proto Func Handler
// Generated at {{now}}

package {{.ServerPackage}}
{{ if .ServerFImports }}
import (
{{- range .ServerFImports}}
    {{.}}
{{- end}}
){{end}}

func {{.ClientFunc}}(req *txws.DiRequest) error {
    //xl.SInfow("{{.ServerFunc}}, ", "HostName", req.WSDialer().HostName(), "Request", req.Request().String())
    //dohPlayerImpl := req.WsPlayer().(*doh_player.DohPlayerImpl)
    _ = req.Request().(*{{.ProtoName}}.{{.ServerProto}})
    return nil
}
`