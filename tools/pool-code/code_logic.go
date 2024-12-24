package pool_code

import (
    "bytes"
    "strings"
    "text/template"
    "time"

    xl "tx-common/xlogger"
    poolTemplate "tx-tools/pool-code/template"
    poolTypes "tx-tools/pool-code/types"
    "tx-tools/utils"
)

var logicParses []*template.Template
var _imports poolTypes.IImports

func InitLogicTemplates(imports poolTypes.IImports) (err error) {
    logicParses = make([]*template.Template, poolTypes.MaxTemplate)
    if logicParses[poolTypes.FuncTemplate], err = InitTemplate("FuncFileTStr", poolTemplate.FuncTemplateStr); err != nil {
        return err
    }
    if logicParses[poolTypes.ImplTemplate], err = InitTemplate("ImplFileTStr", poolTemplate.ImplTemplateStr); err != nil {
        return err
    }
    _imports = imports
    return nil
}

func GenLogicTemplate(configs map[string]*poolTypes.TemplateFileConfig) {
    for key, config := range configs {
        poolData := poolTypes.NewPoolData(key, config, _imports)
        // Impl Pool Template
        if b, err := ParseTemplateFileData(logicParses[poolTypes.ImplTemplate], poolData); err == nil {
            if err = saveLogicFile(config.NameSpace+"_impl", b, config.DirPath); err != nil {
                xl.SErrorf("GenLogicTemplate SaveFile FileName:%s error:%s", key, err.Error())
            }
        } else {
            xl.SErrorf("ParseGormData error:%s", err.Error())
            return
        }
        // Func Pool Template
        if b, err := ParseTemplateFileData(logicParses[poolTypes.FuncTemplate], poolData); err == nil {
            if err = saveLogicFile(config.NameSpace+"_func", b, config.DirPath); err != nil {
                xl.SErrorf("GenLogicTemplate SaveFile FileName:%s error:%s", key, err.Error())
            }
        } else {
            xl.SErrorf("ParseGormData error:%s", err.Error())
            return
        }
    }
    return
}

func saveLogicFile(fileName string, bytes []byte, destDir string) error {
    if err := utils.SaveFile(destDir, strings.ToLower(fileName+".go"), bytes, true, true); err != nil {
        return err
    }
    xl.SInfof("SaveFile FileName:%s succeed!", fileName)
    return nil
}

func ParseTemplateFileData(parse *template.Template, data *poolTypes.PoolData) ([]byte, error) {
    var buf bytes.Buffer
    if err := parse.Execute(&buf, data); err != nil {
        return nil, err
    }
    return buf.Bytes(), nil
}

func InitTemplate(key, str string) (*template.Template, error) {
    return template.New(key).Funcs(template.FuncMap{
        "CamelizeStr":    utils.CamelizeStr,
        "LowerLeftWord":  utils.LowerLeftWord,
        "LowerRightWord": utils.LowerRightWord,
        "now": func() string {
            return time.Now().Format("2006-01-02 Z07:00")
        },
    }).Parse(str)
}
