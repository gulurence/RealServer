/*
Navicat MySQL Data Transfer

Source Server         : 219.235.1.239
Source Server Version : 50724
Source Host           : 219.235.1.239:3306
Source Database       : qjfml_log

Target Server Type    : MYSQL
Target Server Version : 50724
File Encoding         : 65001

Date: 2019-01-04 20:24:09
*/

SET FOREIGN_KEY_CHECKS=0;
-- ----------------------------
-- Table structure for `action`
-- ----------------------------
DROP TABLE IF EXISTS `action`;
CREATE TABLE `action` (
  `actionid` int(10) NOT NULL AUTO_INCREMENT,
  `actionname` varchar(30) NOT NULL DEFAULT '',
  `actioncolumnid` int(10) NOT NULL DEFAULT '0',
  `action` varchar(20) NOT NULL DEFAULT '',
  `viewmode` tinyint(1) NOT NULL DEFAULT '0',
  PRIMARY KEY (`actionid`)
) ENGINE=MyISAM AUTO_INCREMENT=91 DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of action
-- ----------------------------

-- ----------------------------
-- Table structure for `actioncolumn`
-- ----------------------------
DROP TABLE IF EXISTS `actioncolumn`;
CREATE TABLE `actioncolumn` (
  `actioncolumnid` int(10) NOT NULL AUTO_INCREMENT,
  `actioncolumnname` varchar(30) NOT NULL DEFAULT '',
  `createdate` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`actioncolumnid`)
) ENGINE=MyISAM AUTO_INCREMENT=7 DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of actioncolumn
-- ----------------------------

-- ----------------------------
-- Table structure for `actiongroup`
-- ----------------------------
DROP TABLE IF EXISTS `actiongroup`;
CREATE TABLE `actiongroup` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `action` varchar(20) NOT NULL DEFAULT '',
  `groupid` int(10) NOT NULL DEFAULT '0',
  `createdate` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=368 DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of actiongroup
-- ----------------------------

