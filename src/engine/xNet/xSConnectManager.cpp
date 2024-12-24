#include "xSConnectManager.h"
//#include "../xServer/ZoneServer.h"
#include "../xCommand/xCommand.h"


xSConnectManager::xSConnectManager(ZoneServer* pServer)
    : m_pZoneServer(pServer) {

#ifndef _WIN32
    server_epfd = 0;
    server_epfd = epoll_create(256);
    if (server_epfd < 0) {
        XERR("epoll_create() failed ");
        server_epfd = 0;
    } else
        XLOG("create server epfd ");
#else
    FD_ZERO(&m_ReadFDs[SELECT_BAK]);
    FD_ZERO(&m_WriteFDs[SELECT_BAK]);
    FD_ZERO(&m_ExceptFDs[SELECT_BAK]);
    m_Timeout[SELECT_BAK].tv_sec = 0;
    m_Timeout[SELECT_BAK].tv_usec = 10;
    m_MinFD = m_MaxFD = INVALID_SOCKET;
    m_nFDSize = 0;
#endif

}

#ifdef _WIN32

void xSConnectManager::addSocket(xNetProcessor* pProcess) {
    //if (!pProcess) {
    //    return;
    //}

    //SOCKET fd = pProcess->GetSock().get_fd();
    //if (fd == INVALID_SOCKET) {
    //    return;
    //}

    //if (m_nFDSize >= FD_SETSIZE) {
    //    return;
    //}

    //m_MinFD = ((m_MinFD == INVALID_SOCKET) ? fd : min(fd, m_MinFD));
    //m_MaxFD = ((m_MaxFD == INVALID_SOCKET) ? fd : max(fd, m_MaxFD));

    //FD_SET(fd, &m_ReadFDs[SELECT_BAK]);
    //FD_SET(fd, &m_WriteFDs[SELECT_BAK]);
    //FD_SET(fd, &m_ExceptFDs[SELECT_BAK]);

    //m_nFDSize++;

    //XLOG("============= addSocket fd:%d ", fd);

    //m_lockClientConnect.lock();
    //m_mapClient[pProcess] = 0;
    //m_lockClientConnect.unlock();
}

void xSConnectManager::removeSocket(xNetProcessor* pProcess) {
    //if (!pProcess) {
    //    return;
    //}

    //// 杩欓噷鐩存帴鍒犻櫎
    //m_lockClientConnect.lock();
    //if (m_mapClient.find(pProcess) == m_mapClient.end()) {
    //    m_lockClientConnect.unlock();
    //    return;
    //}

    //m_mapClient.erase(pProcess);

    //SOCKET fd = pProcess->GetSock().get_fd();
    //if (fd == INVALID_SOCKET || m_MinFD == INVALID_SOCKET || m_MaxFD == INVALID_SOCKET) {
    //    m_lockClientConnect.unlock();
    //    return;
    //}

    //if (fd == m_MinFD) {
    //    SOCKET& s = m_MinFD;
    //    std::map<xNetProcessor*, int>::iterator it = m_mapClient.begin();
    //    for (; it != m_mapClient.end(); ++it) {
    //        xNetProcessor* pPlayer = it->first;
    //        if (!pPlayer) {
    //            m_lockClientConnect.unlock();
    //            return;
    //        }

    //        SOCKET temp = pPlayer->GetSock().get_fd();
    //        if (temp == fd)
    //            continue;
    //        if (temp == INVALID_SOCKET)
    //            continue;

    //        if (temp < s) {
    //            s = temp;
    //        }
    //    }

    //    if (m_MinFD == m_MaxFD && m_MaxFD == fd) {
    //        m_MinFD = m_MaxFD = INVALID_SOCKET;
    //    }
    //} else if (fd == m_MaxFD) {
    //    SOCKET& s = m_MaxFD;
    //    std::map<xNetProcessor*, int>::iterator it = m_mapClient.begin();
    //    for (; it != m_mapClient.end(); ++it) {
    //        xNetProcessor* pPlayer = it->first;
    //        if (!pPlayer) {
    //            m_lockClientConnect.unlock();
    //            return;
    //        }

    //        SOCKET temp = pPlayer->GetSock().get_fd();
    //        if (temp == fd)
    //            continue;
    //        if (temp == INVALID_SOCKET)
    //            continue;

    //        if (temp > s) {
    //            s = temp;
    //        }
    //    }

    //    if (m_MaxFD == m_MinFD && m_MaxFD == fd) {
    //        m_MinFD = m_MaxFD = INVALID_SOCKET;
    //    }
    //}

    //FD_CLR(fd, &m_ReadFDs[SELECT_BAK]);
    //FD_CLR(fd, &m_WriteFDs[SELECT_BAK]);
    //FD_CLR(fd, &m_ExceptFDs[SELECT_BAK]);

    ////XLOG("============= removeSocket fd:%d ", fd);

    //m_nFDSize--;

    //if (m_nFDSize <= 0) {
    //    m_MaxFD = INVALID_SOCKET;
    //}

    //m_lockClientConnect.unlock();
}

