#include "xNetProcessor.h"

//xNetProcessor::xNetProcessor() {
//}
//
//void xNetProcessor::init(int fd, const char* n) {
//    m_stSock.init();
//
//    setNPState(NP_CREATE);
//
//    m_u64ID = 0;
//    setName(n);
//    m_epfd = 0;
//
//    bzero(&m_ip, sizeof(m_ip));
//    m_port = 0;
//
//    m_pUser = nullptr;
//}

void xNetProcessor::cleanup() {
    setNPState(NP_CREATE);

    m_u64ID = 0;
    setName("");
    m_epfd = 0;

    bzero(&m_ip, sizeof(m_ip));
    m_port = 0;
    m_pSock->cleanup();

    m_pUser = nullptr;
}

xNetProcessor::~xNetProcessor() {
}

void xNetProcessor::setName(const char* n) {
    bzero(m_szName, sizeof(m_szName));
    if (n) {
        strncpy(m_szName, n, MAX_NAMESIZE);
    }
}

void xNetProcessor::disconnect() {
    m_pSock->close();
    //XLOG("[Socket]%s disconnect %p", m_szName, this);
    setNPState(NP_CLOSE);
}

void xNetProcessor::addEpoll(int ep) {
#ifndef _WIN32
    m_epfd = ep;
    epoll_event ev;
    bzero(&ev, sizeof(ev));
    ev.data.fd = m_stSock.get_fd();
    ev.data.ptr = this;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    epoll_ctl(ep, EPOLL_CTL_ADD, m_stSock.get_fd(), &ev);
    setNPState(NP_VERIFIED);
#ifdef _LX_DEBUG
    XDBG("[婵烇綀顕ф慨婵玴oll],%s,epfd:%d, fd:%d", m_szName, m_epfd, sock.get_fd());
#endif
#else

#endif
}

void xNetProcessor::delEpoll() {
#ifndef _WIN32
    epoll_event ev;
    bzero(&ev, sizeof(ev));
    ev.data.fd = m_stSock.get_fd();
    ev.data.ptr = this;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    epoll_ctl(m_epfd, EPOLL_CTL_DEL, m_stSock.get_fd(), &ev);
#ifdef _LX_DEBUG
    XDBG("[闁告帞濞€濞呭鼎poll],%s,epfd:%d, fd:%d", m_szName, m_epfd, sock.get_fd());
#endif
#else

#endif
}

bool xNetProcessor::readCmd() {
    return m_pSock->readToBuf();
}

bool xNetProcessor::sendCmd() {
    return m_pSock->sendCmd();
}

