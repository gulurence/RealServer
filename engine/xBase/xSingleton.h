/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xSingleton.h
      Purpose:   单例模板 xSingleton<T>

      定义 xSingleton<T> 模板，提供 getMe/instance/get_mutable_instance 访问器
      内嵌 CGarbo 类在静态析构期自动释放单例实例
  *****************************************************************/

#pragma once

#include "xDefine.h"

template <typename T> class xSingleton
{
private:
    xSingleton(const xSingleton &);
    const xSingleton &operator=(const xSingleton &);

protected:
    static T *_instance;
    xSingleton()
    {
        static CGarbo garbo;
    }
    virtual ~xSingleton()
    {
    }

public:
    /*        static void delMe()
            {
                SAFE_DELETE(_instance);
            }
            */
    static T *instance()
    {
        if (!_instance)
        {
            _instance = new T;
        }
        return _instance;
    }
    static T &getMe()
    {
        if (!_instance)
        {
            _instance = new T;
        }
        return *_instance;
    }
    // Alias for compatibility with boost::serialization::singleton pattern
    static T &get_mutable_instance()
    {
        return getMe();
    }
    class CGarbo
    {
    public:
        ~CGarbo()
        {
            SAFE_DELETE(xSingleton::_instance);
        }
        CGarbo()
        {
        }
    };
};

template <typename T> T *xSingleton<T>::_instance = 0;
