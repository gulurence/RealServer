package proto_type

import (
	"errors"
	"strings"
	"unicode"
)

var protoParams *ProtoParams

func InitProtoParams(system string, imports IImports, handlers IHandlers) {
	protoParams = &ProtoParams{
		SystemName: system,
		Imports:    imports,
		Handlers:   handlers,
	}
}

func (pd *ProtoData) ClientPackage() string {
	return strings.ToLower(pd.ServerCmd[0:2] + "_handler")
}

func (pd *ProtoData) ServerPackage() string {
	return strings.ToLower(pd.ClientCmd[0:2] + "_handler")
}

func (pd *ProtoData) ServerHandler() string {
	return pd.ClientCmd[0:2] + "HandlerInit"
}

func (pd *ProtoData) InitCmdMessage(key string) error {
	if len(key) != 2 {
		return errors.New("InitCmdMessage error, key is more tow char")
	}
	pd.ClientCmd = key + "HandlerCMD"
	pd.ServerCmd = key[1:2] + key[0:1] + "HandlerCMD"
	pd.ClientMessage = key + "ProtoMessage"
	pd.ServerMessage = key[1:2] + key[0:1] + "ProtoMessage"
	return nil
}

func (pd *ProtoData) checkProtoCommand() error {
	for _, _ = range pd.ServerCommands {

	}

	for _, _ = range pd.ClientCommands {

	}
	return nil
}

func (c *ProtoCommand) InitProtoName() error {
	list := strings.Split(c.CmdName, "_")
	if len(list) < 3 {
		return errors.New("ProtoName error" + c.CmdName)
	}
	if len(list[1]) != 2 {
		return errors.New("ProtoName error" + c.CmdName)
	}
	c.ServerProto = list[1] + list[2]
	c.ClientProto = list[1][1:2] + list[1][0:1] + list[2]
	c.ServerFunc = c.ClientProto + "Handler"
	c.ClientFunc = c.ServerProto + "Handler"
	c.ServerPackage = c.initPackageName(list[1][1:2]+list[1][0:1], list[2])
	c.ClientPackage = c.initPackageName(list[1], list[2])

	c.FunFileName()
	return nil
}

func (c *ProtoCommand) initPackageName(ser string, cmd string) (result string) {
	result = ser + "_"
	runes := []rune(cmd)
	for i, r := range runes {
		if unicode.IsUpper(r) {
			if i != 0 {
				break
			}
			result += string(unicode.ToLower(r))
		} else {
			result += string(r)
		}
	}
	return strings.ToLower(result)
}

func (c *ProtoCommand) FunFileName() (path, result string) {
	isEnd := false
	result = strings.ToLower(c.ServerProto[0:2] + "_")
	ProtoName := c.ServerProto[2:]
	if protoParams.Handlers != nil {
		_, _func := protoParams.Handlers.ExPackageFunc(c.HandlerID)
		if len(_func) > 2 {
			ProtoName = _func
		}
	}

	runes := []rune(ProtoName)
	for i, r := range runes {
		if unicode.IsUpper(r) {
			if i != 0 {
				result += "_"
				c.PathName = path
				isEnd = true
			}
			result += string(unicode.ToLower(r))
			if !isEnd {
				path += string(unicode.ToLower(r))
			}
		} else {
			result += string(r)
			if !isEnd {
				path += string(r)
			}
		}
	}
	return "/" + path + "/", result
}

func (c *ProtoCommand) ExClientPackage() string {
	if protoParams.Handlers != nil {
		_package, _ := protoParams.Handlers.ExPackageFunc(c.HandlerID)
		if len(_package) > 0 {
			return _package
		}
	}
	return c.ClientPackage
}

func (c *ProtoCommand) ExClientFunc() string {
	if protoParams.Handlers != nil {
		_, _func := protoParams.Handlers.ExPackageFunc(c.HandlerID)
		if len(_func) > 0 {
			return c.ClientFunc[0:2] + _func + "Handler"
		}
	}
	return c.ClientFunc
}
