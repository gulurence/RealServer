
#include "xNetAgent.h"

xNetAgent::xNetAgent(const NetAgentConfigST &stConfig)
{
    m_stConfig = stConfig;
    //
    XINF("Create Service Net [%s]", m_stConfig.strServiceName.c_str());
}

xNetAgent::~xNetAgent()
{
}

void xNetAgent::Release()
{
}

void PackageCheck(void **pParam, tcp_socket *socket, uint64 *u64CID, NetPackageCallPtr pPackageCall,
                  xCircularBuffer *pCircularBuffer, std::atomic<bool> *bIsExit)
{
    while (!(*bIsExit))
    {
        auto packageFullSize = pCircularBuffer->HaveFullPackage();
        if (packageFullSize > 0)
        {
            NetPackageSharedPtr pPackagePtr = std::make_shared<NetPackageST>(packageFullSize);
            pCircularBuffer->Read(pPackagePtr->pPackageData, packageFullSize);
            pCircularBuffer->Put(packageFullSize);
            if (pPackageCall)
            {
                pPackageCall(*pParam, *socket, *u64CID, pPackagePtr);
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}

awaitable<void> xNetAgent::Accept(tcp_socket socket)
{
    // 用 shared_ptr 持有 socket, 以便注册进上层发送通道 (SendToCID)
    auto sharedSocket = std::make_shared<tcp_socket>(std::move(socket));
    NetCID u64CID = 0;
    if (m_stConfig.pAcceptCall)
    {
        std::atomic<bool> bIsExit = false;
        xCircularBuffer *pCircularBuffer = new xCircularBuffer(m_stConfig.u16CacheBuffSize);
        // 0.获取新连接ID
        u64CID = m_stConfig.pAcceptCall(m_stConfig.pParam, *sharedSocket);
        XINF("Accept client u64CID [%ld] !!!", u64CID);
        // 0.5 注册 socket 供发送通道使用
        if (m_stConfig.pSocketReady)
        {
            m_stConfig.pSocketReady(m_stConfig.pParam, u64CID, sharedSocket);
        }
        // 1.开启解包线程
        std::thread t(PackageCheck, &(m_stConfig.pParam), sharedSocket.get(), &u64CID,
                      m_stConfig.pPackageCall, pCircularBuffer, &bIsExit);
        //
        // 2.监听 接收数据
        for (;;)
        {
            auto [error, nread]
                = co_await sharedSocket->async_read_some(pCircularBuffer->GetWriteBuffer());
            if (error)
            {
                if (m_stConfig.pDisconnect)
                {
                    m_stConfig.pDisconnect(m_stConfig.pParam, u64CID);
                }
                break;
            }
            else
            {
                if (nread == 0)
                {
                    // 没有读到数据, 连接已关闭
                    if (m_stConfig.pDisconnect)
                    {
                        m_stConfig.pDisconnect(m_stConfig.pParam, u64CID);
                    }
                    break;
                }
                else
                {
                    pCircularBuffer->Write(nread);
                }
            }
        }
        bIsExit = true;
        t.join();
        // 注销 socket
        if (m_stConfig.pSocketReady)
        {
            m_stConfig.pSocketReady(m_stConfig.pParam, u64CID, nullptr);
        }
        delete pCircularBuffer;
        pCircularBuffer = nullptr;
        XINF("Client disconnect cid [%ld].", u64CID);
    }
}

awaitable<void> xNetAgent::Listener(uint16 u16Port)
{
    auto executor = co_await this_coro::executor;
    tcp_acceptor acceptor(executor, {tcp::v4(), u16Port});
    for (;;)
    {
        if (auto [e, socket] = co_await acceptor.async_accept(); socket.is_open())
        {
            co_spawn(executor, Accept(std::move(socket)), detached);
        }
    }
}

void xNetAgent::thread_proc()
{
    try
    {
        asio::io_context io_context(1);
        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) { io_context.stop(); });
        co_spawn(io_context, Listener(m_stConfig.u16Port), detached);
        io_context.run();
    }
    catch (std::exception &e)
    {
        XERR("xService::thread_proc Exception: %s", e.what());
    }
}
