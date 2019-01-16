/*****************************************************************
            Copyright (c) 2009, XXXXXX������Ƽ��ɷ����޹�˾
                    All rights reserved

    �������ڣ� 2009��12��2�� 17ʱ04��
    �ļ����ƣ� SingletonTemplate.h
    ˵    ���� ����ģʽ�ķ���ʵ�֡�����Ԫ�ṩ�������͵����ࣺ

                �ֱ�ʵ�������������ϵĵ����� ���У�

                1. ��CiaSingleInstance<T>��T��is a�Ĺ�ϵ��Ӧ�ô�ģʽ��˵��T���ǿɼ̳еģ�����T�������������������
                1.1 Ĭ�Ϲ��캯��������������public��protected�ķ��ʼ���
                1.2 ��������������virtual��

                2. ��ChaSingleInstance<T>��T��has a�Ĺ�ϵ����������ϵ��Ӧ�ô�ģʽ��˵��T��ChaSingleInstance<T>��
                ��һ����̬��Ա�������ͣ�����T�������������������
                2.1 Ĭ�Ϲ��캯��������������public�ķ��ʼ���

                ���飺1. ���ȿ���ʹ��ChaSingleInstance<T>
                2. typedef ��ʹ������߿ɶ��ԣ�
                �磺typedef ChaSingleInstance<T> TSingleInstance;

    ��ǰ�汾��  1.00
    ��    �ߣ�  
    ��    ����  ����  

    ��ǰ�汾��  1.01
    ��    �ߣ�  
    ��    ����  ��Ӷ��̵߳�ʵ��֧��, ��̬����˳���ʼ��֧��

*********************************************************************/
#pragma once


template <typename T>
class CiaSingleton final : public T
{
public:
    typedef T SingletonType;

public:
    // ��ȡ����
    static SingletonType * singleton();
    // �ͷŵ���
    static void release();

private:
    // ����ʵ��ָ��
    static SingletonType *sm_pSingleInstance;

private:
    CiaSingleton(const CiaSingleton &) = delete;
    const CiaSingleton & operator=(const CiaSingleton &) = delete;

    // ���಻�ɼ̳У�����ʵ����
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

    // ��ȡ����
    static SingleInstanceType * singleton();
    // �ͷŵ���
    static void release();

private:
    // ����ʵ��ָ��
    static SingleInstanceType *sm_pSingleInstance;

private:
    ChaSingleton(const ChaSingleton &) = delete;
    const ChaSingleton & operator=(const ChaSingleton &) = delete;

    // ���಻�ɼ̳У�����ʵ����
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


// ��CiaSingleInstance�Ķ���
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


// ��ChaSingleInstance�Ķ���
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
