/*
Navicat MySQL Data Transfer

Source Server         : 219.235.1.239
Source Server Version : 50724
Source Host           : 219.235.1.239:3306
Source Database       : act_user_data_new

Target Server Type    : MYSQL
Target Server Version : 50724
File Encoding         : 65001

Date: 2019-01-04 20:23:35
*/

SET FOREIGN_KEY_CHECKS=0;
-- ----------------------------
-- Table structure for `abilitysort`
-- ----------------------------
DROP TABLE IF EXISTS `abilitysort`;
CREATE TABLE `abilitysort` (
  `charid` bigint(20) unsigned NOT NULL,
  `name` varchar(32) NOT NULL,
  `ability` int(10) unsigned NOT NULL,
  `country` int(10) unsigned NOT NULL,
  `profession` int(10) unsigned NOT NULL,
  PRIMARY KEY (`charid`),
  UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of abilitysort
-- ----------------------------

-- ----------------------------
-- Table structure for `auction`
-- ----------------------------
DROP TABLE IF EXISTS `auction`;
CREATE TABLE `auction` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `ownername` varchar(32) NOT NULL,
  `objid` int(10) unsigned NOT NULL,
  `objname` varchar(32) NOT NULL,
  `alevel` tinyint(3) unsigned NOT NULL,
  `atype` tinyint(3) unsigned NOT NULL,
  `color` tinyint(3) unsigned NOT NULL,
  `money` bigint(20) unsigned NOT NULL,
  `num` int(10) unsigned NOT NULL,
  `saletime` int(10) unsigned NOT NULL,
  `state` tinyint(3) unsigned NOT NULL,
  `bin` blob,
  PRIMARY KEY (`id`),
  KEY `auction_index` (`atype`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of auction
-- ----------------------------

-- ----------------------------
-- Table structure for `averexpstatistics`
-- ----------------------------
DROP TABLE IF EXISTS `averexpstatistics`;
CREATE TABLE `averexpstatistics` (
  `day` int(10) unsigned NOT NULL,
  `levels` int(10) unsigned NOT NULL,
  `type` int(10) unsigned NOT NULL,
  `exp` bigint(20) unsigned NOT NULL,
  KEY `day` (`day`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of averexpstatistics
-- ----------------------------

-- ----------------------------
-- Table structure for `averexptempstatistics`
-- ----------------------------
DROP TABLE IF EXISTS `averexptempstatistics`;
CREATE TABLE `averexptempstatistics` (
  `day` int(10) unsigned NOT NULL,
  `levels` int(10) unsigned NOT NULL,
  `type` int(10) unsigned NOT NULL,
  `charid` bigint(20) unsigned NOT NULL,
  `exp` bigint(20) unsigned NOT NULL,
  KEY `day` (`day`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of averexptempstatistics
-- ----------------------------

-- ----------------------------
-- Table structure for `battlekillsort`
-- ----------------------------
DROP TABLE IF EXISTS `battlekillsort`;
CREATE TABLE `battlekillsort` (
  `charid` bigint(20) unsigned NOT NULL,
  `name` varchar(32) NOT NULL,
  `killnum` int(10) unsigned NOT NULL,
  `country` int(10) unsigned NOT NULL,
  `profession` int(10) unsigned NOT NULL,
  `septname` varchar(32) NOT NULL,
  `day` int(10) unsigned NOT NULL,
  `isget` int(10) unsigned NOT NULL,
  `mtime` int(10) unsigned NOT NULL,
  `levels` int(10) unsigned NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of battlekillsort
-- ----------------------------

-- ----------------------------
-- Table structure for `bosslist`
-- ----------------------------
DROP TABLE IF EXISTS `bosslist`;
CREATE TABLE `bosslist` (
  `id` bigint(20) unsigned NOT NULL,
  `name` varchar(32) DEFAULT NULL,
  `mapname` varchar(32) DEFAULT NULL,
  `killerid` bigint(20) unsigned NOT NULL,
  `killername` varchar(32) NOT NULL,
  `killcount` int(10) unsigned NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of bosslist
-- ----------------------------

-- ----------------------------
-- Table structure for `cashcow`
-- ----------------------------
DROP TABLE IF EXISTS `cashcow`;
CREATE TABLE `cashcow` (
  `id` bigint(20) unsigned NOT NULL,
  `name` varchar(32) NOT NULL,
  `money` bigint(20) unsigned NOT NULL,
  `totalmoney` bigint(20) unsigned NOT NULL,
  `starttime` int(10) unsigned NOT NULL,
  `country` int(3) unsigned NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of cashcow
-- ----------------------------

-- ----------------------------
-- Table structure for `cashcowstealer`
-- ----------------------------
DROP TABLE IF EXISTS `cashcowstealer`;
CREATE TABLE `cashcowstealer` (
  `ownerid` bigint(20) unsigned NOT NULL,
  `name` varchar(32) NOT NULL,
  `money` bigint(20) unsigned NOT NULL,
  `country` int(3) unsigned NOT NULL,
  KEY `ownerid` (`ownerid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of cashcowstealer
-- ----------------------------

-- ----------------------------
-- Table structure for `charbase`
-- ----------------------------
DROP TABLE IF EXISTS `charbase`;
CREATE TABLE `charbase` (
  `platformid` int(10) unsigned NOT NULL COMMENT '平台ID',
  `zoneid` int(10) unsigned NOT NULL COMMENT '区ID',
  `accid` bigint(20) unsigned NOT NULL COMMENT '游戏账号ID',
  `charid` bigint(20) unsigned NOT NULL COMMENT '角色ID',
  `name` varchar(32) NOT NULL COMMENT '角色名',
  `source` varchar(128) NOT NULL COMMENT '来源',
  `loginType` tinyint(3) unsigned NOT NULL COMMENT '是否是flash 端',
  `createtime` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '创建时间',
  `country` int(10) unsigned NOT NULL COMMENT '国家',
  `septid` int(10) unsigned NOT NULL COMMENT '家族ID',
  `mapid` int(10) unsigned NOT NULL COMMENT '地图ID',
  `male` tinyint(3) unsigned NOT NULL COMMENT '性别: 1女0男',
  `profession` tinyint(3) unsigned NOT NULL COMMENT '职业: 1战士 2弓手 4谋士 8术士',
  `level` tinyint(3) unsigned NOT NULL COMMENT '等级',
  `exp` bigint(20) unsigned NOT NULL COMMENT '经验',
  `ip` int(10) unsigned NOT NULL COMMENT '玩家登陆IP',
  `onlinetime` int(10) unsigned NOT NULL COMMENT '在线时间(单位:秒)',
  `offlinetime` int(10) unsigned NOT NULL COMMENT '上次离线时间(Unix时间)',
  `addict` tinyint(3) unsigned NOT NULL COMMENT '防沉迷标识:  0验证成功 1未验证或验证失败 2未成年',
  `face` tinyint(3) NOT NULL,
  `money` bigint(20) unsigned NOT NULL COMMENT '银子(单位:文)',
  `gold` bigint(20) unsigned NOT NULL COMMENT '金子(单位:文)',
  `bindmoney` bigint(20) unsigned NOT NULL COMMENT '绑定银子(单位:文)',
  `bindgold` bigint(20) unsigned NOT NULL COMMENT '工资(单位:文)',
  `pay` bigint(20) unsigned NOT NULL COMMENT '已充值金额(单位:元)',
  `consume` bigint(20) unsigned NOT NULL COMMENT '已消耗银子(单位:文)',
  `pkmode` tinyint(3) unsigned NOT NULL COMMENT 'pk 模式',
  `pkvalue` int(10) unsigned NOT NULL COMMENT '当前PK值',
  `attrpoints` int(10) unsigned NOT NULL COMMENT '剩余属性点',
  `hp` int(10) unsigned NOT NULL COMMENT '血量',
  `vigor` int(5) unsigned NOT NULL COMMENT '活力值',
  `friendpoint` int(10) unsigned NOT NULL COMMENT '好友点',
  `notalkendtime` int(10) unsigned NOT NULL COMMENT '被禁言的截止时间',
  `noregendtime` int(10) unsigned NOT NULL COMMENT '被封号的截止时间',
  `vip` int(10) unsigned NOT NULL COMMENT 'vip道具获得的vip等级',
  `honor` int(10) unsigned NOT NULL COMMENT '声望值',
  `flag` int(10) unsigned NOT NULL COMMENT '玩家标识',
  `skillpoints` int(10) unsigned NOT NULL COMMENT 'skillpoints',
  `minloginType` varchar(512) NOT NULL,
  `deltime` int(10) unsigned NOT NULL COMMENT '玩家删除角色时间',
  `bin` blob COMMENT '二进制数据',
  `snapequip` varchar(256) NOT NULL,
  `guide` int(11) NOT NULL COMMENT '新手引导',
  `srcmapid` int(11) NOT NULL,
  `curbodystrength` int(11) NOT NULL COMMENT '前当体力点',
  `timetickbodystrength` int(11) NOT NULL COMMENT '体力点增加倒计时',
  `canbuybodystrength` int(11) NOT NULL COMMENT '今天可以购买的体力点数',
  `storage` blob COMMENT '仓库的二进制数据',
  `EspSpirit` int(11) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`charid`),
  UNIQUE KEY `name` (`name`),
  KEY `accid` (`accid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of charbase
-- ----------------------------

-- ----------------------------
-- Table structure for `charid`
-- ----------------------------
DROP TABLE IF EXISTS `charid`;
CREATE TABLE `charid` (
  `charid` int(10) unsigned NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`charid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of charid
-- ----------------------------

-- ----------------------------
-- Table structure for `city`
-- ----------------------------
DROP TABLE IF EXISTS `city`;
CREATE TABLE `city` (
  `cityid` bigint(20) unsigned NOT NULL COMMENT '城市ID',
  `name` varchar(32) NOT NULL COMMENT '城市名称',
  `country` bigint(20) unsigned NOT NULL COMMENT '国家ID',
  `master` bigint(20) unsigned NOT NULL,
  `money` bigint(20) unsigned NOT NULL,
  `mainbattle` int(10) unsigned NOT NULL,
  `firstbattle` int(3) unsigned NOT NULL,
  `notice` varchar(256) DEFAULT NULL COMMENT '通知，公告',
  `bin` blob,
  PRIMARY KEY (`cityid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of city
-- ----------------------------

-- ----------------------------
-- Table structure for `citymember`
-- ----------------------------
DROP TABLE IF EXISTS `citymember`;
CREATE TABLE `citymember` (
  `septid` bigint(20) unsigned NOT NULL,
  `name` varchar(32) NOT NULL,
  `cityid` bigint(20) unsigned NOT NULL,
  PRIMARY KEY (`septid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of citymember
-- ----------------------------

-- ----------------------------
-- Table structure for `citymemberapply`
-- ----------------------------
DROP TABLE IF EXISTS `citymemberapply`;
CREATE TABLE `citymemberapply` (
  `septid` bigint(20) unsigned NOT NULL,
  `cityid` bigint(20) unsigned NOT NULL,
  PRIMARY KEY (`septid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of citymemberapply
-- ----------------------------

-- ----------------------------
-- Table structure for `country`
-- ----------------------------
DROP TABLE IF EXISTS `country`;
CREATE TABLE `country` (
  `id` bigint(20) unsigned NOT NULL,
  `money` bigint(20) unsigned NOT NULL DEFAULT '0',
  `notice` varchar(64) NOT NULL,
  `officer` varchar(256) NOT NULL DEFAULT '',
  `lastdianquantime` int(10) unsigned NOT NULL DEFAULT '0',
  `zoneusernum` int(10) unsigned NOT NULL DEFAULT '0',
  `countryusernum` int(10) unsigned NOT NULL DEFAULT '0',
  `welfaremoney` bigint(20) unsigned NOT NULL DEFAULT '0',
  `lastsalarytime` int(10) unsigned NOT NULL DEFAULT '0',
  `salary` varchar(256) NOT NULL DEFAULT '',
  `tax` varchar(256) NOT NULL DEFAULT '',
  `fulitime` int(10) unsigned NOT NULL DEFAULT '0',
  `fulilevel` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `havekingtime` int(10) unsigned NOT NULL DEFAULT '0',
  `buffid` int(10) unsigned NOT NULL DEFAULT '0',
  `buffendtime` int(10) unsigned NOT NULL DEFAULT '0',
  `buffcount` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `master` bigint(20) unsigned NOT NULL,
  `bin` blob,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of country
-- ----------------------------

-- ----------------------------
-- Table structure for `countryinfo`
-- ----------------------------
DROP TABLE IF EXISTS `countryinfo`;
CREATE TABLE `countryinfo` (
  `countryid` bigint(20) unsigned NOT NULL,
  `charid` bigint(20) unsigned NOT NULL,
  `day` int(10) unsigned NOT NULL,
  KEY `countryid` (`countryid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of countryinfo
-- ----------------------------

-- ----------------------------
-- Table structure for `countrywelfare`
-- ----------------------------
DROP TABLE IF EXISTS `countrywelfare`;
CREATE TABLE `countrywelfare` (
  `country` int(10) unsigned NOT NULL,
  `type` tinyint(3) unsigned NOT NULL,
  `time` int(10) unsigned NOT NULL,
  `minlevel` tinyint(3) unsigned NOT NULL,
  `maxlevel` tinyint(3) unsigned NOT NULL,
  `booknum` int(10) unsigned NOT NULL,
  `drownum` int(10) unsigned NOT NULL,
  `money` bigint(20) unsigned NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of countrywelfare
-- ----------------------------

-- ----------------------------
-- Table structure for `day_online_time`
-- ----------------------------
DROP TABLE IF EXISTS `day_online_time`;
CREATE TABLE `day_online_time` (
  `charid` bigint(20) unsigned NOT NULL,
  `day` int(10) unsigned NOT NULL,
  `onlinetime` int(10) unsigned NOT NULL,
  KEY `charid` (`charid`,`day`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of day_online_time
-- ----------------------------

-- ----------------------------
-- Table structure for `friends`
-- ----------------------------
DROP TABLE IF EXISTS `friends`;
CREATE TABLE `friends` (
  `id` bigint(20) unsigned NOT NULL COMMENT '家玩ID',
  `count` smallint(6) NOT NULL,
  `bin` blob COMMENT '家玩所有好友信息 都保存到这个bin内部',
  PRIMARY KEY (`id`),
  KEY `id` (`id`),
  KEY `id_2` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of friends
-- ----------------------------

-- ----------------------------
-- Table structure for `friend_addlist`
-- ----------------------------
DROP TABLE IF EXISTS `friend_addlist`;
CREATE TABLE `friend_addlist` (
  `id` bigint(20) NOT NULL,
  `bin` blob,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

-- ----------------------------
-- Records of friend_addlist
-- ----------------------------

-- ----------------------------
-- Table structure for `gateinfo`
-- ----------------------------
DROP TABLE IF EXISTS `gateinfo`;
CREATE TABLE `gateinfo` (
  `zoneid` int(10) unsigned NOT NULL,
  `ip` int(10) unsigned NOT NULL,
  `port` int(10) unsigned NOT NULL,
  `onlinenum` int(10) unsigned NOT NULL,
  UNIQUE KEY `ip` (`ip`,`port`),
  KEY `zone` (`zoneid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of gateinfo
-- ----------------------------

-- ----------------------------
-- Table structure for `globalrecord`
-- ----------------------------
DROP TABLE IF EXISTS `globalrecord`;
CREATE TABLE `globalrecord` (
  `id` int(10) unsigned NOT NULL,
  `name` varchar(32) CHARACTER SET latin1 NOT NULL,
  `value` int(10) unsigned NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of globalrecord
-- ----------------------------

-- ----------------------------
-- Table structure for `gmhelper`
-- ----------------------------
DROP TABLE IF EXISTS `gmhelper`;
CREATE TABLE `gmhelper` (
  `charid` bigint(20) unsigned NOT NULL,
  `nickname` varchar(32) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of gmhelper
-- ----------------------------

-- ----------------------------
-- Table structure for `groups`
-- ----------------------------
DROP TABLE IF EXISTS `groups`;
CREATE TABLE `groups` (
  `id` bigint(20) unsigned NOT NULL,
  `flag` tinyint(3) unsigned NOT NULL,
  `groupname` varchar(14) DEFAULT NULL,
  `memberid` bigint(20) unsigned NOT NULL,
  `membername` varchar(32) NOT NULL,
  `memberface` tinyint(3) unsigned NOT NULL,
  `post` varchar(64) DEFAULT NULL,
  `lasttime` int(10) unsigned DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of groups
-- ----------------------------

-- ----------------------------
-- Table structure for `killsort`
-- ----------------------------
DROP TABLE IF EXISTS `killsort`;
CREATE TABLE `killsort` (
  `charid` bigint(20) unsigned NOT NULL,
  `name` varchar(32) NOT NULL,
  `killnum` int(10) unsigned NOT NULL,
  `country` int(10) unsigned NOT NULL,
  `profession` int(10) unsigned NOT NULL,
  `septname` varchar(32) NOT NULL,
  `day` int(10) unsigned NOT NULL,
  `isget` int(10) unsigned NOT NULL,
  `mtime` int(10) unsigned NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of killsort
-- ----------------------------

-- ----------------------------
-- Table structure for `leavesept`
-- ----------------------------
DROP TABLE IF EXISTS `leavesept`;
CREATE TABLE `leavesept` (
  `charid` bigint(20) unsigned NOT NULL,
  `mtime` int(10) unsigned NOT NULL,
  `count` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`charid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of leavesept
-- ----------------------------

-- ----------------------------
-- Table structure for `loginstatistics`
-- ----------------------------
DROP TABLE IF EXISTS `loginstatistics`;
CREATE TABLE `loginstatistics` (
  `day` int(10) unsigned NOT NULL,
  `levels` int(10) unsigned NOT NULL,
  `num` int(10) unsigned NOT NULL,
  KEY `day` (`day`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of loginstatistics
-- ----------------------------

-- ----------------------------
-- Table structure for `mail`
-- ----------------------------
DROP TABLE IF EXISTS `mail`;
CREATE TABLE `mail` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `type` tinyint(3) unsigned NOT NULL,
  `sendtime` int(10) unsigned NOT NULL,
  `sourname` varchar(32) NOT NULL,
  `destname` varchar(32) NOT NULL,
  `charge` int(10) unsigned NOT NULL DEFAULT '0',
  `moneytype` tinyint(3) unsigned NOT NULL,
  `money` int(10) unsigned NOT NULL DEFAULT '0',
  `content` varchar(200) NOT NULL DEFAULT '',
  `readflag` tinyint(3) unsigned NOT NULL,
  `issendback` tinyint(3) unsigned NOT NULL,
  `item` blob,
  PRIMARY KEY (`id`),
  KEY `destname` (`destname`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of mail
-- ----------------------------

-- ----------------------------
-- Table structure for `mapexpstatistics`
-- ----------------------------
DROP TABLE IF EXISTS `mapexpstatistics`;
CREATE TABLE `mapexpstatistics` (
  `day` int(10) unsigned NOT NULL,
  `levels` int(10) unsigned NOT NULL,
  `mapid` int(10) unsigned NOT NULL,
  `exp` bigint(20) unsigned NOT NULL,
  KEY `day` (`day`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of mapexpstatistics
-- ----------------------------

-- ----------------------------
-- Table structure for `maxexpstatistics`
-- ----------------------------
DROP TABLE IF EXISTS `maxexpstatistics`;
CREATE TABLE `maxexpstatistics` (
  `day` int(10) unsigned NOT NULL,
  `levels` int(10) unsigned NOT NULL,
  `type` int(10) unsigned NOT NULL,
  `exp` bigint(20) unsigned NOT NULL,
  KEY `day` (`day`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of maxexpstatistics
-- ----------------------------

-- ----------------------------
-- Table structure for `mushop_sale_records`
-- ----------------------------
DROP TABLE IF EXISTS `mushop_sale_records`;
CREATE TABLE `mushop_sale_records` (
  `name` varchar(32) NOT NULL,
  `itemid` int(10) unsigned NOT NULL,
  `qual` tinyint(3) unsigned NOT NULL,
  `time` int(10) unsigned NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of mushop_sale_records
-- ----------------------------

-- ----------------------------
-- Table structure for `nextcharid`
-- ----------------------------
DROP TABLE IF EXISTS `nextcharid`;
CREATE TABLE `nextcharid` (
  `zoneid` int(5) unsigned NOT NULL,
  `nextcharid` int(10) unsigned NOT NULL DEFAULT '21',
  PRIMARY KEY (`zoneid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of nextcharid
-- ----------------------------

-- ----------------------------
-- Table structure for `orderlist`
-- ----------------------------
DROP TABLE IF EXISTS `orderlist`;
CREATE TABLE `orderlist` (
  `orderlist` varchar(32) NOT NULL,
  `time` int(10) unsigned NOT NULL,
  PRIMARY KEY (`orderlist`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of orderlist
-- ----------------------------

-- ----------------------------
-- Table structure for `partnersort`
-- ----------------------------
DROP TABLE IF EXISTS `partnersort`;
CREATE TABLE `partnersort` (
  `charid` bigint(20) unsigned NOT NULL,
  `name` varchar(32) NOT NULL,
  `value` int(10) unsigned NOT NULL,
  `country` int(10) unsigned NOT NULL,
  `color` int(10) unsigned NOT NULL,
  `petname` varchar(32) NOT NULL,
  `mtime` int(10) unsigned NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of partnersort
-- ----------------------------

-- ----------------------------
-- Table structure for `paysort`
-- ----------------------------
DROP TABLE IF EXISTS `paysort`;
CREATE TABLE `paysort` (
  `charid` bigint(20) unsigned NOT NULL,
  `name` varchar(32) NOT NULL,
  `value` bigint(20) unsigned NOT NULL,
  `country` int(10) unsigned NOT NULL,
  `profession` int(10) unsigned NOT NULL,
  `day` int(10) unsigned NOT NULL,
  `anonymous` int(10) unsigned NOT NULL,
  `isget` int(10) unsigned NOT NULL,
  `mtime` int(10) unsigned NOT NULL,
  `type` int(10) unsigned NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of paysort
-- ----------------------------

-- ----------------------------
-- Table structure for `purchase`
-- ----------------------------
DROP TABLE IF EXISTS `purchase`;
CREATE TABLE `purchase` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `ownername` varchar(32) NOT NULL,
  `objid` int(10) unsigned NOT NULL,
  `objname` varchar(32) NOT NULL,
  `alevel` tinyint(3) unsigned NOT NULL,
  `atype` tinyint(3) unsigned NOT NULL,
  `color` tinyint(3) unsigned NOT NULL,
  `money` bigint(20) unsigned NOT NULL,
  `num` int(10) unsigned NOT NULL,
  `saletime` int(10) unsigned NOT NULL,
  `state` tinyint(3) unsigned NOT NULL,
  PRIMARY KEY (`id`),
  KEY `purchase_index` (`atype`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of purchase
-- ----------------------------

-- ----------------------------
-- Table structure for `sept`
-- ----------------------------
DROP TABLE IF EXISTS `sept`;
CREATE TABLE `sept` (
  `septid` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(32) NOT NULL,
  `country` tinyint(3) unsigned NOT NULL,
  `master` bigint(20) unsigned NOT NULL,
  `assistant` varchar(256) DEFAULT NULL,
  `elder` varchar(256) DEFAULT NULL,
  `money` bigint(20) unsigned NOT NULL,
  `createtime` int(10) unsigned NOT NULL,
  `alevel` tinyint(3) unsigned NOT NULL,
  `maplevel` tinyint(3) unsigned NOT NULL,
  `notice` varchar(256) DEFAULT NULL,
  `onbuilding` tinyint(3) unsigned NOT NULL,
  `buildendtime` int(10) unsigned NOT NULL,
  `builds` varchar(128) DEFAULT NULL,
  `haveseptmap` tinyint(3) unsigned NOT NULL,
  `citybattleid` int(10) unsigned NOT NULL,
  `maxmember` int(10) unsigned NOT NULL,
  `bin` blob,
  PRIMARY KEY (`septid`),
  UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of sept
-- ----------------------------

-- ----------------------------
-- Table structure for `septmember`
-- ----------------------------
DROP TABLE IF EXISTS `septmember`;
CREATE TABLE `septmember` (
  `charid` bigint(20) unsigned NOT NULL,
  `name` varchar(32) NOT NULL,
  `septid` bigint(20) unsigned NOT NULL,
  `alevel` tinyint(3) unsigned NOT NULL,
  `nickname` varchar(32) NOT NULL,
  `contribution` int(10) unsigned NOT NULL,
  `lastonline` int(10) unsigned NOT NULL,
  `carecount` int(10) unsigned NOT NULL,
  `carepoint` int(10) unsigned NOT NULL,
  `plant` blob,
  PRIMARY KEY (`charid`),
  UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of septmember
-- ----------------------------

-- ----------------------------
-- Table structure for `septmemberapply`
-- ----------------------------
DROP TABLE IF EXISTS `septmemberapply`;
CREATE TABLE `septmemberapply` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `charid` bigint(20) unsigned NOT NULL,
  `name` varchar(32) NOT NULL,
  `alevel` tinyint(3) unsigned NOT NULL,
  `septid` bigint(20) unsigned NOT NULL,
  `applytime` int(10) unsigned NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `charid` (`charid`,`septid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of septmemberapply
-- ----------------------------

-- ----------------------------
-- Table structure for `sharem`
-- ----------------------------
DROP TABLE IF EXISTS `sharem`;
CREATE TABLE `sharem` (
  `shareid` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `charid` bigint(20) unsigned NOT NULL,
  `itemid` int(10) unsigned NOT NULL,
  `cfgid` int(10) unsigned NOT NULL,
  `isget` tinyint(3) unsigned NOT NULL,
  `sharetype` tinyint(3) unsigned NOT NULL,
  `createtime` int(10) unsigned NOT NULL,
  `level` int(10) unsigned NOT NULL,
  `sharename` varchar(32) DEFAULT NULL,
  `text` varchar(80) DEFAULT NULL,
  PRIMARY KEY (`shareid`),
  KEY `charid` (`charid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of sharem
-- ----------------------------

-- ----------------------------
-- Table structure for `sort`
-- ----------------------------
DROP TABLE IF EXISTS `sort`;
CREATE TABLE `sort` (
  `charid` bigint(20) unsigned NOT NULL,
  `name` varchar(32) NOT NULL,
  `level` int(10) unsigned NOT NULL,
  `exp` bigint(20) unsigned NOT NULL,
  `country` int(10) unsigned NOT NULL,
  `profession` int(10) unsigned NOT NULL,
  PRIMARY KEY (`charid`),
  UNIQUE KEY `name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of sort
-- ----------------------------

-- ----------------------------
-- Table structure for `statistics_char_active`
-- ----------------------------
DROP TABLE IF EXISTS `statistics_char_active`;
CREATE TABLE `statistics_char_active` (
  `charid` bigint(20) unsigned NOT NULL,
  `atype` tinyint(3) unsigned NOT NULL,
  `loginType` tinyint(3) unsigned NOT NULL,
  `onlinetime` int(10) unsigned NOT NULL,
  `offlinetime` int(10) unsigned NOT NULL,
  `profession` tinyint(3) unsigned NOT NULL,
  `ip` int(10) unsigned NOT NULL,
  `pay` int(10) unsigned NOT NULL,
  KEY `ip` (`ip`),
  KEY `atype_onlinetime_charid` (`atype`,`onlinetime`,`charid`),
  KEY `charid_onlinetime` (`charid`,`onlinetime`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of statistics_char_active
-- ----------------------------

-- ----------------------------
-- Table structure for `statistics_map_money`
-- ----------------------------
DROP TABLE IF EXISTS `statistics_map_money`;
CREATE TABLE `statistics_map_money` (
  `day` int(10) unsigned NOT NULL,
  `levels` int(10) unsigned NOT NULL,
  `mapid` int(10) unsigned NOT NULL,
  `money` bigint(20) unsigned NOT NULL,
  `type` int(10) unsigned NOT NULL,
  KEY `day` (`day`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of statistics_map_money
-- ----------------------------

-- ----------------------------
-- Table structure for `statistics_max_money`
-- ----------------------------
DROP TABLE IF EXISTS `statistics_max_money`;
CREATE TABLE `statistics_max_money` (
  `day` int(10) unsigned NOT NULL,
  `levels` int(10) unsigned NOT NULL,
  `type` int(10) unsigned NOT NULL,
  `money` bigint(20) unsigned NOT NULL,
  KEY `day` (`day`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of statistics_max_money
-- ----------------------------

-- ----------------------------
-- Table structure for `statistics_normal_total`
-- ----------------------------
DROP TABLE IF EXISTS `statistics_normal_total`;
CREATE TABLE `statistics_normal_total` (
  `day` int(10) unsigned NOT NULL,
  `levels` int(10) unsigned NOT NULL,
  `type` int(10) unsigned NOT NULL,
  `param` int(10) unsigned NOT NULL,
  `value` bigint(20) unsigned NOT NULL,
  KEY `day` (`day`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of statistics_normal_total
-- ----------------------------

-- ----------------------------
-- Table structure for `statistics_online_member`
-- ----------------------------
DROP TABLE IF EXISTS `statistics_online_member`;
CREATE TABLE `statistics_online_member` (
  `time` int(10) unsigned NOT NULL,
  `onlinenum` int(10) unsigned NOT NULL,
  `flashnum` int(10) unsigned NOT NULL,
  PRIMARY KEY (`time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of statistics_online_member
-- ----------------------------

-- ----------------------------
-- Table structure for `twitter`
-- ----------------------------
DROP TABLE IF EXISTS `twitter`;
CREATE TABLE `twitter` (
  `twitterid` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `id` bigint(20) unsigned NOT NULL,
  `name` varchar(32) NOT NULL,
  `face` tinyint(3) unsigned NOT NULL,
  `newstype` tinyint(3) unsigned NOT NULL,
  `flag` tinyint(3) unsigned NOT NULL,
  `gentime` int(10) unsigned NOT NULL,
  `updatetime` int(10) unsigned NOT NULL,
  `color` int(10) unsigned NOT NULL,
  `msg` varchar(200) DEFAULT NULL,
  PRIMARY KEY (`twitterid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of twitter
-- ----------------------------

-- ----------------------------
-- Table structure for `twittercomment`
-- ----------------------------
DROP TABLE IF EXISTS `twittercomment`;
CREATE TABLE `twittercomment` (
  `id` bigint(20) unsigned NOT NULL,
  `name` varchar(32) NOT NULL,
  `face` tinyint(3) unsigned NOT NULL,
  `newstype` tinyint(3) unsigned NOT NULL,
  `gentime` int(10) unsigned NOT NULL,
  `twitterid` bigint(20) unsigned NOT NULL,
  `authorid` bigint(20) unsigned NOT NULL,
  `authornewstype` tinyint(3) unsigned NOT NULL,
  `msg` varchar(200) DEFAULT NULL,
  KEY `twitterid` (`twitterid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of twittercomment
-- ----------------------------

-- ----------------------------
-- Table structure for `twitterforward`
-- ----------------------------
DROP TABLE IF EXISTS `twitterforward`;
CREATE TABLE `twitterforward` (
  `id` bigint(20) unsigned NOT NULL,
  `name` varchar(32) NOT NULL,
  `face` tinyint(3) unsigned NOT NULL,
  `newstype` tinyint(3) unsigned NOT NULL,
  `flag` tinyint(3) unsigned NOT NULL,
  `gentime` int(10) unsigned NOT NULL,
  `twitterid` bigint(20) unsigned NOT NULL,
  `authorid` bigint(20) unsigned NOT NULL,
  `authornewstype` tinyint(3) unsigned NOT NULL,
  `msg` varchar(200) DEFAULT NULL,
  KEY `twitterid` (`twitterid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of twitterforward
-- ----------------------------

-- ----------------------------
-- Table structure for `world_records`
-- ----------------------------
DROP TABLE IF EXISTS `world_records`;
CREATE TABLE `world_records` (
  `day` int(10) unsigned NOT NULL,
  `worldlevel` int(10) unsigned NOT NULL,
  `strongcountry` tinyint(3) unsigned NOT NULL,
  `midcountry` tinyint(3) unsigned NOT NULL,
  `weakcountry` tinyint(3) unsigned NOT NULL,
  PRIMARY KEY (`day`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of world_records
-- ----------------------------
