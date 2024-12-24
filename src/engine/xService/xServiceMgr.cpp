
#include "xServiceMgr.h"

#include "xBase/xXMLParser.h"
#include "../xSConnect/xTask.h"
#include "../xSConnect/xClient.h"


xServiceMgr::xServiceMgr(const char* pszServiceName, uint16 u16Port) {
    m_u16Port = u16Port;
    //
    bzero(m_pszServiceName, sizeof(m_pszServiceName));
    strncpy(m_pszServiceName, pszServiceName, MAX_NAMESIZE - 1);
    //
    SetServiceState(XSERVICE_CREATE);
    //
    XINF("xServiceMgr::xServiceMgr Create [%s]", pszServiceName);
}

xServiceMgr::~xServiceMgr() {

}

void xServiceMgr::Release() {

    xXMLParser::clearSystem();
}


#include <iostream>
#include <boost/asio.hpp>
#include <vector>

class CircularBuffer
{
public:
    CircularBuffer(std::size_t size)
        : head_(0), tail_(0) {
        buffer_ = new std::vector<char>();
        buffer_->reserve(size);
    }

public:
    std::size_t size() const {
        return size_;
    }

    // 获取可写空间的缓冲区
    boost::asio::mutable_buffer get_write_buffer() {
        if (is_full()) {
            resize(buffer_->capacity() * 2);
        }
        if (head_ <= tail_) {
            // 空间从 tail 到 head
            std::size_t available = buffer_->capacity() - tail_;
            return boost::asio::mutable_buffer(&(*buffer_)[tail_], available);
        } else {
            // 空间从 head 到 tail
            return boost::asio::mutable_buffer(&(*buffer_)[tail_], head_- tail_);
        }
    }

    // 写入数据
    void write(std::size_t size) {
        size_ += size;
        tail_ = (tail_ + size)% buffer_->capacity();
        if (size_ >= buffer_->capacity()) {
            resize(buffer_->capacity() * 2);
        }
    }

    // 读取数据
    void read(char *out, std::size_t size) {
        size_ += size;
        tail_ = (tail_ + size) % buffer_->capacity();
        if (size_ >= buffer_->capacity()) {
            resize(buffer_->capacity() * 2);
        }
    }

public:
    void resize(std::size_t size) {
        std::vector<char>* tbuffer_ = new std::vector<char>();
        tbuffer_->reserve(size);
        if (tail_ < head_) {
            bcopy(&((*buffer_)[head_]), &((*tbuffer_)[0]), buffer_->capacity() - head_);
            bcopy(&((*buffer_)[0]), &((*tbuffer_)[buffer_->capacity() - head_]), tail_);
        } else if (tail_ > head_) {
            bcopy(&((*buffer_)[head_]), &((*tbuffer_)[0]), tail_- head_);
        } else {
            if (tail_ == 0) {
                bcopy(&((*buffer_)[head_]), &((*tbuffer_)[0]), size_);
            } else {
                bcopy(&((*buffer_)[head_]), &((*tbuffer_)[0]), buffer_->capacity() - head_);
                bcopy(&((*buffer_)[0]), &((*tbuffer_)[buffer_->capacity() - head_]), tail_);
            }
        }
        head_ = 0;
        tail_ = size_;
        delete buffer_;
        buffer_ = tbuffer_;
    }

    bool is_full() const {
        return size_ >= buffer_->capacity();
    }

    void reset() {
        head_ = 0;
        tail_ = 0;
    }

private:
    std::vector<char> *buffer_;
    std::size_t head_;
    std::size_t tail_;
    std::size_t size_;
};

