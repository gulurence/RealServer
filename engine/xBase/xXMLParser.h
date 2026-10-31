/*****************************************************************
      Copyright (c) 2026 GuMingYao,  All rights reserved

      File name: xXMLParser.h
      Purpose:   libxml2 封装 XML 解析类

      声明 xXMLParser 类
      提供节点遍历、路径查询、属性读取与 InnerXml 接口，模板 getPropValue 支持类型转换
  *****************************************************************/

#pragma once

#include "xDefine.h"

#include <libxml/parser.h>
// iconv_t is used privately; real <iconv.h> included in .cpp
#ifndef _ICONV_H
typedef void *iconv_t;
#endif

// XML婢跺嫮鎮?
class xXMLParser
{
public:
    xXMLParser();
    ~xXMLParser();

    static void initSystem();
    static void clearSystem();

    bool parseDoc(const char *file);
    xmlNodePtr getRoot();
    xmlNodePtr getChild(xmlNodePtr node);
    xmlNodePtr getChild(xmlNodePtr node, const char *name);
    uint32 getChildNum(xmlNodePtr node, const char *name);

    xmlNodePtr getNext(xmlNodePtr node);
    xmlNodePtr getNext(xmlNodePtr node, const char *name);
    xmlNodePtr getNodeByPath(const char *path);

    bool getPropStr(xmlNodePtr node, const char *name, std::string &out) const;
    bool InnerXml(xmlNodePtr node, std::string &out) const;
    template <typename T> bool getPropValue(xmlNodePtr node, const char *name, T *out) const
    {
        if (!node)
        {
            return false;
        }
        if (!xmlHasProp(node, BAD_CAST name))
        {
            return false;
        }

        *out = T();

        xmlChar *utf = xmlGetProp(node, BAD_CAST name);

#if 0
        size_t len = (xmlStrlen(utf)+1)*2;
        char *conv = (char *) xmlMalloc(len);
        if (0==code_convert("UTF-8", "GBK", (char *)utf, xmlStrlen(utf), conv, len))
        {
            //        XDBG("xml converted: %s size:%u", conv, xmlStrlen(utf));
            bcopy(conv, (void *)utf, (size_t)xmlStrlen(utf));
        }
        else
        XERR("xml conversion error: %s", name);

        std::stringstream ss((char *)conv);

#endif
        std::stringstream ss((char *)utf);
        ss >> *out;
        xmlFree(utf);
        return true;
    }

private:
    // bool toUTF8();
    static int32 code_convert(const char *from_charset, const char *to_charset, char *inbuf,
                              size_t inlen, char *outbuf, size_t outlen);
    xmlDocPtr doc;
    char *docBuffer;

    static iconv_t iconv_utf8_gbk;
    static iconv_t iconv_gbk_utf8;
    static int32 gbk_input(uint8 *out, int32 *outlen, const uint8 *in, int32 *inlen);
    static int32 gbk_output(uint8 *out, int32 *outlen, const uint8 *in, int32 *inlen);
};
