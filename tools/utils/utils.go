package utils

import (
	"go/format"
	"hash/fnv"
	"os"
	"path/filepath"
	"strings"
	"text/template"
	"time"
	"unicode"

	xl "tx-common/xlogger"
)

func InitTemplate(key, str string) (*template.Template, error) {
	return template.New(key).Funcs(template.FuncMap{
		"CamelizeStr":   CamelizeStr,
		"LowerLeftWord": LowerLeftWord,
		"now": func() string {
			return time.Now().Format("2006-01-02 Z07:00")
		},
	}).Parse(str)
}

func FirstCharacter(name string) string {
	return strings.ToLower(name)[:1]
}

func CamelizeStr(str, p string) (result string) {
	if len(str) == 0 {
		return str
	}
	str = replaceInvalidChars(str)
	content := str[0:2]
	if content == "DB" {
		result = p + str[2:]
	} else {
		result = p + str
	}
	return result
}

func LowerRightWord(str string) string {
	return strings.ToLower(str[:1]) + str[1:]
}

func UpperRightWord(str string) string {
	return strings.ToUpper(str[:1]) + str[1:]
}

func HashString(str string) uint32 {
	h := fnv.New32a()
	_, _ = h.Write([]byte(str))
	return h.Sum32()
}

func LowerLeftWord(str string) (result string) {
	runes := []rune(str)
	count := 0
	for _, r := range runes {
		if unicode.IsUpper(r) {
			count++
		} else {
			break
		}
	}
	for i := 0; i < count-1; i++ {
		runes[i] = unicode.ToLower(runes[i])
	}
	return string(runes)
}

func GormDataFileName(str string, p string) (result string) {
	str = CamelizeStr(str, p)
	runes := []rune(str)
	for i, r := range runes {
		if unicode.IsUpper(r) {
			if i != 0 {
				result += "_"
			}
			result += string(unicode.ToLower(r))
		} else {
			result += string(r)
		}
	}
	return result
}

func CamelizeWord(word string) string {
	runes := []rune(word)
	for i, r := range runes {
		if i == 0 {
			runes[i] = unicode.ToUpper(r)
		} else {
			runes[i] = unicode.ToLower(r)
		}
	}
	return string(runes)
}

func replaceInvalidChars(str string) string {
	str = strings.ReplaceAll(str, "-", "_")
	str = strings.ReplaceAll(str, " ", "_")
	return strings.ReplaceAll(str, ".", "_")
}

func SaveFile(dirPath, fileName string, text []byte, clean, fmt bool) error {
	if clean {
		_ = os.Remove(filepath.Join(dirPath, fileName))
	} else {
		if _, err := os.Stat(filepath.Join(dirPath, fileName)); err == nil || os.IsExist(err) {
			xl.SWarnf("Save File: %s has been, should not over write it!", fileName)
			return err
		}
	}
	err := MkdirPathIfNotExist(dirPath)
	if err != nil {
		xl.SErrorf("MkDirPath %s Error, %s", dirPath, err.Error())
		return err
	}
	file, err := os.Create(filepath.Join(dirPath, fileName))
	if err != nil {
		return err
	}
	defer func(file *os.File) {
		_ = file.Close()
	}(file)

	if !fmt {
		_, err = file.Write(text)
		return err
	}

	p, err := format.Source(text)
	if err != nil {
		return err
	}
	p = []byte(strings.ReplaceAll(string(p), "\t", "    "))
	_, err = file.Write(p)
	return err
}

func MkdirPathIfNotExist(dirPath string) error {
	if _, err := os.Stat(dirPath); os.IsNotExist(err) {
		return os.MkdirAll(dirPath, os.ModePerm)
	}
	return nil
}

func CleanUpGenFiles(dir string) error {
	exist, err := FileExists(dir)
	if err != nil {
		return err
	}
	if exist {
		return os.RemoveAll(dir)
	}
	return nil
}

// FileExists reports whether the named file or directory exists.
func FileExists(name string) (bool, error) {
	if _, err := os.Stat(name); err != nil {
		if os.IsNotExist(err) {
			return false, err
		}
	}
	return true, nil
}

func TemplateDataFileName(str string, p string) (result string) {
	str = CamelizeStr(str, p)
	runes := []rune(str)
	for i, r := range runes {
		if unicode.IsUpper(r) {
			if i != 0 {
				result += "_"
			}
			result += string(unicode.ToLower(r))
		} else {
			result += string(r)
		}
	}
	return result
}

func RedisKeyName(str string) (result string) {
	str = replaceInvalidChars(str)
	runes := []rune(str)
	splits := []rune("_")
	var hasUpper = true
	for _, r := range runes {
		if !hasUpper {
			result += string(unicode.ToUpper(r))
			hasUpper = true
			continue
		}
		if r == splits[0] {
			hasUpper = false
		}
		result += string(r)
	}
	return result
}

func AddImportsString(list []string, str string, index int) (newList []string) {
	for i, s := range list {
		if i == index {
			newList = append(newList, str)
		}
		newList = append(newList, s)
	}
	return newList
}

func AddImportsStringBefore(list []string, str string, aft string) (newList []string) {
	for _, s := range list {
		if s == aft {
			newList = append(newList, str)
		}
		newList = append(newList, s)
	}
	return newList
}

func AddImportsXXString(list []string, str string) (newList []string) {
	for _, s := range list {
		if s == str {
			return list
		}
	}
	return append(list, str)
}

func AddImportsStringAfter(list []string, str string, aft string) (newList []string) {
	for _, s := range list {
		newList = append(newList, s)
		if s == aft {
			newList = append(newList, str)
		}
	}
	return newList
}

func RemoveImports(list []string, index int) []string {
	copy(list[index:], list[index+1:])
	return list[:len(list)-1]
}

func RemoveImportsString(list []string, str string) []string {
	for i, s := range list {
		if s == str {
			return RemoveImports(list, i)
		}
	}
	return list
}
