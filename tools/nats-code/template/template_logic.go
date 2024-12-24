package nats_template

import (
    "text/template"

    xl "tx-common/xlogger"
    "tx-tools/nats-code/proto"
    "tx-tools/utils"
)

var (
    natsCmdParse *template.Template
)

func InitProtoTemplate() (err error) {
    if natsCmdParse, err = utils.InitTemplate("natsCmdParse", natsTemplateStr); err != nil {
        xl.SError(err.Error())
        return err
    }
    return nil
}

func GenNatsTemplate(protoData map[string]*nats_proto.NatsProtoData, serverDir string) {
    if natsCmdParse == nil {
        xl.SError("GenServerTemplate error: dohParse is nil.")
        return
    }
    for _, proto := range protoData {
        if proto == nil {
            xl.SErrorf("GenServerTemplate SaveFile error:%s", "Wrong Workdir Set Handler_gen workdir as D:/work/tx!")
            continue
        }
        if b, err := ParseProtoData(natsCmdParse, proto); err == nil {
            if err = saveProtoDataFile(b, serverDir, "nats_proto_init.go", true); err != nil {
                xl.SErrorf("GenNatsTemplate SaveFile nats_proto_init.go error:%s", err.Error())
            }
        } else {
            xl.SErrorf("GenNatsTemplate error:%s", err.Error())
            return
        }
    }
    return
}

func saveProtoDataFile(bytes []byte, destDir, fileName string, clean bool) error {
    err := utils.SaveFile(destDir, fileName, bytes, clean, true)
    if err != nil {
        return err
    }
    xl.SInfof("SaveFile FileName:%s succeed!", fileName)
    return nil
}
