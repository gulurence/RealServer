package nats_proto

import (
    "go/ast"
)

type NatsProtoParams struct {
    Imports    []string `json:"imports"`
    SystemName string   `json:"system_name"`
}

type NatsProtoCommand struct {
    CmdId     int    `json:"cmd_id"`
    CmdName   string `json:"cmd_name"`
    Package   string `json:"package"`
    ProtoName string `json:"proto_name"`
}

type NatsProtoData struct {
    NatsImports   []string                     `json:"imports"`
    SystemName    string                       `json:"system_name"`
    Package       string                       `json:"package"`
    NatsCommands  map[string]*NatsProtoCommand `json:"nats_commands"`
    ProtoMessages map[string]*ast.TypeSpec     `json:"proto_messages"`
}
