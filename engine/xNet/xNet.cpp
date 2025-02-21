
#include "xNet.h"


xNet::xNet(const char* pszServiceName, uint16 u16Port, NetPackageAcceptPtr pAcceptCall, NetPackageCallPtr pPackageCall, uint16 u16CacheBuffSize) {
    m_u16Port = u16Port;
    m_pAcceptCall = pAcceptCall;
    m_pPackageCall = pPackageCall;
    m_u16CacheBuffSize = u16CacheBuffSize;
    bzero(m_pszServiceName, sizeof(m_pszServiceName));
    strncpy(m_pszServiceName, pszServiceName, MAX_NAMESIZE - 1);
    //
    XINF("Create Service Net [%s]", pszServiceName);
}

xNet::~xNet() {
}

void xNet::Release() {
}

void PackageCheck(tcp_socket* socket, uint64* u64CID, NetPackageCallPtr pPackageCall, xCircularBuffer* pCircularBuffer, std::atomic<bool>* bIsExit) {
    while (!(*bIsExit)) {
        auto packageFullSize = pCircularBuffer->HaveFullPackage();
        if (packageFullSize > 0) {
            NetPackageSharedPtr pPackagePtr = std::make_shared<NetPackageST>(packageFullSize);
            pCircularBuffer->Read(pPackagePtr->pPackageData, packageFullSize);
            pCircularBuffer->Put(packageFullSize);
            if (pPackageCall) {
                pPackageCall(*socket, *u64CID, pPackagePtr);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}

awaitable<void> xNet::Accept(tcp_socket socket) {
    uint64 u64CID = 0;
    if (m_pAcceptCall) {
        std::atomic<bool> bIsExit = false;
        xCircularBuffer* pCircularBuffer = new xCircularBuffer(m_u16CacheBuffSize);
        // 0.获取新连接ID
        u64CID = m_pAcceptCall(socket);
        XINF("Accept client u64CID [%ld] !!!", u64CID);
        // 1.开启解包线程
        std::thread t(PackageCheck, &socket, &u64CID, m_pPackageCall, pCircularBuffer, &bIsExit);
        //
        NetPackageHeadST stPackageHead;
        // 2.监听 接收数据
        for (;;) {


            //auto [error, nread] = co_await async_read(socket,boost::asio::mutable_buffer(&stPackageHead, NetPackageHeadSize));

            auto [error, nread] = co_await socket.async_read_some(pCircularBuffer->GetWriteBuffer());
            if (error) {
                // 如果读取发生错误，打印错误信息并做进一步处理
                std::cerr << "Error during read: " << error.message() << std::endl;
                if (error == boost::asio::error::not_connected) {
                    // 如果连接被关闭，可以做相应的处理
                    std::cerr << "Socket is not connected!" << std::endl;
                }
                break;
            } else {
                // 成功读取数据
                if (nread == 0) {
                    // 如果没有读取到数据，表明连接可能已经关闭
                    std::cerr << "No data read, connection might be closed!" << std::endl;
                    break;
                } else {
                    // 处理读取到的数据
                    //std::cout << "Read " << nread << " bytes." << std::endl;
                    pCircularBuffer->Write(nread);
                }
            }
        }
        bIsExit = true;
        t.join();
        delete pCircularBuffer;
        pCircularBuffer = nullptr;
        std::cerr << "Client disconnect.\n";
    }
}

awaitable<void> xNet::Listener(uint16 u16Port) {
    auto executor = co_await this_coro::executor;
    tcp_acceptor acceptor(executor, { tcp::v4(), u16Port });
    for (;;) {
        if (auto [e, socket] = co_await acceptor.async_accept(); socket.is_open()) {
            co_spawn(executor, Accept(std::move(socket)), detached);
        }
    }
}

void xNet::thread_proc() {
    try {
        boost::asio::io_context io_context(1);
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) { io_context.stop(); });
        co_spawn(io_context, Listener(m_u16Port), detached);
        io_context.run();
    } catch (std::exception& e) {
        XERR("xService::thread_proc Exception: %s", e.what());
    }
}
