#pragma once

#include "xDefine.h"


template <typename T>
class xCircularPool
{
public:
    xCircularPool(size_t initial_capacity = 5)
        : capacity(initial_capacity), head(0), tail(0) {
        buffer = new T[capacity];
    }

    ~xCircularPool() {
        delete[] buffer;
    }

    bool Push(const T& value) {
        size_t tail_pos = tail.load(std::memory_order_relaxed);
        size_t next_tail = (tail_pos + 1) % capacity;

        if (next_tail == head.load(std::memory_order_acquire)) {
            // 队列已满，尝试扩容
            resize(capacity * 2);  // 扩展队列为原来的两倍大小
            tail_pos = tail.load(std::memory_order_relaxed);
            next_tail = (tail_pos + 1) % capacity;
        }

        buffer[tail_pos] = value;
        tail.store(next_tail, std::memory_order_release);
        return true;
    }

    bool Pop(T& result) {
        size_t head_pos = head.load(std::memory_order_relaxed);

        if (head_pos == tail.load(std::memory_order_acquire)) {
            // 队列为空
            return false;
        }

        result = buffer[head_pos];
        buffer[head_pos] = nullptr;
        head.store((head_pos + 1) % capacity, std::memory_order_release);
        return true;
    }

private:
    void resize(size_t new_capacity) {
        // 锁住队列，避免其他线程在扩容时访问队列
        std::lock_guard<std::mutex> lock(resize_mutex);

        // 如果扩容后的大小与当前大小相同，直接返回
        if (new_capacity == capacity) {
            return;
        }

        // 创建新的更大的缓冲区
        T* new_buffer = new T[new_capacity];

        size_t current_size = (tail.load() - head.load()) % capacity;
        size_t idx = 0;
        while (head.load() != tail.load()) {
            new_buffer[idx++] = buffer[head.load()];
            head.store((head.load() + 1) % capacity, std::memory_order_relaxed);
        }

        for (int i = 0; i < current_size; ++i) {
            buffer[i] = nullptr;
        }

        // 更新容量和指针
        delete[] buffer;
        buffer = new_buffer;
        capacity = new_capacity;
        head.store(0, std::memory_order_relaxed);
        tail.store(current_size, std::memory_order_relaxed);

    }

private:
    size_t capacity;               // 队列容量
    std::atomic<size_t> head;      // 队头指针
    std::atomic<size_t> tail;      // 队尾指针
    T* buffer;                     // 数据缓冲区
    std::mutex resize_mutex;       // 用于扩容时锁住队列
};

//// 生产者线程
//void producer(LockFreeQueue<int>& queue) {
//    for (int i = 0; i < 10; ++i) {
//        while (!queue.enqueue(i)) {
//            std::this_thread::yield();  // 队列满时等待
//        }
//        std::cout << "Produced: " << i << "\n";
//    }
//}
//
//// 消费者线程
//void consumer(LockFreeQueue<int>& queue) {
//    int value;
//    for (int i = 0; i < 10; ++i) {
//        while (!queue.dequeue(value)) {
//            std::this_thread::yield();  // 队列空时等待
//        }
//        std::cout << "Consumed: " << value << "\n";
//    }
//}
//
//int main() {
//    LockFreeQueue<int> queue;
//
//    std::thread t1(producer, std::ref(queue));
//    std::thread t2(consumer, std::ref(queue));
//
//    t1.join();
//    t2.join();
//
//    return 0;
//}
//
//
