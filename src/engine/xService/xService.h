#pragma once

#include "xServiceDefine.h"

#include "xBase/xDefine.h"
#include "xBase/xThread.h"
#include "xBase/xTime.h"
#include "xNet/xSocket.h"


class xService : public xThread
{
public:
    xService(const char* pszServiceName, uint16 u16Port);
    virtual ~xService();


public:
    void ServiceStop() {
        SetServiceState(XSERVICE_STOP);
    }
    xServiceState GetServiceState() { 
        return m_enServiceState; 
    }
    void SetServiceState(xServiceState enState) {
        m_enServiceState = enState;
    }

public:
    virtual void thread_proc();

protected:
    virtual void Release();

private:
    xServiceState m_enServiceState;
    char m_pszServiceName[MAX_NAMESIZE] = { 0 };
    uint16 m_u16Port = 0;
};




class ModuleInf
{
public:
    ModuleInf(){}
    ~ModuleInf(){}

public:
    virtual std::string ModuleName() = 0;

public:
    virtual int Install() = 0;
    virtual int Init() = 0;
    virtual int Running() = 0;
    virtual int Pause() = 0;
    virtual int Close() = 0;
    virtual int UnInstall() = 0;
};


#define IP_LEN 16
#define NET_VERIFY_KEY_LEN 33

struct MNetParamsST {
    MNetParamsST(uint8 *pszIp, uint16 u16Port, uint8* pszVerifyKey) {
        m_u16Port = u16Port;
        bcopy(pszIp, m_pszIp, IP_LEN-1);
        bcopy(pszVerifyKey, m_pszVerifyKey, NET_VERIFY_KEY_LEN - 1);
    }

public:
    uint8 m_pszIp[IP_LEN] = { 0 };
    uint16 m_u16Port;
    uint8 m_pszVerifyKey[NET_VERIFY_KEY_LEN] = { 0 };
};

class MNet : public ModuleInf
{
public:
    MNet(const std::string &strModuleName,const MNetParamsST&stParams):m_strModuleName(strModuleName){}
    ~MNet(){}

public:
    virtual std::string ModuleName() { return m_strModuleName; }

public:
    virtual int Install() { return 0; }
    virtual int Init() { return 0; }
    virtual int Running() { return 0; }
    virtual int Pause() { return 0; }
    virtual int Close() { return 0; }
    virtual int UnInstall() { return 0; }

private:
    std::string m_strModuleName;
};

inline void testttt() {

    //auto* pModule = new MNet("MNet#Client",);

}

typedef std::map<std::string, ModuleInf*> MapModule;
class ModuleMgr
{
public:
    ModuleMgr() {}
    ~ModuleMgr() {}

private:
    MapModule m_mapMoudle;

public:
    bool RegistModule(ModuleInf *pModule) {

        return true;
    }
    bool UnRegistModule(ModuleInf* pModule) {

        return true;
    }
};

class ServiceFun
{

};

class ServiceInterface : public ServiceFun
{
public:
    ServiceInterface(){}
    ~ServiceInterface(){}

public:
    int serviceType = 0;
    std::string serviceName;

public:


private:
    void Type() {}
    void Name(){}

private:


};

class SLogin : public ServiceInterface
{
public:
    SLogin(){}
    ~SLogin(){}

public:
    SLogin* Create() {
        return new SLogin();
    }
};

template <class T>
class ServiceCreator
{
public:
    ServiceCreator(){}
    ~ServiceCreator(){}

public:

};


class ServiceMgr
{
public:

private:

public:


};

template <class T>
class SIN
{
    T data;
};

inline void testFun() {


    //ServiceCreator<SLogin> 



    //SIN<Plateform>


    //SLog(Plateform).Print("",32234);

    //SDB(Plateform).Load("", 32234);

    //SRedis(Plateform).Set("", 32234);


    //S(Net,Plateform).Send("", 32234);
    //SNet(Plateform).Send("", 32234);

    //gSNetLogin.Send(1254,"",122);

    //SNet(Plateform).Send("", 32234);


    //SRedis.Set(SLOGIN, "123", "123");


    //SNet.Send(SLOGIN, "123", "123");




}


