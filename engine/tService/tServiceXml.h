/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: tServiceXml.h
      Purpose:   XML 配置服务声明

      声明继承 ThridService 的 tServiceXml（EST_Xml）
      内部使用 CServerConfig（xXMLParser/libxml2）解析配置
  *****************************************************************/

#pragma once

#include "tThridService.h"

// tServiceXml — XML 配置服务封装
// 内部使用 CServerConfig (xXMLParser / libxml2)
class tServiceXml : public ThridService
{
public:
    tServiceXml(int32 id, const std::string &name) : ThridService(EST_Xml, id, name)
    {
    }

    virtual bool Init(const ThridServiceConfig &cfg) override;
    virtual void Release() override;
};
