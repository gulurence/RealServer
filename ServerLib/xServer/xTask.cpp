#include "xTask.h"

xTask::xTask(int fd)
:xNetProcessor(fd)
{
    m_stSock.setNonBlock();
}

xTask::~xTask()
{
}
