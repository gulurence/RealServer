package proto_type

import (
	"go/ast"
)

type ProtoParams struct {
	SystemName string
	Imports    IImports
	Handlers   IHandlers
}

type ProtoCommand struct {
	HandlerID      int
	IsMessage      bool
	CmdName        string
	ServerProto    string
	ClientProto    string
	ServerFunc     string
	ClientFunc     string
	ProtoName      string
	ServerPackage  string
	ClientPackage  string
	PathName       string
	ServerFImports []string
	ClientFImports []string
}

type ProtoData struct {
	IImports
	FileName       string
	OutFileName    string
	ClientCmd      string
	ServerCmd      string
	ClientMessage  string
	ServerMessage  string
	SystemName     string
	ProtoName      string
	ServerPath     string
	ClientPath     string
	ServerImports  []string
	ClientImports  []string
	RPCCommands    map[string]*ProtoCommand
	NoRspCommands  map[string]*ProtoCommand
	CommCommands   map[string]map[string]*ProtoCommand
	ServerCommands map[string]*ProtoCommand
	ClientCommands map[string]*ProtoCommand
	ProtoMessages  map[string]*ast.TypeSpec
}

type IImports interface {
	ServerHImports() []string
	ServerFImports() []string
	ClientHImports() []string
	ClientFImports() []string
}

type IHandlers interface {
	ExPackageFunc(handlerId int) (string, string)
}
