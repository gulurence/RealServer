﻿#pragma once

#include "xDefine.h"

#include <libxml/parser.h>
#include <libxml/encoding.h>
#include <iconv.h>


//XML婢跺嫮鎮?
class xXMLParser {
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
    template<typename T>
    bool getPropValue(xmlNodePtr node, const char *name, T* out) const {
        if (!node)
            return false;
        if (!xmlHasProp(node, BAD_CAST name))
            return false;

        *out = T();

        xmlChar *utf = xmlGetProp(node, BAD_CAST name);

#if 0
        size_t len = (xmlStrlen(utf)+1)*2;
        char *conv = (char *) xmlMalloc(len);
        if (0==code_convert("UTF-8", "GBK", (char *)utf, xmlStrlen(utf), conv, len))
        {
            //        XDBG("xml 鏉烆剛鐖滈崥?%s size:%u", conv, xmlStrlen(utf));
            bcopy(conv, (void *)utf, (size_t)xmlStrlen(utf));
        }
        else
        XERR("xml 鏉烆剛鐖滄径杈Е... %s", name);

        std::stringstream ss((char *)conv);

#endif
        std::stringstream ss((char *) utf);
        ss >> *out;
        xmlFree (utf);
        return true;
    }

private:
    //bool toUTF8();
    static int code_convert(const char *from_charset, const char *to_charset,
            char *inbuf, size_t inlen, char *outbuf, size_t outlen);
    xmlDocPtr doc;
    char *docBuffer;

    static iconv_t iconv_utf8_gbk;
    static iconv_t iconv_gbk_utf8;
    static int gbk_input(unsigned char *out, int *outlen,
            const unsigned char *in, int *inlen);
    static int gbk_output(unsigned char *out, int *outlen,
            const unsigned char *in, int *inlen);

};



