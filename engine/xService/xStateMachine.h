#pragma once

#include "xState.h"

template<typename entity_type>
class xStateMachine
{
    public:
        xStateMachine(entity_type *_owner, xState<entity_type> *_curState, 
                xState<entity_type> *_globalState = NULL) : owner(_owner) 
        {
            prevState = NULL;
            curState = _curState;
            globalState = _globalState;
        }
        virtual ~xStateMachine(){}

        void update() const
        {
            if (globalState) globalState->execute(owner);
            if (curState) curState->execute(owner);
        }

        void changeState(xState<entity_type> *newState)
        {
            if (!newState || curState == newState) return;
            prevState = curState;
            curState = newState;

            if (prevState)
                prevState->exit(owner);
            curState->enter(owner);
        }

        void changeGlobalState(xState<entity_type> *newState)
        {
            if (!newState) return;
            if (globalState)
                globalState->exit(owner);
            globalState = newState;
            globalState->enter(owner);
        }

        void revertToPreviousState()
        {
            changeState(prevState);
        }

        xState<entity_type> *getCurState()
        {
            return curState;
        }

    private:
        entity_type * const owner; //闁绘鍩栭埀顑跨劍濠р偓闁归晲鐒﹀﹢渚€鎳?

        xState<entity_type> *prevState;        //闁告挸绉崇粩鎾偐閼哥鍋?
        xState<entity_type> *curState;        //鐟滅増鎸告晶鐘绘偐閼哥鍋?
        xState<entity_type> *globalState;    //闁稿繈鍔岄惇顒勬偐閼哥鍋?

};



