package proto_template

import (
	"strings"
	"text/template"

	"tx-tools/proto-code/proto"
	"tx-tools/utils"

	xl "tx-common/xlogger"
)

var (
	serverHParse *template.Template
	serverFParse *template.Template
	clientHParse *template.Template
	clientFParse *template.Template
)

func InitProtoTemplate() (err error) {
	if serverHParse, err = utils.InitTemplate("serverHParse", serverHandlerTemplateStr); err != nil {
		xl.SError(err.Error())
		return err
	}
	if serverFParse, err = utils.InitTemplate("serverFParse", serverFuncTemplateStr); err != nil {
		xl.SError(err.Error())
		return err
	}
	if clientHParse, err = utils.InitTemplate("clientHParse", clientHandlerTemplateStr); err != nil {
		xl.SError(err.Error())
		return err
	}
	if clientFParse, err = utils.InitTemplate("clientFParse", clientFuncTemplateStr); err != nil {
		xl.SError(err.Error())
		return err
	}
	return nil
}

func GenServerTemplate(protoData map[string]*proto_type.ProtoData, serverDir string) {
	if serverHParse == nil || serverFParse == nil {
		xl.SError("GenServerTemplate error: dohParse is nil.")
		return
	}
	for _, proto := range protoData {
		if proto == nil {
			xl.SErrorf("GenServerTemplate SaveFile error:%s", "Wrong Workdir Set Handler_gen workdir as D:/work/tx!")
			continue
		}
		if b, err := ParseProtoData(serverHParse, proto); err == nil {
			if err := saveProtoDataFile(b, serverDir, strings.ToLower(proto.ServerPackage())+"_init.go", true); err != nil {
				xl.SErrorf("GenServerTemplate SaveFile FileName:%s error:%s", proto.FileName, err.Error())
			}
		} else {
			xl.SErrorf("GenServerTemplate error:%s", err.Error())
			return
		}

		/*        for _, command := range proto.ServerCommands {
		          if command.IsMessage {
		              continue
		          }
		          if b, err := ParseCommandData(serverFParse, command); err == nil {
		              path, fileName := command.FunFileName()
		              if err = saveProtoDataFile(b, serverDir+path, fileName+".go", false); err != nil {
		                  xl.SErrorf("GenServerTemplate SaveFile FileName:%s error:%s", proto.FileName, err.Error())
		              }
		          } else {
		              xl.SErrorf("GenServerTemplate error:%s", err.Error())
		              return
		          }
		      }*/
		for _, command := range proto.ClientCommands {
			if b, err := ParseCommandData(serverFParse, command); err == nil {
				path, fileName := command.FunFileName()
				if err = saveProtoDataFile(b, serverDir+path, fileName+".go", false); err != nil {
					xl.SErrorf("GenServerTemplate SaveFile FileName:%s error:%s", proto.FileName, err.Error())
				}
			} else {
				xl.SErrorf("GenServerTemplate error:%s", err.Error())
				return
			}
		}
	}
	return
}

func GenClientTemplate(protoData map[string]*proto_type.ProtoData, clientDir string) {
	if clientHParse == nil || clientFParse == nil {
		xl.SError("GenFuncTemplate error: dohParse is nil.")
		return
	}
	for _, proto := range protoData {
		if b, err := ParseProtoData(clientHParse, proto); err == nil {
			if err = saveProtoDataFile(b, clientDir, strings.ToLower(proto.ClientPackage())+"_init.go", true); err != nil {
				xl.SErrorf("GenFuncTemplate SaveFile FileName:%s error:%s", proto.FileName, err.Error())
			}
		} else {
			xl.SErrorf("GenFuncTemplate error:%s", err.Error())
			return
		}

		for _, command := range proto.ServerCommands {
			if b, err := ParseCommandData(clientFParse, command); err == nil {
				path, fileName := command.FunFileName()
				if err = saveProtoDataFile(b, clientDir+path, fileName+".go", false); err != nil {
					xl.SErrorf("GenFuncTemplate SaveFile FileName:%s error:%s", proto.FileName, err.Error())
				}
			} else {
				xl.SErrorf("GenFuncTemplate error:%s", err.Error())
				return
			}
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
