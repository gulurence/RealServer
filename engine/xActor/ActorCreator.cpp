/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: ActorCreator.cpp
      Purpose:   ActorCreator 静态成员定义

      定义静态成员 CActorTypeMap ActorCreator::m_mapActorType, 供 GCC/Clang 链接使用
  *****************************************************************/

#include "ActorCreator.h"

// Static member definition (required by GCC/Clang, MSVC linkers are more lenient)
CActorTypeMap ActorCreator::m_mapActorType;
