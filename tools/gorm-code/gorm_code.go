package gorm_code

import (
	xl "tx-common/xlogger"
	"tx-tools/gorm-code/gorm_types"
	"tx-tools/gorm-code/templates/dao"
	"tx-tools/gorm-code/templates/data"
	"tx-tools/gorm-code/templates/logic"
	"tx-tools/gorm-code/templates/pool"
	"tx-tools/gorm-code/templates/redis"
	"tx-tools/utils"
)

func GenGormPoolTemplate(gormTypes map[string]*gorm_types.GormTypeData, sysPath, outPath string) bool {
	for s := range gormTypes {
		gormTypes[s].GormTypeBase.SystemPath = sysPath
	}
	if err := db_template.InitDataTemplate(); err != nil {
		xl.SError("InitDohTemplate error:%s", err.Error())
		return false
	}
	if err := logic.InitLogicTemplates(); err != nil {
		xl.SError("InitLogicTemplate error:%s", err.Error())
		return false
	}
	if err := dao_template.InitDaoTemplates(); err != nil {
		xl.SError("InitDaoTemplate error:%s", err.Error())
		return false
	}
	if err := pool_template.InitPoolTemplates(); err != nil {
		xl.SError("InitPoolTemplates error:%s", err.Error())
		return false
	}
	if err := redis_template.InitRedisTemplates(); err != nil {
		xl.SError("InitRedisTemplates error:%s", err.Error())
		return false
	}

	dohDir := outPath + "/data"
	_ = utils.MkdirPathIfNotExist(dohDir)
	db_template.GenDataTemplate(gormTypes, dohDir)

	logicDir := outPath + "/logic"
	_ = utils.MkdirPathIfNotExist(logicDir)
	logic.GenLogicTemplate(gormTypes, logicDir)

	daoDir := outPath + "/dao"
	_ = utils.MkdirPathIfNotExist(daoDir)
	dao_template.GenDaoTemplate(gormTypes, daoDir)

	poolDir := outPath + "/pool"
	_ = utils.MkdirPathIfNotExist(poolDir)
	pool_template.GenPoolTemplate(gormTypes, poolDir)

	redisDir := outPath + "/redis"
	_ = utils.MkdirPathIfNotExist(redisDir)
	redis_template.GenRedisTemplate(gormTypes, redisDir)

	return true
}
