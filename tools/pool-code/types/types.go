package pool_types

const (
    FuncTemplate = iota
    ImplTemplate
    MaxTemplate
)

type IImports interface {
    LogicImports(key string, list []string) []string
}