bool xSConnectManager::processNetwork() {

    //m_lockClientConnect.lock();

    //if (m_nFDSize <= 0) {
    //    m_lockClientConnect.unlock();
    //    return true;
    //}

    //{
    //    if (m_MaxFD == INVALID_SOCKET && m_MinFD == INVALID_SOCKET) {
    //        m_lockClientConnect.unlock();
    //        return true;
    //    }

    //    m_Timeout[SELECT_USE].tv_sec = m_Timeout[SELECT_BAK].tv_sec;
    //    m_Timeout[SELECT_USE].tv_usec = m_Timeout[SELECT_BAK].tv_usec;
    //    m_ReadFDs[SELECT_USE] = m_ReadFDs[SELECT_BAK];
    //    m_WriteFDs[SELECT_USE] = m_WriteFDs[SELECT_BAK];
    //    m_ExceptFDs[SELECT_USE] = m_ExceptFDs[SELECT_BAK];

    //    try {
    //        INT iRet = select((INT)m_MaxFD + 1,
    //            &m_ReadFDs[SELECT_USE],
    //            &m_WriteFDs[SELECT_USE],
    //            &m_ExceptFDs[SELECT_USE],
    //            &m_Timeout[SELECT_USE]);
    //        if (iRet == SOCKET_ERROR) {
    //            XERR("select ----- 111111 ");
    //            //                 getchar();
    //            //                 getchar();
    //            m_lockClientConnect.unlock();
    //            return false;
    //        }
    //    } catch (...) {
    //        XERR("select ----- 222222 ");
    //        //             getchar();
    //        //             getchar();
    //        m_lockClientConnect.unlock();
    //        return false;
    //    }
    //}

    //std::map<xNetProcessor*, int> tempMapC = m_mapClient;

    //m_lockClientConnect.unlock();

    //{
    //    std::map<xNetProcessor*, int>::iterator it = tempMapC.begin();
    //    for (; it != tempMapC.end();) {
    //        bool runSuccess = false;
    //        std::map<xNetProcessor*, int>::iterator it_tmp = it++;
    //        xNetProcessor* pNetProcess = it_tmp->first;

    //        do {
    //            if (!pNetProcess) {
    //                break;
    //            }

    //            SOCKET s = pNetProcess->GetSock().get_fd();
    //            if (s == SOCKET_ERROR) {
    //                break;
    //            }

    //            if (FD_ISSET(s, &m_ExceptFDs[SELECT_USE])) {
    //                XLOG("杩炴帴閿欒 %s %p", pNetProcess->m_szName, pNetProcess);
    //                break;
    //            }

    //            if (FD_ISSET(s, &m_ReadFDs[SELECT_USE])) {
    //                if (!pNetProcess->readCmd()) {
    //                    //XLOG("璇诲彇閿欒 %s %p", pNetProcess->m_szName, pNetProcess);
    //                    break;
    //                }

    //                unsigned char* cmd = 0;
    //                unsigned short cmdLen;
    //                bool process_ret = true;
    //                while (pNetProcess->getCmd(cmd, cmdLen)) {
    //                    process_ret = m_pZoneServer->processCmd(pNetProcess->m_u32SGUID, pNetProcess, cmd, cmdLen);
    //                    pNetProcess->popCmd();
    //                    if (!cmdLen || !process_ret) {
    //                        break;
    //                    }
    //                }
    //                if (cmdLen && !process_ret) {
    //                    xCommand* c = (xCommand*)cmd;
    //                    if (c) {
    //                        XERR("娑堟伅澶勭悊閿欒 %s %p (%u, %u),len:%u", pNetProcess->m_szName, pNetProcess, c->cmd, c->param, cmdLen);
    //                    } else {
    //                        XERR("娑堟伅澶勭悊閿欒 %s %p", pNetProcess->m_szName, pNetProcess);
    //                    }

    //                    break;
    //                }
    //            }

    //            runSuccess = true;

    //        } while (false);

    //        if (!runSuccess) {
    //            pNetProcess->setNPState(xNetProcessor::NP_DISCONNECT);
    //            m_pZoneServer->closeServer(pNetProcess);
    //        }
    //    }
    //}

    return true;
}

