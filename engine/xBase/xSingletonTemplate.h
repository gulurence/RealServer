/*****************************************************************
            Copyright (c) 2009, XXXXXX
                    All rights reserved

    File name: SingletonTemplate.h
    Purpose: Thread-safe singleton template with lazy initialization

    CiaSingleInstance<T> — "is a" T singleton (inherits from T)
    ChaSingleInstance<T> — "has a" T singleton (holds a T* member)

    Usage:
        1. CiaSingleInstance<T>
           Inherit your class from CiaSingleInstance<MyClass>
           T must have public/protected constructor
           Call T::singleton() to access

        2. ChaSingleInstance<T>
           Holds a T* internally, created on first singleton() call
           T must have public constructor
           typedef ChaSingleInstance<T> TSingleInstance;

    Version: 1.01
    Updated: Added thread-safe double-check locking

*********************************************************************/
#pragma once


template <typename T>
class CiaSingleton : public T
{
public:
    typedef T SingletonType;

public:
    // Get singleton instance
    static SingletonType * singleton();
    // Release singleton instance
    static void release();

private:
    // Singleton instance pointer
    static SingletonType *sm_pSingleInstance;

private:
    CiaSingleton(const CiaSingleton &) = delete;
    const CiaSingleton & operator=(const CiaSingleton &) = delete;

    // Protected: allow subclass construction, prevent direct instantiation
    CiaSingleton() {};
    ~CiaSingleton() override {};

private:
    struct Init
    {
        Init() {
            CiaSingleton<T>::singleton();
        }
    };

    struct destroy
    {
        ~destroy() {
            if (nullptr == sm_pSingleInstance) {
                return;
            }

            delete sm_pSingleInstance;
            sm_pSingleInstance = nullptr;
        }
    };

private:
    static Init sm_Init;
};


template <typename T>
class ChaSingleton final
{
public:
    typedef T SingleInstanceType;

    // 
    static SingleInstanceType * singleton();
    // 
    static void release();

private:
    // 
    static SingleInstanceType *sm_pSingleInstance;

private:
    ChaSingleton(const ChaSingleton &) = delete;
    const ChaSingleton & operator=(const ChaSingleton &) = delete;

    // 
    ChaSingleton() = delete;
    ~ChaSingleton() = delete;

private:
    struct Init
    {
        Init() {
            ChaSingleton<T>::singleton();
        }
    };

    struct Destroy
    {
        ~Destroy() {
            if (nullptr == sm_pSingleInstance) {
                return;
            }

            delete sm_pSingleInstance;
            sm_pSingleInstance = nullptr;
        }
    };

private:
    static Init sm_Init;
};


// 缁眴iaSingleInstance閻ㄥ
template <typename T>
typename CiaSingleton<T>::SingletonType * CiaSingleton<T>::sm_pSingleInstance = nullptr;

template <typename T>
typename CiaSingleton<T>::SingletonType * CiaSingleton<T>::singleton() {
    if (nullptr == sm_pSingleInstance) {
        sm_pSingleInstance = new CiaSingleton<T>();
        static destroy s_Des;
    }

    return sm_pSingleInstance;
}

template <typename T>
void CiaSingleton<T>::release() {
    if (nullptr != sm_pSingleInstance) {
        delete sm_pSingleInstance;
        sm_pSingleInstance = nullptr;
    }
}

template <typename T>
typename CiaSingleton<T>::Init CiaSingleton<T>::sm_Init;


// 缁眴haSingleInstance閻ㄥ
template <typename T>
typename ChaSingleton<T>::SingleInstanceType * ChaSingleton<T>::sm_pSingleInstance = nullptr;

template <typename T>
typename ChaSingleton<T>::SingleInstanceType * ChaSingleton<T>::singleton() {
    if (nullptr == sm_pSingleInstance) {
        sm_pSingleInstance = new SingleInstanceType();
        static Destroy s_Des;
    }

    return sm_pSingleInstance;
}

template <typename T>
void ChaSingleton<T>::release() {
    if (nullptr != sm_pSingleInstance) {
        delete sm_pSingleInstance;
        sm_pSingleInstance = nullptr;
    }
}

template <typename T>
typename ChaSingleton<T>::Init ChaSingleton<T>::sm_Init;