bool read_data(tcp_socket& stSocket, CircularBuffer& circular_buffer) {
    bool bSuccess = true;
    auto write_buffer = circular_buffer.get_write_buffer();
    if (write_buffer.size() > 0) {
        stSocket.async_read_some(write_buffer,
            [&stSocket, &circular_buffer,&bSuccess](const boost::system::error_code& error, std::size_t bytes_transferred) {
                if (error) {
                    // 如果读取发生错误，打印错误信息并做进一步处理
                    std::cerr << "Error during read: " << error.message() << std::endl;
                    if (error == boost::asio::error::not_connected) {
                        // 如果连接被关闭，可以做相应的处理
                        std::cerr << "Socket is not connected!" << std::endl;
                    }
                    bSuccess = false;
                } else {
                    // 成功读取数据
                    if (bytes_transferred == 0) {
                        // 如果没有读取到数据，表明连接可能已经关闭
                        std::cerr << "No data read, connection might be closed!" << std::endl;
                        bSuccess = false;
                    } else {
                        // 处理读取到的数据
                        std::cout << "Read " << bytes_transferred << " bytes." << std::endl;
                        // 正常处理
                        circular_buffer.write(bytes_transferred);
                    }
                }
                // 检查是否连接
                if (stSocket.is_open() && error != boost::asio::error::not_connected) {
                    // 执行某些处理，可能是继续读取数据
                    bSuccess = false;
                } else {
                    // 如果连接已经关闭，清理工作或停止进一步的读取
                    std::cerr << "Socket is closed or not connected!" << std::endl;
                    bSuccess = false;
                }
                //if (!error) {
                //    // 数据成功读取到环形缓冲区
                //    circular_buffer.write(bytes_transferred);
                //    std::cout << "Read " << bytes_transferred << " bytes into circular buffer.\n";
                //} else {
                //    std::cerr << "Error reading data: " << error.message() << std::endl;
                //}
            });
    } else {
        std::cerr << "No space available in circular buffer to write data.\n";
    }
    return bSuccess;
}


//awaitable<void> ClientAccept(tcp_socket stSocket) {
//    /*
//    
//    client p = new client(stSocket);
//    
//    // net -> 
//    p->ReciveMsg();
//    // logic ->
//    p->OnMsgProcess();
//    
//    */
//    xNetProcessor* pProcess = new xNetProcessor(new xSocket(stSocket));
//    CircularBuffer *circular_buffer = new CircularBuffer(1024);
//
//    char data[1024];
//    for (;;) {
//
//        if (!read_data(stSocket, *circular_buffer)) {
//            break;
//        }
//
//        // 异步读取数据
//        //stSocket.async_read_some(boost::asio::buffer(data),
//        //    [&stSocket](const boost::system::error_code& e1, std::size_t nread) {
//        //        if (e1) {
//        //            // 如果读取发生错误，打印错误信息并做进一步处理
//        //            std::cerr << "Error during read: " << e1.message() << std::endl;
//        //            if (e1 == boost::asio::error::not_connected) {
//        //                // 如果连接被关闭，可以做相应的处理
//        //                std::cerr << "Socket is not connected!" << std::endl;
//        //            }
//        //        } else {
//        //            // 成功读取数据
//        //            if (nread == 0) {
//        //                // 如果没有读取到数据，表明连接可能已经关闭
//        //                std::cerr << "No data read, connection might be closed!" << std::endl;
//        //            } else {
//        //                // 处理读取到的数据
//        //                std::cout << "Read " << nread << " bytes." << std::endl;
//        //            }
//        //        }
//        //        // 检查是否连接
//        //        if (stSocket.is_open() && e1 != boost::asio::error::not_connected) {
//        //            // 执行某些处理，可能是继续读取数据
//        //        } else {
//        //            // 如果连接已经关闭，清理工作或停止进一步的读取
//        //            std::cerr << "Socket is closed or not connected!" << std::endl;
//        //        }
//        //    }
//        //);
//    }
//
//    std::cerr << "Client disconnect.\n";
//    return;
//}

//awaitable<void> ClientListener(uint16 u16Port) {
//    auto executor = co_await this_coro::executor;
//    tcp_acceptor acceptor(executor, { tcp::v4(), u16Port});
//    for (;;) {
//        if (auto [e, socket] = co_await acceptor.async_accept(); socket.is_open())
//            co_spawn(executor, ClientAccept(std::move(socket)), detached);
//    }
//}


void xServiceMgr::thread_proc() {
    //try {
    //    boost::asio::io_context io_context(1);
    //    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    //    signals.async_wait([&](auto, auto) { io_context.stop(); });
    //    co_spawn(io_context, ClientListener(m_u16Port), detached);
    //    io_context.run();
    //} catch (std::exception& e) {
    //    XERR("xServiceMgr::thread_proc Exception: %s", e.what());
    //}
}
