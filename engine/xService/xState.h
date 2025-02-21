#pragma once

template<typename entity_type>
class xState
{
    public:
        virtual ~xState(){}

        virtual void enter(entity_type *) = 0;
        virtual void execute(entity_type *) = 0;
        virtual void exit(entity_type *) = 0;
};



