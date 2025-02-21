#pragma once

#include "xNetDefine.h"



class xCircularBuffer
{
public:
    xCircularBuffer(const std::size_t& size);
    ~xCircularBuffer();

public:
    boost::asio::mutable_buffer GetWriteBuffer();
    void Write(const std::size_t& size);

public:
    int Read(uint8* out, const std::size_t& size);
    bool Put(const std::size_t& size);

private:
    void Resize(const std::size_t& size);
    bool IsFull() const {
        return size_.load(std::memory_order_relaxed) == max_size_;
    }
    std::size_t Size() const {
        return size_.load(std::memory_order_relaxed);
    }

public:
    uint16 HaveFullPackage();

public:
    void Reset() {
        head_ = 0;
        tail_ = 0;
    }

private:
    bool m_bNeedBody = true;
    NetPackageHeadST m_stNetPackageHead;

private:
    std::atomic<std::size_t> head_;       // The head of the buffer
    std::atomic<std::size_t> tail_;       // The tail of the buffer
    std::atomic<std::size_t> size_;       // The current size of the buffer
    std::size_t max_size_;                // The maximum capacity of the buffer
    char* buffer_;                        // The underlying buffer
};

