package nats_template

import (
    "bytes"
    "text/template"

    "tx-tools/nats-code/proto"
)

func ParseProtoData(parse *template.Template, data *nats_proto.NatsProtoData) ([]byte, error) {
    var buf bytes.Buffer
    if err := parse.Execute(&buf, data); err != nil {
        return nil, err
    }
    return buf.Bytes(), nil
}

func ParseCommandData(parse *template.Template, data *nats_proto.NatsProtoCommand) ([]byte, error) {
    var buf bytes.Buffer
    if err := parse.Execute(&buf, data); err != nil {
        return nil, err
    }
    return buf.Bytes(), nil
}
