
#include "xCircularBuffer.h"


xCircularBuffer::xCircularBuffer(const std::size_t& size)
    : head_(0), tail_(0), size_(0), max_size_(size) {
    buffer_ = new char[max_size_];
    std::memset(buffer_, 0, max_size_);
}

xCircularBuffer::~xCircularBuffer() {
    if (buffer_) {
        delete[] buffer_;
        buffer_ = nullptr;
    }
}

boost::asio::mutable_buffer xCircularBuffer::GetWriteBuffer() {
    if (IsFull()) {
        XINF("GetWriteBuffer IsFull");
        Resize(max_size_ * 2);
    }
    std::size_t tail = tail_.load(std::memory_order_relaxed);
    std::size_t head = head_.load(std::memory_order_relaxed);

    if (head <= tail) {
        std::size_t available = max_size_ - tail;
        return boost::asio::mutable_buffer(&buffer_[tail], available);
    } else {
        return boost::asio::mutable_buffer(&buffer_[tail], head - tail);
    }
}

void xCircularBuffer::Write(const std::size_t& size) {
    std::size_t new_tail = (tail_.load(std::memory_order_relaxed) + size) % max_size_;
    std::size_t new_size = size_.load(std::memory_order_relaxed) + size;

    size_.store(new_size, std::memory_order_release);
    tail_.store(new_tail, std::memory_order_release);

    if (new_size >= max_size_) {
        Resize(new_size * 2);
    }
}

int xCircularBuffer::Read(uint8* out, const std::size_t& size) {
    std::size_t head = head_.load(std::memory_order_acquire);
    std::size_t tail = tail_.load(std::memory_order_acquire);

    if (Size() < size) {
        return 0;
    }

    if (tail < head) {
        if (size <= max_size_ - head) {
            std::memcpy(&(out[0]), &(buffer_[head]), size);
        } else {
            std::memcpy(&(out[0]), &(buffer_[head]), max_size_ - head);
            std::memcpy(&(out[max_size_ - head]), &(buffer_[0]), size - (max_size_ - head));
        }
    } else if (tail > head) {
        std::memcpy(&(out[0]), &(buffer_[head]), size);
    } else {
        if (tail == 0) {
            std::memcpy(&(out[0]), &(buffer_[head]), size);
        } else {
            if (size <= max_size_ - head) {
                std::memcpy(&(out[0]), &(buffer_[head]), size);
            } else {
                std::memcpy(&(out[0]), &(buffer_[head]), max_size_ - head);
                std::memcpy(&(out[max_size_ - head]), &(buffer_[0]), size - (max_size_ - head));
            }
        }
    }

    //head_.store((head + size) % max_size_, std::memory_order_release);
    //size_.store(Size() - size, std::memory_order_release);

    return size;
}

bool xCircularBuffer::Put(const std::size_t& size) {
    std::size_t current_size = size_.load(std::memory_order_acquire);
    if (current_size < size) {
        return false;
    }

    std::size_t head = head_.load(std::memory_order_acquire);
    std::size_t new_head;

    if (tail_ < head) {
        if (size <= max_size_ - head) {
            new_head = head + size;
        } else {
            new_head = size - (max_size_ - head);
        }
    } else if (tail_ > head) {
        new_head = head + size;
    } else {
        if (tail_ == 0) {
            new_head = head + size;
        } else {
            if (size <= max_size_ - head) {
                new_head = head + size;
            } else {
                new_head = size - (max_size_ - head);
            }
        }
    }

    head_.store(new_head, std::memory_order_release);
    size_.store(current_size - size, std::memory_order_release);

    return true;
}

void xCircularBuffer::Resize(const std::size_t& size) {

    XINF("xCircularBuffer::Resize size:%d", size);

    if (size < max_size_) {
        return;
    }

    char* tbuffer_ = new char[size];
    std::memset(tbuffer_, 0, size);

    std::size_t head = head_.load(std::memory_order_acquire);
    std::size_t tail = tail_.load(std::memory_order_acquire);

    if (tail < head) {
        std::memcpy(&tbuffer_[0], &buffer_[head], max_size_ - head);
        std::memcpy(&tbuffer_[max_size_ - head], &buffer_[0], tail);
    } else if (tail > head) {
        std::memcpy(&tbuffer_[0], &buffer_[head], tail - head);
    } else {
        if (tail == 0) {
            std::memcpy(&tbuffer_[0], &buffer_[head], size_);
        } else {
            std::memcpy(&tbuffer_[0], &buffer_[head], max_size_ - head);
            std::memcpy(&tbuffer_[max_size_ - head], &buffer_[0], tail);
        }
    }

    head_.store(0, std::memory_order_release);
    tail_.store(size_, std::memory_order_release);

    max_size_ = size;
    delete[] buffer_;
    buffer_ = tbuffer_;
}

uint16 xCircularBuffer::HaveFullPackage() {
    if (m_bNeedBody) {
        if (Size() >= NetPackageHeadSize) {
            m_bNeedBody = false;
            Read((uint8*)&m_stNetPackageHead, NetPackageHeadSize);
            if (Size() >= m_stNetPackageHead.u16BodySize + NetPackageHeadSize) {
                m_bNeedBody = true;
                return m_stNetPackageHead.u16BodySize + NetPackageHeadSize;
            }
        }
    } else {
        if (Size() >= m_stNetPackageHead.u16BodySize + NetPackageHeadSize) {
            m_bNeedBody = true;
            return m_stNetPackageHead.u16BodySize + NetPackageHeadSize;
        }
    }
    return 0;
}

