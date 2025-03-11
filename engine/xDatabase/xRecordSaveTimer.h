#pragma once

#include "RecordDef.h"

struct SaveTimerData
{
    uint64 u64DataID = 0;
    uint32 u32EndTimeTick = 0;
    SaveTimerData(uint64 uID, uint32 endTime) {
        u64DataID = uID;
        u32EndTimeTick = endTime;
    }
};

typedef std::map<uint64, time_t> RECORD_SAVE_ID_MAP;
typedef std::list<SaveTimerData> RECORD_SAVE_TIME_LIST;


class RecordSaveTimer
{
public:
    RecordSaveTimer(){}
    ~RecordSaveTimer(){}

private:
    std::mutex m_lockTimer;
    RECORD_SAVE_ID_MAP m_mapID;
    RECORD_SAVE_TIME_LIST m_listTimerNode;

public:
    void RunTimer(time_t time, bool allSave=false) {
        RECORD_SAVE_TIME_LIST tmpList;
        m_lockTimer.lock();
        auto it = m_listTimerNode.begin();
        for (; it != m_listTimerNode.end();)
        {
            auto it_temp = it++;

            if ((*it_temp).u32EndTimeTick <= time || allSave) {
                tmpList.push_back(*it_temp);
                m_mapID.erase((*it_temp).u64DataID);
                m_listTimerNode.erase(it_temp);
            }
            else {
                break;
            }
        }
        m_lockTimer.unlock();

        auto it_run = tmpList.begin();
        for (; it_run != tmpList.end();++it_run) {
            OnSaveTimer((*it_run).u64DataID);
        }
    }
    void AddTimer(uint64 uID, uint32 delaySaveTime = RECORD_DELAY_SAVE_TIME) {
        if (uID <= 0) {
            return;
        }

        time_t curTime = time(nullptr);
        m_lockTimer.lock();
        if (m_mapID.find(uID) != m_mapID.end()) {
            if (m_mapID[uID] > curTime + delaySaveTime) {
                m_mapID[uID] = curTime + delaySaveTime;
            }
            m_lockTimer.unlock();
            return;
        }

        m_mapID[uID] = curTime;
        m_listTimerNode.push_back(SaveTimerData(uID, curTime + delaySaveTime));
        m_lockTimer.unlock();
    }

public:
    virtual void OnSaveTimer(uint64) = 0;
};




