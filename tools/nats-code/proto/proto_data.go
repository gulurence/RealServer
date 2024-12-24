package nats_proto

import (
    "errors"
    "strings"
)

var protoParams *NatsProtoParams

func InitProtoParams(system string, imports []string) {
    protoParams = &NatsProtoParams{
        SystemName: system,
        Imports:    imports,
    }
}

func (c *NatsProtoCommand) InitProtoName() error {
    list := strings.Split(c.CmdName, "_")
    if len(list) < 3 {
        return errors.New("ProtoName error" + c.CmdName)
    }
    c.ProtoName = list[1] + list[2]
    return nil
}
