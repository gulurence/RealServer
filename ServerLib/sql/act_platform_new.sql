/*
Navicat MySQL Data Transfer

Source Server         : 192.168.1.149
Source Server Version : 50173
Source Host           : 192.168.1.149:3306
Source Database       : act_platform_new

Target Server Type    : MYSQL
Target Server Version : 50173
File Encoding         : 65001

Date: 2014-09-22 19:52:43
*/

SET FOREIGN_KEY_CHECKS=0;
-- ----------------------------
-- Table structure for `antiaddict`
-- ----------------------------
DROP TABLE IF EXISTS `antiaddict`;
CREATE TABLE `antiaddict` (
  `accid` bigint(20) unsigned NOT NULL COMMENT '防沉迷信息',
  `verifiedzone` int(10) unsigned NOT NULL,
  `time` int(10) unsigned NOT NULL,
  `result` int(10) unsigned NOT NULL,
  `name` varchar(20) NOT NULL,
  `id` varchar(18) NOT NULL COMMENT '身份证id',
  PRIMARY KEY (`accid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of antiaddict
-- ----------------------------

-- ----------------------------
-- Table structure for `mediabag`
-- ----------------------------
DROP TABLE IF EXISTS `mediabag`;
CREATE TABLE `mediabag` (
  `bagid` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `bagname` varchar(32) NOT NULL,
  `createtime` int(10) unsigned NOT NULL,
  `startusetime` int(10) unsigned NOT NULL,
  `endusetime` int(10) unsigned NOT NULL,
  `state` int(10) unsigned NOT NULL,
  `drawnum` int(10) unsigned NOT NULL,
  `money` varchar(20) NOT NULL,
  `bindmoney` varchar(20) NOT NULL,
  `content` text,
  `notes` varchar(2048) NOT NULL,
  PRIMARY KEY (`bagid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of mediabag
-- ----------------------------

-- ----------------------------
-- Table structure for `mediacard`
-- ----------------------------
DROP TABLE IF EXISTS `mediacard`;
CREATE TABLE `mediacard` (
  `cardid` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `cardname` varchar(32) NOT NULL,
  `createtime` int(10) unsigned NOT NULL,
  `startusetime` int(10) unsigned NOT NULL,
  `endusetime` int(10) unsigned NOT NULL,
  `state` int(10) unsigned NOT NULL,
  `totalnum` int(10) unsigned NOT NULL,
  `drawnum` int(10) unsigned NOT NULL,
  `userlevel` int(10) unsigned NOT NULL,
  `userexploit` int(10) unsigned NOT NULL,
  `userability` int(10) unsigned NOT NULL,
  `userprofession` int(10) unsigned NOT NULL,
  `userlevelsort` int(10) unsigned NOT NULL,
  `userabilitysort` int(10) unsigned NOT NULL,
  `usersex` int(10) unsigned NOT NULL,
  `useliveness` int(10) unsigned NOT NULL,
  `useronlinetime` int(10) unsigned NOT NULL,
  `userdrawnum` int(10) unsigned NOT NULL,
  `useablezones` varchar(128) NOT NULL,
  `bagids` varchar(2048) NOT NULL,
  `notes` varchar(2048) NOT NULL,
  PRIMARY KEY (`cardid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of mediacard
-- ----------------------------

-- ----------------------------
-- Table structure for `mediacardserial`
-- ----------------------------
DROP TABLE IF EXISTS `mediacardserial`;
CREATE TABLE `mediacardserial` (
  `cardid` int(10) unsigned NOT NULL,
  `serial` varchar(36) NOT NULL,
  `givetime` int(10) unsigned NOT NULL,
  `giveaccid` bigint(20) unsigned NOT NULL,
  `giveserverid` int(10) unsigned NOT NULL,
  `state` int(10) unsigned NOT NULL,
  `drawtime` int(10) unsigned NOT NULL,
  `userzoneid` int(10) unsigned NOT NULL,
  `userid` bigint(20) unsigned NOT NULL,
  `username` varchar(32) NOT NULL,
  `uniqid` varchar(32) NOT NULL,
  UNIQUE KEY `serial` (`serial`),
  KEY `cardid` (`cardid`),
  KEY `giveaccid` (`giveaccid`),
  CONSTRAINT `mediacardserial_ibfk_1` FOREIGN KEY (`cardid`) REFERENCES `mediacard` (`cardid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of mediacardserial
-- ----------------------------

-- ----------------------------
-- Table structure for `minorityinfo`
-- ----------------------------
DROP TABLE IF EXISTS `minorityinfo`;
CREATE TABLE `minorityinfo` (
  `id` varchar(18) NOT NULL,
  `name` varchar(20) NOT NULL,
  `onlinemin` int(10) unsigned NOT NULL,
  `lastofflinetime` int(10) unsigned NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of minorityinfo
-- ----------------------------

-- ----------------------------
-- Table structure for `nameused`
-- ----------------------------
DROP TABLE IF EXISTS `nameused`;
CREATE TABLE `nameused` (
  `name` varchar(32) NOT NULL,
  PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of nameused
-- ----------------------------

-- ----------------------------
-- Table structure for `serverlist`
-- ----------------------------
DROP TABLE IF EXISTS `serverlist`;
CREATE TABLE `serverlist` (
  `zoneid` int(10) unsigned NOT NULL,
  `servertype` varchar(32) NOT NULL,
  `servername` varchar(32) NOT NULL,
  `ip` varchar(32) NOT NULL,
  `port` int(10) unsigned NOT NULL,
  `extip` varchar(32) NOT NULL,
  `extport` int(10) unsigned NOT NULL,
  UNIQUE KEY `zoneid` (`zoneid`,`ip`,`port`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- ----------------------------
-- Records of serverlist
-- ----------------------------
INSERT INTO `serverlist` VALUES ('1', 'GateServer', 'GateServer7', '127.0.0.7', '0', '127.0.0.1', '0');
INSERT INTO `serverlist` VALUES ('1', 'GateServer', 'GateServer8', '127.0.0.8', '0', '127.0.0.1', '0');
INSERT INTO `serverlist` VALUES ('1', 'GateServer', 'GateServer9', '127.0.0.9', '0', '127.0.0.1', '0');
INSERT INTO `serverlist` VALUES ('1', 'GateServer', 'GateServer10', '127.1.1.10', '0', '127.0.0.1', '0');
INSERT INTO `serverlist` VALUES ('1', 'RegServer', 'RegServer', '192.168.1.149', '21001', '192.168.1.149', '21000');
INSERT INTO `serverlist` VALUES ('1', 'Services', 'Services', '192.168.1.149', '27002', '192.168.1.149', '27002');
INSERT INTO `serverlist` VALUES ('1', 'CenterServer', 'CenterServer', '192.168.1.149', '27003', '192.168.1.149', '27003');
INSERT INTO `serverlist` VALUES ('1', 'SuperServer', 'SuperServer', '192.168.1.149', '31111', '192.168.1.149', '30101');
INSERT INTO `serverlist` VALUES ('1', 'RecordServer', 'RecordServer', '192.168.1.149', '31112', '192.168.1.149', '30102');
INSERT INTO `serverlist` VALUES ('1', 'SessionServer', 'SessionServer', '192.168.1.149', '31113', '192.168.1.149', '30103');
INSERT INTO `serverlist` VALUES ('1', 'SceneServer', 'SceneServer1', '192.168.1.149', '31114', '192.168.1.149', '30104');
INSERT INTO `serverlist` VALUES ('1', 'SceneServer', 'SceneServerQuest1', '192.168.1.149', '31115', '192.168.1.149', '30105');
INSERT INTO `serverlist` VALUES ('1', 'GateServer', 'GateServer1', '192.168.1.149', '31118', '192.168.1.149', '31118');
INSERT INTO `serverlist` VALUES ('1', 'GateServer', 'GateServer2', '192.168.1.149', '31119', '192.168.1.149', '31119');
INSERT INTO `serverlist` VALUES ('1', 'LogServer', 'LogServer', '192.168.1.149', '31120', '192.168.1.149', '30108');
INSERT INTO `serverlist` VALUES ('1', 'GateServer', 'GateServer3', '192.168.1.149', '31121', '192.168.1.149', '31121');
INSERT INTO `serverlist` VALUES ('1', 'GateServer', 'GateServer4', '192.168.1.149', '31122', '192.168.1.149', '31122');
INSERT INTO `serverlist` VALUES ('1', 'GateServer', 'GateServer5', '192.168.1.149', '31123', '192.168.1.149', '31123');
INSERT INTO `serverlist` VALUES ('1', 'GateServer', 'GateServer6', '192.168.1.149', '31124', '192.168.1.149', '31124');
INSERT INTO `serverlist` VALUES ('1', 'WebServer', 'WebServer', '192.168.1.149', '31198', '192.168.1.149', '31199');
