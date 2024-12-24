package nats_proto

import (
    "go/ast"
    "go/parser"
    "go/token"

    "tx-common/xutil"
)

func NewNatsProtoData(fileName string) *NatsProtoData {
    fSet, err := parser.ParseFile(token.NewFileSet(), fileName, nil, 0)
    if err != nil {
        return nil
    }

    protoData := &NatsProtoData{
        NatsImports:   protoParams.Imports,
        SystemName:    protoParams.SystemName,
        Package:       fSet.Name.Name,
        NatsCommands:  make(map[string]*NatsProtoCommand),
        ProtoMessages: make(map[string]*ast.TypeSpec),
    }

    for key, object := range fSet.Scope.Objects {
        switch object.Kind {
        case ast.Con:
            valueSpec, ok := object.Decl.(*ast.ValueSpec)
            if !ok {
                break
            }
            ident, ok := valueSpec.Type.(*ast.Ident)
            if !ok {
                break
            }
            if ident == nil || valueSpec == nil {
                break
            }
            if ident.Name == "NatsHandlerCMD" {
                bLit,_ := valueSpec.Values[0].(*ast.BasicLit)
                if bLit.Kind == token.INT {
                    cmd := &NatsProtoCommand{
                        CmdId:   xutil.StringToInt(bLit.Value),
                        CmdName: key,
                        Package: protoData.Package,
                    }
                    _ = cmd.InitProtoName()
                    if cmd.CmdId > 0 {
                        protoData.NatsCommands[key] = cmd
                    }
                }
            }
        case ast.Typ:
            typeSpec, ok := object.Decl.(*ast.TypeSpec)
            if !ok {
                break
            }
            structType, ok := typeSpec.Type.(*ast.StructType)
            if !ok {
                break
            }
            if typeSpec == nil || structType == nil {
                break
            }
            protoData.ProtoMessages[key] = typeSpec
        }
    }

    return protoData
}
