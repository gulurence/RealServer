

#include "xSocket.h"
#include "../xBase/xTools.h"

#include <zlib.h>
extern "C" {
    #include "des/d3des.h"
}

#include <errno.h>
#include <fcntl.h>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/tcp.h>
#endif


#define COMPRESS_LEVEL 6
#define MIN_COMPRESS_SIZE 48

uint8 fixedkey[8] = { 95, 27, 5, 20, 131, 4, 8, 88 };

xSocket::xSocket(tcp_socket& stSocket):m_stSocket(stSocket) {
}

xSocket::~xSocket() {
    close();
}

void xSocket::init() {

    //writeBuffer.init("writeBuffer", sock);
    //readBuffer.init("readBuffer", sock);
    //encBuffer.init("encBuffer", sock);
    //tmpWriteBuffer.init("tmpWriteBuffer", sock);
    //tmpDecBuffer.init("tmpDecBuffer", sock);
    //cmdBuffer.init("cmdBuffer", sock);

    unCompCmdRealSize = 0;
    encFlag = false;
    compFlag = false;
    sendLen = 0;

    setSockOpt();
}

void xSocket::cleanup() {
    //fd = 0;
    //writeBuffer.cleanup();
    //readBuffer.cleanup();
    //encBuffer.cleanup();
    //tmpWriteBuffer.cleanup();
    //tmpDecBuffer.cleanup();
    //cmdBuffer.cleanup();
}

void xSocket::close() {
    //if (valid()) {
    //    SAFE_CLOSE_SOCKET(fd);
    ///    fd = -1;
    //}
}

void xSocket::shutdown(int how) {
    //if (valid()) {
    //    //XDBG("[Socket]shutdown %d", fd);
    //    ::shutdown(fd, how);
    //    fd = -1;
    //}
}

bool xSocket::setNonBlock() {
#ifndef _WIN32
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    if (-1 == fcntl(fd, F_SETFL, flags)) {
        XERR("[Socket]setNonBlock failed %d", fd);
        return false;
    }
#else
    // 閺夆晜鐟╅崳鐑芥閳ь剛鎲版担宄扳枏闁活潿鍔戝顏堟⒓鐠囧樊鏁氶柡鍫濇惈閸?
    //ULONG argp = 1;
    //if (ioctlsocket(fd, FIONBIO, &argp) == SOCKET_ERROR) {
    //    //INT iErr = WSAGetLastError() ;
    //    return false;
    //}
#endif

    int on = 1;
    /* make socket here */
//#ifdef WIN32
//    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const char*)&on, sizeof(on));
//    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&on, sizeof(on));
//#else
//    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void*)&on, sizeof(on));
//    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*)&on, sizeof(on));
//#endif

    return true;
}

bool xSocket::setSockOpt() {
    //int nRecvBuf = MAX_BUFSIZE * 2;
    //setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const char*)&nRecvBuf, sizeof(int));
    //int nSendBuf = MAX_BUFSIZE * 2;
    //setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (const char*)&nSendBuf, sizeof(int));
    return true;
}

uint16 xSocket::sizeMod8(uint16 len) {
    return len;
    return (len + 7) / 8 * 8;
}

