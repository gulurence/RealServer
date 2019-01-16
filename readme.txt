v:0.0.1
d:2019-1-16 23:52:00
e:gumy


purpose:
    game server template to all type game [ARPG/SLG/FPS/RTS/Chess/Sandbox/... .. .]
    [different module function to different game type,just need add to your servertemplate]


            *more see document dir*


======================= introduce ====================

Engine include :
1.libxml2
2.log4xx
3.dbconnect[mysql->c++]
4.net[window->iocp/linux->epoll]] 

Tip: Server can easy to build and just need write game logic, have all base function


======================= build ====================
build in windows 

   Use vs2017 Open Server.sln

build in linux [linux centos 7.0]

    1.clean all server build data use : 
        sh clean_server 
    2.make all server use :
        sh make_server
    3.run all server use :
        sh run_server
    4.stop all server use :
        sh stop_server
    

======================= deployment ====================



    1.db init
        1.use '\Document\sql' dir files create databases; same dbname to file name, and import data to table

    2.run param
        2.in db[act_platform_new] table[serverlist] is the all server ip/port info to server name [(ip/port) edit to you self]
        3.run in linux as [./GameServer -d -nGameServer1]  [-d run ad linux doman] [-n+ServerNodeName to get ip/port and config info]
        4.run in windows see to (3), but not need '-d'

    3.code edit
        5.in function 'MyServer::init' code 'setConnectServerType("server type name");' is to this server, has link server type
        6.in function 'MyServer::checkConnect' code 'checkConnectedServer("server type name");' is set this server need connect to target server type

    2.config
        7.in bin file dir log.xml is log4xx config file has all server config info
        8.in bin file dir config.xml is db connect config info




[excuse me for my poor english :) ]



