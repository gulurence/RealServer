﻿#pragma once

#include "xDefine.h"

template <typename T>
class UniqueIDManager
{
public:
    UniqueIDManager(T min, T max = (T)-2) :_min(min), _max(max - 1) {
        _next = _min;
        _num = (_max - _min + 1);
        _used.clear();
        _count = 0;
    }
    T& getUniqueID(T& value) {
        if (_count >= _num) {
            value = (T)-1;
            return value;
        }

        T tmp = _next;
        while (1) {
            if (_max == _next)
                _next = _min;
            else
                _next++;
            if (_used.find(tmp) == _used.end()) {
                _used.insert(tmp);
                ++_count;
                value = tmp;
                return value;
            }
            tmp = _next;
        }
    }
    void putUniqueID(T value) {
        if (_used.find(value) != _used.end()) {
            _used.erase(value);
            _count--;
        }
    }
    T getMin() {
        return _min;
    }
    T getMax() {
        return _max;
    }
    void clear() {
        _used.clear();
    }

private:
    UniqueIDManager();
    T _min;
    T _max;
    T _next;
    T _num;
    T _count;
    std::set<T> _used;
};

typedef UniqueIDManager<uint32> UniqueUInt32IDManager;
typedef UniqueIDManager<uint16> UniqueUInt16IDManager;