-- ----------------------------
-- Table structure for `active`
-- ----------------------------
DROP TABLE IF EXISTS `active`;
CREATE TABLE `active` (
  `time` int(10) NOT NULL,
  `zoneid` int(11) NOT NULL,
  `accid` int(11) NOT NULL,
  `charid` bigint(20) NOT NULL,
  `islogin` tinyint(1) NOT NULL,
  `onlinetime` int(11) NOT NULL DEFAULT '0',
  `platformid` int(10) unsigned NOT NULL,
  KEY `s1` (`time`) USING HASH,
  KEY `s2` (`zoneid`) USING HASH,
  KEY `s3` (`onlinetime`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of active
-- ----------------------------

-- ----------------------------
-- Table structure for `activity`
-- ----------------------------
DROP TABLE IF EXISTS `activity`;
CREATE TABLE `activity` (
  `actid` varchar(20) NOT NULL DEFAULT '',
  `range` varchar(200) NOT NULL DEFAULT '',
  `condition` tinytext NOT NULL,
  `starttime` int(11) NOT NULL,
  `endtime` int(11) NOT NULL,
  `reward` varchar(300) NOT NULL DEFAULT '',
  `picture` varchar(100) NOT NULL DEFAULT '',
  `createdate` int(11) NOT NULL,
  KEY `actid` (`actid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of activity
-- ----------------------------

-- ----------------------------
-- Table structure for `adm_user`
-- ----------------------------
DROP TABLE IF EXISTS `adm_user`;
CREATE TABLE `adm_user` (
  `uid` int(11) NOT NULL AUTO_INCREMENT,
  `username` varchar(30) NOT NULL,
  `password` varchar(32) NOT NULL,
  PRIMARY KEY (`uid`),
  UNIQUE KEY `username` (`username`)
) ENGINE=MyISAM AUTO_INCREMENT=3 DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of adm_user
-- ----------------------------

-- ----------------------------
-- Table structure for `announcement`
-- ----------------------------
DROP TABLE IF EXISTS `announcement`;
CREATE TABLE `announcement` (
  `aid` mediumint(7) NOT NULL AUTO_INCREMENT,
  `subject` varchar(100) NOT NULL DEFAULT '',
  `content` tinytext NOT NULL,
  `mid` int(10) NOT NULL DEFAULT '0',
  `mname` varchar(30) NOT NULL DEFAULT '',
  `createdate` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`aid`)
) ENGINE=MyISAM AUTO_INCREMENT=4 DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of announcement
-- ----------------------------

-- ----------------------------
-- Table structure for `equiplog`
-- ----------------------------
DROP TABLE IF EXISTS `equiplog`;
CREATE TABLE `equiplog` (
  `accid` bigint(20) NOT NULL,
  `zoneid` int(11) NOT NULL,
  `time` int(11) NOT NULL,
  `charid` bigint(20) NOT NULL,
  `equipiD` int(11) NOT NULL,
  `equipname` varchar(32) NOT NULL,
  `guid` varchar(64) NOT NULL,
  `log` varchar(32) NOT NULL,
  `platformid` int(11) NOT NULL,
  PRIMARY KEY (`accid`,`charid`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

-- ----------------------------
-- Records of equiplog
-- ----------------------------

-- ----------------------------
-- Table structure for `groupmanager`
-- ----------------------------
DROP TABLE IF EXISTS `groupmanager`;
CREATE TABLE `groupmanager` (
  `groupid` int(10) NOT NULL AUTO_INCREMENT,
  `groupname` varchar(30) NOT NULL DEFAULT '',
  `groupinfo` varchar(100) NOT NULL DEFAULT '0',
  `createdate` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`groupid`)
) ENGINE=MyISAM AUTO_INCREMENT=7 DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of groupmanager
-- ----------------------------

-- ----------------------------
-- Table structure for `itemlog`
-- ----------------------------
DROP TABLE IF EXISTS `itemlog`;
CREATE TABLE `itemlog` (
  `accid` int(11) NOT NULL,
  `charid` bigint(20) DEFAULT NULL,
  `zoneid` int(11) NOT NULL,
  `time` int(10) unsigned NOT NULL,
  `itemid` int(10) unsigned NOT NULL,
  `itemname` varchar(32) NOT NULL,
  `itemnum` int(10) unsigned NOT NULL,
  `qual` tinyint(3) unsigned NOT NULL,
  `type` int(10) unsigned NOT NULL,
  `platformid` int(10) unsigned NOT NULL,
  PRIMARY KEY (`time`),
  KEY `s1` (`accid`) USING HASH,
  KEY `s2` (`time`) USING BTREE,
  KEY `s3` (`itemid`,`itemname`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of itemlog
-- ----------------------------

-- ----------------------------
-- Table structure for `lowerlevel`
-- ----------------------------
DROP TABLE IF EXISTS `lowerlevel`;
CREATE TABLE `lowerlevel` (
  `power1` varchar(10) NOT NULL DEFAULT '',
  `power2` varchar(10) NOT NULL DEFAULT ''
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of lowerlevel
-- ----------------------------

-- ----------------------------
-- Table structure for `mail`
-- ----------------------------
DROP TABLE IF EXISTS `mail`;
CREATE TABLE `mail` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `time` int(10) unsigned NOT NULL,
  `accid` int(11) NOT NULL,
  `zoneid` int(11) NOT NULL,
  `charid` bigint(20) NOT NULL,
  `title` varchar(100) DEFAULT NULL,
  `content` varchar(1000) DEFAULT NULL,
  `sign` tinyint(1) unsigned zerofill NOT NULL DEFAULT '0' COMMENT '0æœªå›žå¤1å·²è¯»2å·²å›žå¤',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of mail
-- ----------------------------

-- ----------------------------
-- Table structure for `mailinglist`
-- ----------------------------
DROP TABLE IF EXISTS `mailinglist`;
CREATE TABLE `mailinglist` (
  `mailid` mediumint(8) NOT NULL AUTO_INCREMENT,
  `mid` int(10) NOT NULL,
  `mname` varchar(30) NOT NULL DEFAULT '',
  `mailto` varchar(300) NOT NULL DEFAULT '',
  `subject` varchar(100) NOT NULL DEFAULT '',
  `content` tinytext NOT NULL,
  `createdate` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`mailid`)
) ENGINE=MyISAM AUTO_INCREMENT=4 DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of mailinglist
-- ----------------------------

-- ----------------------------
-- Table structure for `mapcoor`
-- ----------------------------
DROP TABLE IF EXISTS `mapcoor`;
CREATE TABLE `mapcoor` (
  `cid` int(10) NOT NULL AUTO_INCREMENT,
  `name` varchar(50) NOT NULL DEFAULT '',
  `x` varchar(20) NOT NULL DEFAULT '',
  `y` varchar(20) NOT NULL DEFAULT '',
  PRIMARY KEY (`cid`)
) ENGINE=MyISAM AUTO_INCREMENT=5 DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of mapcoor
-- ----------------------------

-- ----------------------------
-- Table structure for `master`
-- ----------------------------
DROP TABLE IF EXISTS `master`;
CREATE TABLE `master` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `name` varchar(30) NOT NULL DEFAULT '',
  `password` varchar(32) NOT NULL DEFAULT '',
  `viewmode` tinyint(1) NOT NULL DEFAULT '0',
  `email` varchar(100) NOT NULL DEFAULT '',
  `platformid` int(11) NOT NULL DEFAULT '0',
  `createdate` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=10 DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of master
-- ----------------------------

-- ----------------------------
-- Table structure for `mastergroup`
-- ----------------------------
DROP TABLE IF EXISTS `mastergroup`;
CREATE TABLE `mastergroup` (
  `id` int(10) NOT NULL AUTO_INCREMENT,
  `masterid` int(10) NOT NULL DEFAULT '0',
  `name` varchar(30) NOT NULL DEFAULT '',
  `groupid` int(10) NOT NULL DEFAULT '0',
  `createdate` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=MyISAM AUTO_INCREMENT=10 DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of mastergroup
-- ----------------------------

-- ----------------------------
-- Table structure for `moneylog`
-- ----------------------------
DROP TABLE IF EXISTS `moneylog`;
CREATE TABLE `moneylog` (
  `accid` bigint(20) NOT NULL,
  `time` int(11) NOT NULL,
  `charid` bigint(20) NOT NULL,
  `ulevel` smallint(6) NOT NULL,
  `moneytype` smallint(6) NOT NULL,
  `optype` smallint(6) NOT NULL,
  `count` smallint(6) NOT NULL,
  `num` bigint(20) NOT NULL,
  `remain` bigint(20) NOT NULL,
  `reason` char(255) NOT NULL,
  `platformid` int(11) NOT NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

-- ----------------------------
-- Records of moneylog
-- ----------------------------

-- ----------------------------
-- Table structure for `notice`
-- ----------------------------
DROP TABLE IF EXISTS `notice`;
CREATE TABLE `notice` (
  `nid` int(10) NOT NULL AUTO_INCREMENT,
  `mid` int(10) NOT NULL DEFAULT '0',
  `mname` varchar(30) NOT NULL DEFAULT '',
  `type` tinyint(3) NOT NULL DEFAULT '0',
  `status` tinyint(1) NOT NULL DEFAULT '0',
  `area` varchar(300) NOT NULL DEFAULT '',
  `content` tinytext NOT NULL,
  `starttime` int(11) NOT NULL DEFAULT '0',
  `endtime` int(11) NOT NULL DEFAULT '0',
  `number` mediumint(7) NOT NULL DEFAULT '0',
  `createdate` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`nid`)
) ENGINE=MyISAM AUTO_INCREMENT=5 DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of notice
-- ----------------------------

-- ----------------------------
-- Table structure for `online`
-- ----------------------------
DROP TABLE IF EXISTS `online`;
CREATE TABLE `online` (
  `time` int(11) NOT NULL,
  `zoneid` int(11) NOT NULL,
  `mapid` int(11) NOT NULL,
  `num` int(11) NOT NULL,
  `platformid` int(10) unsigned NOT NULL,
  PRIMARY KEY (`time`,`mapid`),
  KEY `s1` (`time`,`zoneid`,`mapid`) USING BTREE,
  KEY `s2` (`time`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of online
-- ----------------------------
INSERT INTO online VALUES ('1422350098', '1', '999', '0', '20130001');
INSERT INTO online VALUES ('1422350098', '1', '1003', '2', '20130001');
INSERT INTO online VALUES ('1422350098', '1', '1004', '0', '20130001');
INSERT INTO online VALUES ('1422350098', '1', '10001', '0', '20130001');
INSERT INTO online VALUES ('1422350098', '1', '10002', '0', '20130001');
INSERT INTO online VALUES ('1422350098', '1', '10003', '0', '20130001');
INSERT INTO online VALUES ('1422350098', '1', '10004', '0', '20130001');
INSERT INTO online VALUES ('1422350098', '1', '10005', '0', '20130001');
INSERT INTO online VALUES ('1422350098', '1', '10006', '0', '20130001');
INSERT INTO online VALUES ('1422350098', '1', '10007', '0', '20130001');
INSERT INTO online VALUES ('1422350098', '1', '10008', '0', '20130001');
INSERT INTO online VALUES ('1422350098', '1', '10009', '0', '20130001');
INSERT INTO online VALUES ('1422350098', '1', '10010', '0', '20130001');
INSERT INTO online VALUES ('1422350098', '1', '10011', '0', '20130001');
INSERT INTO online VALUES ('1422350098', '1', '10012', '0', '20130001');
INSERT INTO online VALUES ('1422350098', '1', '10013', '0', '20130001');
INSERT INTO online VALUES ('1422350098', '1', '10014', '0', '20130001');
INSERT INTO online VALUES ('1422350098', '1', '10015', '0', '20130001');
INSERT INTO online VALUES ('1422350098', '1', '10016', '0', '20130001');
INSERT INTO online VALUES ('1422350099', '1', '20001', '0', '20130001');
INSERT INTO online VALUES ('1422350099', '1', '20002', '0', '20130001');

-- ----------------------------
-- Table structure for `operatinglog`
-- ----------------------------
DROP TABLE IF EXISTS `operatinglog`;
CREATE TABLE `operatinglog` (
  `account` bigint(20) NOT NULL,
  `charid` bigint(20) NOT NULL,
  `zoneid` int(11) NOT NULL,
  `objtype` int(11) NOT NULL,
  `operatingtype` int(11) NOT NULL,
  `systemid` int(11) NOT NULL,
  `time` int(11) NOT NULL,
  `p1` int(11) DEFAULT NULL,
  `p2` int(11) DEFAULT NULL,
  `p3` int(11) DEFAULT NULL,
  `param` varchar(512) DEFAULT NULL,
  PRIMARY KEY (`charid`,`time`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

-- ----------------------------
-- Records of operatinglog
-- ----------------------------

-- ----------------------------
-- Table structure for `operlog`
-- ----------------------------
DROP TABLE IF EXISTS `operlog`;
CREATE TABLE `operlog` (
  `logid` int(10) NOT NULL AUTO_INCREMENT,
  `mid` int(10) NOT NULL,
  `mname` varchar(30) NOT NULL DEFAULT '',
  `msg` varchar(300) NOT NULL DEFAULT '',
  `opertime` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`logid`)
) ENGINE=MyISAM AUTO_INCREMENT=9 DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of operlog
-- ----------------------------

-- ----------------------------
-- Table structure for `order`
-- ----------------------------
DROP TABLE IF EXISTS `order`;
CREATE TABLE `order` (
  `oid` varchar(20) NOT NULL DEFAULT '',
  `power1` varchar(20) NOT NULL DEFAULT '',
  `appid` int(10) NOT NULL DEFAULT '0',
  `applicant` varchar(20) NOT NULL DEFAULT '',
  `content` varchar(100) NOT NULL DEFAULT '',
  `reason` varchar(200) NOT NULL DEFAULT '',
  `power2` varchar(10) NOT NULL DEFAULT '0',
  `origin` varchar(20) NOT NULL DEFAULT '',
  `status` tinyint(2) NOT NULL,
  `result` tinyint(1) NOT NULL DEFAULT '0',
  `datetime` int(11) NOT NULL DEFAULT '0',
  KEY `oid` (`oid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of order
-- ----------------------------

-- ----------------------------
-- Table structure for `orderstatus`
-- ----------------------------
DROP TABLE IF EXISTS `orderstatus`;
CREATE TABLE `orderstatus` (
  `sid` int(10) NOT NULL AUTO_INCREMENT,
  `oid` varchar(20) NOT NULL DEFAULT '',
  `power` varchar(10) NOT NULL DEFAULT '0',
  `hid` int(10) NOT NULL,
  `hname` varchar(30) NOT NULL DEFAULT '',
  `status` tinyint(2) NOT NULL DEFAULT '0',
  `reason` varchar(200) NOT NULL DEFAULT '',
  `datetime` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`sid`)
) ENGINE=MyISAM AUTO_INCREMENT=27 DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of orderstatus
-- ----------------------------

-- ----------------------------
-- Table structure for `replymail`
-- ----------------------------
DROP TABLE IF EXISTS `replymail`;
CREATE TABLE `replymail` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `mailid` int(11) NOT NULL COMMENT 'å›žå¤é‚®ä»¶ID',
  `userid` int(11) NOT NULL COMMENT 'é›‡å‘˜ID',
  `username` varchar(30) DEFAULT NULL COMMENT 'é›‡å‘˜åå­—',
  `mailstructs` varchar(2000) NOT NULL COMMENT 'é‚®ä»¶å†…å®¹ç»“æž„ä½“,json',
  `isok` tinyint(1) DEFAULT NULL COMMENT '0å¤±è´¥,1æˆåŠŸ',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of replymail
-- ----------------------------

-- ----------------------------
-- Table structure for `reward`
-- ----------------------------
DROP TABLE IF EXISTS `reward`;
CREATE TABLE `reward` (
  `rid` varchar(20) NOT NULL DEFAULT '',
  `zoneid` int(11) NOT NULL DEFAULT '0',
  `rolename` varchar(32) NOT NULL DEFAULT '',
  `award` varchar(500) NOT NULL DEFAULT '',
  `status` tinyint(1) NOT NULL DEFAULT '0',
  `datetime` int(11) NOT NULL,
  KEY `rid` (`rid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of reward
-- ----------------------------

-- ----------------------------
-- Table structure for `salerecords`
-- ----------------------------
DROP TABLE IF EXISTS `salerecords`;
CREATE TABLE `salerecords` (
  `accid` int(11) NOT NULL,
  `zoneid` int(11) NOT NULL,
  `time` int(10) unsigned NOT NULL,
  `charid` bigint(20) unsigned NOT NULL,
  `moneytype` tinyint(3) unsigned NOT NULL,
  `price` bigint(20) unsigned NOT NULL,
  `itemid` int(10) unsigned NOT NULL,
  `itemname` varchar(32) NOT NULL,
  `itemnum` int(10) unsigned NOT NULL,
  `qual` tinyint(3) unsigned NOT NULL,
  `ulevel` int(10) unsigned NOT NULL,
  `platformid` int(10) unsigned NOT NULL,
  KEY `s1` (`time`) USING BTREE,
  KEY `s2` (`charid`) USING HASH,
  KEY `s3` (`moneytype`) USING BTREE,
  KEY `s4` (`itemid`,`itemname`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of salerecords
-- ----------------------------

-- ----------------------------
-- Table structure for `supervisoryauth`
-- ----------------------------
DROP TABLE IF EXISTS `supervisoryauth`;
CREATE TABLE `supervisoryauth` (
  `account` char(64) NOT NULL,
  `pwd` char(64) NOT NULL,
  PRIMARY KEY (`account`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

-- ----------------------------
-- Records of supervisoryauth
-- ----------------------------

-- ----------------------------
-- Table structure for `tradelog`
-- ----------------------------
DROP TABLE IF EXISTS `tradelog`;
CREATE TABLE `tradelog` (
  `accid` int(11) DEFAULT NULL,
  `charid` bigint(20) DEFAULT NULL,
  `zoneid` int(11) NOT NULL,
  `time` int(10) unsigned NOT NULL,
  `fromname` varchar(32) NOT NULL,
  `toname` varchar(32) NOT NULL,
  `itemid` int(10) unsigned NOT NULL,
  `itemname` varchar(32) NOT NULL,
  `itemnum` int(10) unsigned NOT NULL,
  `guid` varchar(32) NOT NULL,
  `money` bigint(20) unsigned NOT NULL,
  `tax` bigint(20) unsigned NOT NULL,
  `log` varchar(32) NOT NULL,
  `platformid` int(10) unsigned NOT NULL,
  KEY `s1` (`fromname`) USING BTREE,
  KEY `s2` (`toname`) USING BTREE,
  KEY `s3` (`fromname`,`toname`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of tradelog
-- ----------------------------

-- ----------------------------
-- Table structure for `uservisitlog`
-- ----------------------------
DROP TABLE IF EXISTS `uservisitlog`;
CREATE TABLE `uservisitlog` (
  `time` int(11) NOT NULL,
  `accid` int(11) NOT NULL,
  `zoneid` int(11) NOT NULL,
  `ipaddr` int(11) NOT NULL,
  `maxcversion` int(3) NOT NULL,
  `mincversion` varchar(512) NOT NULL,
  `type` int(3) NOT NULL,
  `flagkey` varchar(256) NOT NULL,
  `platformid` int(10) unsigned NOT NULL,
  KEY `s1` (`time`) USING BTREE,
  KEY `s2` (`zoneid`,`ipaddr`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of uservisitlog
-- ----------------------------