bool xSocket::readToBuf() {
    bool final_ret = true;

    /*
    while (1) {
        int max = readBuffer.getLeft();
        if (max < MAX_BUFSIZE) {
            readBuffer.resize(readBuffer.size() + MAX_BUFSIZE);
            max = readBuffer.getLeft();
        }

        int ret = ::recv(fd, (char*)readBuffer.getBufOffset(), max, 0);
        if (ret < 0) {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                break;
            } else {
                final_ret = false;
                break;
            }
#else
            if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                final_ret = false;

                //                 XDBG("[SOCKET]1闁规亽鍎查弫褰掓煥濞嗘帩鍤?%u %u %u",
                //                     ((Packet *)readBuffer.getBufBegin())->ph.len,
                //                     ((Packet *)readBuffer.getBufBegin())->data[0],
                //                     ((Packet *)readBuffer.getBufBegin())->data[1]);
                //                 XERR("[SOCKET]闁规亽鍎查弫褰掓煥濞嗘帩鍤?errno:%u,%s", errno, strerror(errno));
            }
            //XERR("[SOCKET]闁规亽鍎查弫褰掓煥濞嗘帩鍤?errno:%u,%s", errno, strerror(errno));
            break;
#endif

        } else if (0 == ret) //peer shutdown
        {
            final_ret = false;
            //             XDBG("[SOCKET]2闁规亽鍎查弫褰掓煥濞嗘帩鍤?%u %u %u",
            //                     ((Packet *) readBuffer.getBufBegin())->ph.len,
            //                     ((Packet *) readBuffer.getBufBegin())->data[0],
            //                     ((Packet *) readBuffer.getBufBegin())->data[1]);
            //             XERR("[SOCKET]闁规亽鍎查弫褰掓煥濞嗘帩鍤?errno:%u,%s", errno, strerror(errno));

            break;

        } else {

            readBuffer.put(ret);
            //XDBG("[SOCKET]闁衡偓鐠哄搫鐓傞柡浣哄瀹?%u %u %u", ((Packet *)readBuffer.getBufBegin())->ph.len, ((Packet *)readBuffer.getBufBegin())->data[0], ((Packet *)readBuffer.getBufBegin())->data[1]);
            //bufToCmd();
        }


        //printf(" yyyyyyyyyyyyyyyyyy recv msg len:%d t:%d\n", ret, (UInt32)time(nullptr));
    }

    return final_ret;*/
    return true;
}

bool xSocket::sendCmd() {
    /*
    if (!valid())
        return false;
    if (!writeBuffer.getUsed())
        return false;

    bool final_ret = true;

    int all = writeBuffer.getUsed();

    int ret = ::send(fd, (char*)writeBuffer.getBufBegin(), all, 0);
    if (ret < 0) {
        if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
            //XERR("[SOCKET]闁告瑦鍨块埀顑跨窔閺佸﹦鎷?fd:%d,errno:%u,%s", fd, errno, strerror(errno));
            final_ret = false;
        }
    } else {
        writeBuffer.pop(ret);
        sendLen = ret;
    }

    //printf(" xxxxxxxxxxxxxxxxxxxxxxxxx send msg len:%d t:%d\n", ret, (UInt32)time(nullptr));

    return final_ret;*/
    return true;
}
/*
 * 闁告瑦鍨块埀顑跨劍缁夌兘骞?
 * param data 闁告瑦鍨块埀顑胯兌濞堟垿寮悧鍫濈ウ
 * param len  闁轰胶澧楀畵渚€姊归崹顔碱唺
 */
bool xSocket::sendCmd(const void* data, uint16 len) {
    
    //if (!valid())
    //    return false;
    //encBuffer.reset();            //闂佹彃绉堕悿鍡欑磽閹惧啿鏆遍柛?
    //PacketHead ph;            //闁告绻楅鍛緞?
    //encBuffer.put(&ph, PH_LEN);            //閻忓繋娴囬ˉ濠冨緞?閻庢稒顨夋俊?
    //encBuffer.put(data, len);            //闁轰胶澧楀畵渚€姊归崹顔碱唺
    //PacketHead* p = (PacketHead*)encBuffer.getBufBegin();
    //uint16 real_size = len;

    //if (compFlag/* && len >= MIN_COMPRESS_SIZE*/) {
    //    real_size = compress(encBuffer.getBufOffset(PH_LEN), len);        //闁轰胶澧楀畵渚€宕㈢€ｎ剛绱?
    //    if (real_size > 0)
    //        p->flags |= PACKET_FLAG_COMPRESS;            //闁轰胶澧楀畵浣规交濞戞粠鏀介柛妯侯儑缂傚鏌呮径鎰┾偓?
    //    else
    //        real_size = len;
    //    //XLOG("[闁告ê顑囩紓濉? %u -> %u", len, real_size);
    //} else {
    //    //XLOG("[闁告ê顑囩紓濉? %u", len);
    //}
    //p->len = real_size;
    //real_size += PH_LEN;
    //if (needEnc()) {
    //    p->flags |= PACKET_FLAG_ENCRYPT;
    //    real_size = sizeMod8(real_size);
    //    encrypt(encBuffer.getBufBegin(), real_size);
    //}

    //if (!writeToBuf(encBuffer.getBufBegin(), real_size))
    //    return false;
    //return sendCmd();
    return true;
}

