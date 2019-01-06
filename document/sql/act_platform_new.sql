/*
Navicat MySQL Data Transfer

Source Server         : 219.235.1.239
Source Server Version : 50724
Source Host           : 219.235.1.239:3306
Source Database       : act_platform_new

Target Server Type    : MYSQL
Target Server Version : 50724
File Encoding         : 65001

Date: 2019-01-06 22:55:17
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
  `servertypeid` int(10) NOT NULL COMMENT '路由表ID',
  `servertype` varchar(30) NOT NULL,
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
INSERT INTO serverlist VALUES ('1', '7000', 'WorldServer', 'WorldServer1', '219.235.1.239', '20000', '219.235.1.239', '21000');
INSERT INTO serverlist VALUES ('1', '7000', 'WorldServer', 'WorldServer2', '219.235.1.239', '20001', '219.235.1.239', '21001');
INSERT INTO serverlist VALUES ('1', '7000', 'WorldServer', 'WorldServer3', '219.235.1.239', '20002', '219.235.1.239', '21002');
INSERT INTO serverlist VALUES ('1', '6000', 'GameServer', 'GameServer1', '219.235.1.239', '20100', '219.235.1.239', '21100');
INSERT INTO serverlist VALUES ('1', '6000', 'GameServer', 'GameServer2', '219.235.1.239', '20101', '219.235.1.239', '21101');
INSERT INTO serverlist VALUES ('1', '6000', 'GameServer', 'GameServer3', '219.235.1.239', '20102', '219.235.1.239', '21102');
INSERT INTO serverlist VALUES ('1', '5', 'FriendServerC', 'FriendServerC', '219.235.1.239', '20200', '219.235.1.239', '21200');
INSERT INTO serverlist VALUES ('1', '5000', 'FriendServer', 'FriendServer1', '219.235.1.239', '20201', '219.235.1.239', '21201');
INSERT INTO serverlist VALUES ('1', '5000', 'FriendServer', 'FriendServer2', '219.235.1.239', '20202', '219.235.1.239', '21202');
INSERT INTO serverlist VALUES ('1', '5000', 'FriendServer', 'FriendServer3', '219.235.1.239', '20203', '219.235.1.239', '21203');
INSERT INTO serverlist VALUES ('1', '4', 'AllianceServerC', 'AllianceServerC', '219.235.1.239', '20300', '219.235.1.239', '21300');
INSERT INTO serverlist VALUES ('1', '4000', 'AllianceServer', 'AllianceServer1', '219.235.1.239', '20301', '219.235.1.239', '21301');
INSERT INTO serverlist VALUES ('1', '4000', 'AllianceServer', 'AllianceServer2', '219.235.1.239', '20302', '219.235.1.239', '21302');
INSERT INTO serverlist VALUES ('1', '4000', 'AllianceServer', 'AllianceServer3', '219.235.1.239', '20303', '219.235.1.239', '21303');
INSERT INTO serverlist VALUES ('1', '3', 'TransferServer', 'TransferServer', '219.235.1.239', '20900', '219.235.1.239', '21900');
INSERT INTO serverlist VALUES ('1', '2', 'LoginServer', 'LoginServer', '219.235.1.239', '20901', '219.235.1.239', '21901');
INSERT INTO serverlist VALUES ('1', '1', 'RegServer', 'RegServer', '219.235.1.239', '20902', '219.235.1.239', '21902');

-- ----------------------------
-- Table structure for `serverlist1`
-- ----------------------------
DROP TABLE IF EXISTS `serverlist1`;
CREATE TABLE `serverlist1` (
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
-- Records of serverlist1
-- ----------------------------
INSERT INTO serverlist1 VALUES ('1', 'SessionServer', 'SessionServer', '127.0.0.1', '31113', '127.0.0.1', '30103');
INSERT INTO serverlist1 VALUES ('1', 'GateServer', 'GateServer7', '127.0.0.7', '0', '127.0.0.1', '0');
INSERT INTO serverlist1 VALUES ('1', 'GateServer', 'GateServer8', '127.0.0.8', '0', '127.0.0.1', '0');
INSERT INTO serverlist1 VALUES ('1', 'GateServer', 'GateServer9', '127.0.0.9', '0', '127.0.0.1', '0');
INSERT INTO serverlist1 VALUES ('1', 'GateServer', 'GateServer10', '127.1.1.10', '0', '127.0.0.1', '0');
INSERT INTO serverlist1 VALUES ('1', 'RegServer', 'RegServer', '219.235.1.239', '21001', '219.235.1.239', '21000');
INSERT INTO serverlist1 VALUES ('1', 'Services', 'Services', '219.235.1.239', '27002', '219.235.1.239', '27002');
INSERT INTO serverlist1 VALUES ('1', 'CenterServer', 'CenterServer', '219.235.1.239', '27003', '219.235.1.239', '27003');
INSERT INTO serverlist1 VALUES ('1', 'SuperServer', 'SuperServer', '219.235.1.239', '31111', '219.235.1.239', '30101');
INSERT INTO serverlist1 VALUES ('1', 'RecordServer', 'RecordServer', '219.235.1.239', '31112', '219.235.1.239', '30102');
INSERT INTO serverlist1 VALUES ('1', 'SceneServer', 'SceneServer1', '219.235.1.239', '31114', '219.235.1.239', '30104');
INSERT INTO serverlist1 VALUES ('1', 'SceneServer', 'SceneServerQuest1', '219.235.1.239', '31115', '219.235.1.239', '30105');
INSERT INTO serverlist1 VALUES ('1', 'GateServer', 'GateServer1', '219.235.1.239', '31118', '219.235.1.239', '31118');
INSERT INTO serverlist1 VALUES ('1', 'GateServer', 'GateServer2', '219.235.1.239', '31119', '219.235.1.239', '31119');
INSERT INTO serverlist1 VALUES ('1', 'LogServer', 'LogServer', '219.235.1.239', '31120', '219.235.1.239', '30108');
INSERT INTO serverlist1 VALUES ('1', 'GateServer', 'GateServer3', '219.235.1.239', '31121', '219.235.1.239', '31121');
INSERT INTO serverlist1 VALUES ('1', 'GateServer', 'GateServer4', '219.235.1.239', '31122', '219.235.1.239', '31122');
INSERT INTO serverlist1 VALUES ('1', 'GateServer', 'GateServer5', '219.235.1.239', '31123', '219.235.1.239', '31123');
INSERT INTO serverlist1 VALUES ('1', 'GateServer', 'GateServer6', '219.235.1.239', '31124', '219.235.1.239', '31124');
INSERT INTO serverlist1 VALUES ('1', 'WebServer', 'WebServer', '219.235.1.239', '31198', '219.235.1.239', '31199');

-- ----------------------------
-- Table structure for `test_account`
-- ----------------------------
DROP TABLE IF EXISTS `test_account`;
CREATE TABLE `test_account` (
  `account` varchar(25) NOT NULL,
  `pwd` varchar(25) NOT NULL,
  PRIMARY KEY (`account`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

-- ----------------------------
-- Records of test_account
-- ----------------------------
INSERT INTO test_account VALUES ('000000', '0');
INSERT INTO test_account VALUES ('0000005', '0000005');
