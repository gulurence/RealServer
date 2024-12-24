#include "xTask.h"

xTask::xTask():xNetProcessor(nullptr) {
}

xTask::~xTask()
{
}

void xTask::init(int fd) {
    xNetProcessor::init(fd);
    //m_stSock.setNonBlock();
}

void xTask::cleanup() {
    xNetProcessor::cleanup();
}