#else

void xSConnectManager::addEpoll(int fd) {
    epoll_event ev;
    bzero(&ev, sizeof(ev));
    ev.data.fd = fd;
    ev.data.ptr = this;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    epoll_ctl(server_epfd, EPOLL_CTL_ADD, fd, &ev);
#ifdef _LX_DEBUG
    XDBG("[娣诲姞Epoll],%s,epfd:%d, fd:%d", m_szName, m_epfd, fd);
#endif
}

//涓荤嚎绋?
bool xSConnectManager::processNetwork() {

    if (!server_epfd) {
        return false;
    }

    bzero(server_ev, sizeof(server_ev));
    int nfds = epoll_wait(server_epfd, server_ev, MAX_SERVER_EVENT, 50);

    for (int i = 0; i < nfds; ++i) {
        xNetProcessor* pNetProcess = (xNetProcessor*)server_ev[i].data.ptr;
        if (server_ev[i].events & EPOLLERR) {
            XLOG("杩炴帴閿欒 %s %p", pNetProcess->m_szName, pNetProcess);
            pNetProcess->setNPState(xNetProcessor::NP_DISCONNECT);
            m_pZoneServer->closeServer(pNetProcess);
            continue;
        } else if (server_ev[i].events & EPOLLIN) {
            if (!pNetProcess->readCmd()) {
                //XLOG("璇诲彇閿欒 %s %p", pNetProcess->m_szName, pNetProcess);
                pNetProcess->setNPState(xNetProcessor::NP_DISCONNECT);
                m_pZoneServer->closeServer(pNetProcess);
                continue;
            }

            unsigned char* cmd = 0;
            unsigned short cmdLen;
            bool process_ret = true;
            while (pNetProcess->getCmd(cmd, cmdLen)) {
                process_ret = m_pZoneServer->processCmd(pNetProcess->m_u32SGUID, pNetProcess, cmd, cmdLen);
                pNetProcess->popCmd();
                if (!cmdLen || !process_ret) {
                    break;
                }
            }
            if (cmdLen && !process_ret) {
                xCommand* client = (xCommand*)cmd;
                if (client) {
                    XERR("娑堟伅澶勭悊閿欒 %s %p (%u, %u),len:%u", pNetProcess->m_szName, pNetProcess, client->cmd, client->param, cmdLen);
                    //return true;
                } else {
                    XERR("娑堟伅澶勭悊閿欒 %s %p", pNetProcess->m_szName, pNetProcess);
                }

                pNetProcess->setNPState(xNetProcessor::NP_DISCONNECT);
                m_pZoneServer->closeServer(pNetProcess);
                continue;
            }
        }
    }

    return true;
}


#endif