bool xSocket::writeToBuf(void* data, uint32 len) {
    //uint32 real_size = writeBuffer.put(data, len);

    //return real_size == len;
    return true;
}
/**
 * 闁告ê顑囩紓澶愬极閻楀牆绁?
 * param data 闁轰胶澧楀畵?
 * len 闁轰胶澧楀畵浣衡偓娑欘殙婵☆參姊归崹顔碱唺
 */
uint16 xSocket::compress(void* data, uint64 len) {
    //uint64 newLen = tmpWriteBuffer.size();
    //if (Z_OK == ::compress2((Bytef*)tmpWriteBuffer.getBufBegin(), (uLongf*)&newLen, (Bytef*)data, (uLong)len, COMPRESS_LEVEL)) {
    //    memcpy(data, tmpWriteBuffer.getBufBegin(), newLen);
    //    return newLen;
    //}

    //XLOG("[xSocket],fd:%d,闁告ê顑囩紓澶愭煥濞嗘帩鍤?, fd);
    return 0;
}

/*
 * 閻熸瑱绲界敮鍥╃磽閳哄倹娈堕柟?
 *
 */
uint16 xSocket::uncompress(void* dest, uint64 destLen, void* src, uint64 srcLen) {
    uint64 newLen = destLen;
    if (Z_OK == ::uncompress((Bytef*)dest, (uLongf*)&newLen, (Bytef*)src, (uLong)srcLen)) {
        return newLen;
    }

    return 0;
}

void deskey(unsigned char*, short);
void des(unsigned char*, unsigned char*);
void xSocket::encrypt(void* data, uint16 len) {
    //deskey((unsigned char*)fixedkey, EN0);
    //for (uint16 i = 0; i < len; i += 8)
    //    des((unsigned char*)(uint8*)data + i, (unsigned char*)(uint8*)tmpWriteBuffer.getBufOffset(i));
    //memcpy(data, tmpWriteBuffer.getBufBegin(), len);
}

void xSocket::decrypt(void* data, uint16 len) {
    //deskey((unsigned char*)fixedkey, DE1);
    //for (uint16 i = 0; i < len; i += 8)
    //    des((unsigned char*)(uint8*)data + i, (unsigned char*)(uint8*)tmpDecBuffer.getBufOffset(i));
    //memcpy(data, tmpDecBuffer.getBufBegin(), len);
}

bool xSocket::getCmd(uint8*& cmd, uint16& len) {
    //return cmdQueue.getCmd(cmd, len);

    //cmd = 0;
    //len = 0;

    //uint32 used = readBuffer.getUsed();
    //if (used < MIN_PACKSIZE) {
    //    return false;
    //}
    //void* buf = readBuffer.getBufBegin();
    //Packet* p = (Packet*)buf;

    //uint16 real_size = 0;

    //if (needDec()) {
    //    uint8 copy[8];
    //    memcpy(copy, (uint8*)readBuffer.getBufBegin(), 8);
    //    decrypt(copy, 8);
    //    real_size = ((Packet*)copy)->getFullSize();
    //} else {
    //    real_size = p->getFullSize();
    //}

    //if (real_size > used) {
    //    return false;
    //}

    //if (needDec()) {
    //    decrypt(p, real_size);
    //}

    //unCompCmdRealSize = real_size;

    //if (p->ph.flags & PACKET_FLAG_COMPRESS) {
    //    len = uncompress(cmdBuffer.getBufBegin(), cmdBuffer.size(), p->data, p->ph.len);
    //    if (len > 0) {
    //        cmd = (uint8*)cmdBuffer.getBufBegin();
    //    }
    //    //else
    //    //XLOG("[xSocket],fd:%d,閻熸瑱绲界敮鍥煥濞嗘帩鍤?婵炴垵鐗婃导鍛存⒐閸喖顔?%u", fd, p->ph.len);
    //} else {
    //    cmd = p->data;
    //    len = p->ph.len;
    //}

    return true;
}

bool xSocket::popCmd() {
    //if (unCompCmdRealSize >= MIN_PACKSIZE
    //    && unCompCmdRealSize <= readBuffer.getUsed()) {
    //    if (readBuffer.pop(unCompCmdRealSize)) {
    //        unCompCmdRealSize = 0;
    //        return true;
    //    }
    //}

    return false;
}




