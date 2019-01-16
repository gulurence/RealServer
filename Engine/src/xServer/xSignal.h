#pragma once
#include "GameDefine.h"
#include <set>
#include <list>
template <typename P, typename T>
class xSignal
{
    public:
        xSignal(){}
        ~xSignal(){}

        void connect(P p)
        {
            _plist.push_back(p);
        };

        void disconnect(P p)
        {
            _plist.erase(p);
        };

        void clear()
        {
            for (_piter it=_plist.begin(); it!=_plist.end(); it++)
            {
                (*it).clear();    
            }
        }

        void operator()() const
        {
            for (_piter it=_plist.begin(); it!=_plist.end(); it++)
            {
                (*it)();    
            }
        };

        void operator()(T t) 
        {
            _piter iter = _plist.begin(), temp;
            while (iter != _plist.end())
            {
                temp = iter++;
                AI_RET_ENUM ret = (*temp)(t);
                if (ret == AI_RET_RUN_SUCC_CONT || ret == AI_RET_COND_FAIL_CONT)
                    continue;
                if (ret == AI_RET_COND_FAIL)
                    return;

                (*temp).clear();
                _plist.erase(temp);
            }
        };

    private:
        std::list<P> _plist;
        typedef typename std::list<P>::iterator _piter;
};