bool xNetProcessor::sendCmd(const void* cmd, unsigned short len) {
    /*
#pragma pack(1)
    struct xCommand
    {
        UInt8 cmd = 0;
        UInt8 param = 0;
    };
    // 閻犱警鍨抽弫杈╀沪閸屾粓鐛?
    struct RouteInfo
    {
        // 闁告瑯鍨禍鎺旀崉椤栨粍鏆犻柣銊ュ閻壆鐥?婵絽绻楃换鍐╃▔閳ь剚绋夐鍛疀闁告柡鈧櫕鐝ら柛鎴濈箰閻? 闂侇剙鐏濋崢銈咁潰鐠囧弶鍎曢柣婊庡灟缁卞爼鏌?
        // 濠㈠爢鍌滆壘0閻炴稏鍔庨妵姘跺矗椤栨瑤绨扮紓浣堝懐鏁惧ù鑲╁█閳?缂佹稑顦花?闁汇劌瀚鍌炲磹?闁哄牆绉存慨鐔煎闯閵娿儳娈哄☉鎾卞灩缁辨梻鎷犻妷褏绉圭紓浣圭矊鐎?
        char cNodeLevel = 2;
        // 闁哄嫷鍨伴幆渚€宕ｉ幋锔瑰亾娴ｈ櫣鑸跺ù婊冪灱妤犲洭鎳為崒婊冧化
        char cIsToChiled = 0;
        // 闁哄嫷鍨伴幆渚€寮伴姘辩暛闁?濞撴碍绻冨畵渚€濡撮幁鐖刣 + cIsToChiled闁靛棙鍨跺鍨交濞戞粠鏀介弶鍫涘妼閸?
        char cIsBroadCast = 0;
        // 濞戞挸锕ｇ粩瀵哥棯瑜忓▓?SGUID 闁活潿鍔嬬花顒勭嵁閹稿孩灏￠柟鐑樺浮濞?濞戞挸绉撮崯鈧鐐村閹歌京绱掑▎搴ｇ憪缂佺嫏鍕疀闁告柡鈧櫕鐝?
        UInt32 stPreSGUID[2];
        // 婵犙勨偓鐩橴ID
        UInt32 stSrcSGUID;
        // 闁烩晩鍠楅悥顤橤UID
        UInt32 stTargetSGUID;

        RouteInfo() {}
    };
    struct xServerDispatcherMsg
    {
        xCommand cmdID;
        // 闁荤偛妫涢弫杈ㄧ┍閳╁啩绱?
        RouteInfo stRouteInfo;
        // 閺夆晜鐟╅崳鐑藉及閻も暜otoBuff String 闁轰胶澧楀畵?
        UInt32 msg_len = 0;
        char msg_data[0];// proto -闁?tostring

        void Init(UInt8 cmd, UInt8 param) {
            cmdID.cmd = cmd;
            cmdID.param = param;
        }
        void SetProtoData(char *buff, UInt32 l) {
            msg_len = l;
            memcpy(msg_data, buff, msg_len);
        }
        UInt32 GetTotalLen() { return msg_len + sizeof(xServerDispatcherMsg); }
        char *GetBuffer() { return (char*)this; }
        char *GetMsgBuffer() { return msg_data; }
    };
#define MSG_DISPATCHER_CMD 201
#pragma pack()

    xCommand *cmdID = (xCommand *)cmd;
    if (cmdID->cmd == MSG_DISPATCHER_CMD) {
        xServerDispatcherMsg *pMsg = (xServerDispatcherMsg*)cmd;
        cmdID = (xCommand *)pMsg->GetMsgBuffer();
    }


    XLOG("[[[[[[[[[[[[[[[[[ xNetProcessor::sendCmd - cmd:%d,param:%d ]]]]]]]]]]]]]]]]]] ", cmdID->cmd, cmdID->param);

    if (cmdID->cmd == 103 && cmdID->param == 9) {
        XLOG("11111112312");
    }

    */

    if (getNPState() == NP_CLOSE || NP_DISCONNECT == getNPState()) return false;
    if (!cmd || !len) return false;
#ifndef _WUWENJUAN_DEBUG
#ifndef _LX_DEBUG
#ifndef _TLBJ_DEBUG
    //XDBG("sendCmd %p state:%u %p %u", this,getState(), cmd, len);
#endif
#endif
#endif

    m_lockSendMsg.lock();
    if (!m_pSock->sendCmd(cmd, len)) {
        XERR("sendCmd failed %p", this);
        //post_disconnect();
        m_lockSendMsg.unlock();
        return false;
    }
    /*
    else
    {
        struct epoll_event ev;
        ev.data.fd = sock.get_fd();
        ev.events = EPOLLIN|EPOLLOUT|EPOLLET;
        ev.data.ptr = this;
        epoll_ctl(epfd, EPOLL_CTL_MOD, sock.get_fd(), &ev);
    }
    */
    m_lockSendMsg.unlock();

    return true;
}

bool xNetProcessor::getCmd(unsigned char*& cmd, unsigned short& len) {
    return m_pSock->getCmd(cmd, len);
}

bool xNetProcessor::popCmd() {
    return m_pSock->popCmd();
}



