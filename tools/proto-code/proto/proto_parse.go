package proto_type

import (
	"go/ast"
	"go/parser"
	"go/token"
	"sort"

	xl "tx-common/xlogger"
	"tx-common/xutil"
	"tx-tools/utils"
)

func NewProtoData(fileName, cKey string, serverPath, clientPath string) *ProtoData {
	fSet, err := parser.ParseFile(token.NewFileSet(), fileName, nil, 0)
	if err != nil {
		return nil
	}

	protoData := &ProtoData{
		IImports:       protoParams.Imports,
		FileName:       fileName,
		SystemName:     protoParams.SystemName,
		ProtoName:      fSet.Name.Name,
		ServerPath:     serverPath,
		ClientPath:     clientPath,
		ServerImports:  []string{},
		ClientImports:  []string{},
		ServerCommands: make(map[string]*ProtoCommand),
		ClientCommands: make(map[string]*ProtoCommand),
		ProtoMessages:  make(map[string]*ast.TypeSpec),
	}

	if err = protoData.InitCmdMessage(cKey); err != nil {
		xl.SError(err)
		return nil
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
			if ident.Name == protoData.ServerCmd || ident.Name == protoData.ServerMessage || ident.Name == protoData.ClientCmd || ident.Name == protoData.ClientMessage {
				bLit, _ := valueSpec.Values[0].(*ast.BasicLit)
				if bLit.Kind == token.INT {
					cmd := &ProtoCommand{
						HandlerID:      xutil.StringToInt(bLit.Value),
						IsMessage:      false,
						CmdName:        key,
						ProtoName:      protoData.ProtoName,
						ClientProto:    protoData.ClientPackage(),
						ServerFImports: protoData.ServerFImports(),
						ClientFImports: protoData.ClientFImports(),
					}
					if cmd.HandlerID == 0 {
						break
					}
					if ident.Name == protoData.ServerMessage || ident.Name == protoData.ClientMessage {
						cmd.IsMessage = true
					}
					if err = cmd.InitProtoName(); err == nil {
						if ident.Name == protoData.ServerCmd || ident.Name == protoData.ServerMessage {
							protoData.ServerCommands[key] = cmd
							protoData.ClientImports = utils.AddImportsXXString(protoData.ClientImports, `"`+protoData.ClientPath+`/`+cmd.PathName+`"`)
						} else {
							protoData.ClientCommands[key] = cmd
							protoData.ServerImports = utils.AddImportsXXString(protoData.ServerImports, `"`+protoData.ServerPath+`/`+cmd.PathName+`"`)
						}
					} else {
						xl.SError(err.Error())
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

	_ = protoData.checkProtoCommand()

	sort.Strings(protoData.ClientImports)
	sort.Strings(protoData.ServerImports)
	return protoData
}
