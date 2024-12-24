package proto_template

import (
	"bytes"
	"text/template"
	"tx-tools/proto-code/proto"
)

func ParseProtoData(parse *template.Template, data *proto_type.ProtoData) ([]byte, error) {
	var buf bytes.Buffer
	if err := parse.Execute(&buf, data); err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

func ParseCommandData(parse *template.Template, data *proto_type.ProtoCommand) ([]byte, error) {
	var buf bytes.Buffer
	if err := parse.Execute(&buf, data); err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}
