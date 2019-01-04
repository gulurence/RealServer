#include "xTask.h"

xTask::xTask(int fd)
:xNetProcessor(fd)
{
    sock.setNonBlock();
}

xTask::~xTask()
{
}
