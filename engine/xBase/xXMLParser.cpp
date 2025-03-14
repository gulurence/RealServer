﻿#include "xXMLParser.h"
#include "xTools.h"

iconv_t xXMLParser::iconv_utf8_gbk;
iconv_t xXMLParser::iconv_gbk_utf8;

xXMLParser::xXMLParser() {
    doc = 0;
    docBuffer = NULL;
}

xXMLParser::~xXMLParser() {
    if (doc) xmlFreeDoc(doc);
    if (docBuffer) free(docBuffer);
#ifdef _LX_DEBUG
    XLOG("[xXMLParser],闁哄鍔栭悗?);
#endif
}

void xXMLParser::initSystem() {
    xmlInitParser();
    XLOG("[xXMLParser],闁告帗绻傞～鎰板礌?");
    /*
    LIBXML_TEST_VERSION;
    iconv_utf8_gbk = iconv_open("UTF-8", "GBK");
    iconv_gbk_utf8 = iconv_open("GBK", "UTF-8");

    xmlCharEncodingHandlerPtr h = 0;
    h = xmlNewCharEncodingHandler("GB2312", gbk_input, gbk_output);//婵烇綀顕ф慨鐎廱2312缂傚倹鐗滈悥婊堝绩椤栨稑鐦?
    if (!h)
        XERR("婵炲鍔岄崬绱綛2312缂傚倹鐗滈悥婊勫緞鏉堫偉袝");
    h = xmlNewCharEncodingHandler("GBK", gbk_input, gbk_output);//婵烇綀顕ф慨鐎廱k缂傚倹鐗滈悥婊堝绩椤栨稑鐦?
    if (!h)
        XERR("婵炲鍔岄崬绱綛K缂傚倹鐗滈悥婊勫緞鏉堫偉袝");
    XLOG("[XML]婵炲鍔岄崬鐣屾喆閿濆洨鍨抽柛?GB2312 GBK");
    */
}

void xXMLParser::clearSystem() {
    /*
    iconv_close(iconv_gbk_utf8);
    iconv_close(iconv_utf8_gbk);
    */
    xmlCleanupParser();
    xmlMemoryDump();
}

int xXMLParser::gbk_input(unsigned char* out, int* outlen, const unsigned char* in, int* inlen) {
    char* outbuf = (char*)out;
    char* inbuf = (char*)in;
    size_t          rslt;
    XDBG("[XML]gbk_input 閺夌儐鍓涢悥婊堝礈?%d %s", *inlen, in);

    rslt =
#ifndef _WINDOWS
        iconv(iconv_utf8_gbk, &inbuf, (size_t*)inlen,
            &outbuf, (size_t*)outlen);
#else
        iconv(iconv_utf8_gbk, (char**)&inbuf, (size_t*)inlen,
            &outbuf, (size_t*)outlen);
#endif
    if (rslt < 0) {
        XERR("[XML]gbk_input 閺夌儐鍓涢悥婊勫緞鏉堫偉袝 %d", rslt);
        return rslt;
    }
    XDBG("[XML]gbk_input 閺夌儐鍓涢悥婊堝触?%d %s", *outlen, out);
    *outlen = ((unsigned char*)outbuf - out);
    *inlen = ((unsigned char*)inbuf - in);
    XDBG("[XML]gbk_input 閺夌儐鍓涢悥?ret=%d inlen=%d outlen=%d", rslt, *inlen, *outlen);
    return *outlen;
}

int xXMLParser::gbk_output(unsigned char* out, int* outlen, const unsigned char* in, int* inlen) {
    char* outbuf = (char*)out;
    char* inbuf = (char*)in;
    size_t          rslt;

    rslt =
#ifndef _WINDOWS
        iconv(iconv_gbk_utf8, &inbuf, (size_t*)inlen,
            &outbuf, (size_t*)outlen);
#else
        iconv(iconv_gbk_utf8, (char**)&inbuf, (size_t*)inlen,
            &outbuf, (size_t*)outlen);
#endif

    if (rslt < 0) {
        XERR("[XML]gbk_output 閺夌儐鍓涢悥婊勫緞鏉堫偉袝 %d", rslt);
        return rslt;
    }
    *outlen = ((unsigned char*)outbuf - out);
    *inlen = ((unsigned char*)inbuf - in);
    XDBG("[XML]gbk_output 閺夌儐鍓涢悥?ret=%d inlen=%d outlen=%d", rslt, *inlen, *outlen);
    return *outlen;
}


bool xXMLParser::parseDoc(const char* file) {
    xmlKeepBlanksDefault(1);
    //doc = xmlReadFile(file, "GBK", 0);

    FILE* in = fopen(file, "r");
    if (in == NULL) return false;

    fseek(in, 0L, SEEK_END);
    uint32 numBytes = ftell(in);
    fseek(in, 0L, SEEK_SET);

    docBuffer = (char*)calloc(numBytes, sizeof(char));
    if (docBuffer == NULL)
        return 1;

    fread(docBuffer, sizeof(char), numBytes, in);
    fclose(in);
    doc = xmlParseMemory(docBuffer, numBytes);

    if (!doc) {
        XERR("can not parse xml file %s", file);
        return false;
    }
    return true;
}

int xXMLParser::code_convert(const char* from_charset, const char* to_charset, char* inbuf, size_t inlen, char* outbuf, size_t outlen) {
    iconv_t cd;
    char** pin = &inbuf;
    char** pout = &outbuf;

    cd = iconv_open(to_charset, from_charset);
    if (cd == 0) return -1;
    memset(outbuf, 0, outlen);
#ifndef _WINDOWS
    if (iconv(cd, pin, (size_t*)&inlen, pout, (size_t*)&outlen) == (uint32)-1)
#else
    if (iconv(cd, (char**)pin, (size_t*)&inlen, pout, (size_t*)&outlen) == (uint32)-1)
#endif
        return -1;
    iconv_close(cd);
    return 0;
}

//
/*
bool xXMLParser::toUTF8()
{
    if (!doc) return false;
    xmlNodePtr root = getRoot();
    if (!root) return false;

    xmlChar *utf = xmlNodeGetContent(root);//闂傚洠鍋撻悷鏇氱窔閸ｆ挳寮ㄩ幆褍鏁堕悗?
    XDBG("xml 閺夌儐鍓涢悥婊堝礈?%s", (char *)utf);
    size_t len = xmlStrlen(utf);
    char *out = (char *) xmlMalloc(len);
    bzero(out, len);
    if (0==code_convert("GB2312", "UTF-8", (char *)utf, xmlStrlen(utf), out, len))
    {
        XDBG("xml 閺夌儐鍓涢悥婊堝触?%s", out);
        bcopy(out, (void *)utf, len);
        return true;
    }
    else
        return false;
}
*/

xmlNodePtr xXMLParser::getRoot() {
    if (!doc) { return 0; }
    return xmlDocGetRootElement(doc);
}

xmlNodePtr xXMLParser::getChild(xmlNodePtr node) {
    if (!node) { return 0; }
    return node->xmlChildrenNode;
}

xmlNodePtr xXMLParser::getChild(xmlNodePtr node, const char* name) {
    if (!node) { return 0; }

    xmlNodePtr tmp = node->xmlChildrenNode;
    while (tmp) {
        if (tmp->type == XML_ELEMENT_NODE) {
            if (!xmlStrcmp(tmp->name, (const xmlChar*)name)) {
                return tmp;
            }
        }
        tmp = getNext(tmp);
    }
    return 0;
}

uint32 xXMLParser::getChildNum(xmlNodePtr node, const char* name) {
    if (!node) { return 0; }

    uint32 num = 0;
    xmlNodePtr tmp = node->xmlChildrenNode;
    while (tmp) {
        if (tmp->type == XML_ELEMENT_NODE) {
            if (!xmlStrcmp(tmp->name, (const xmlChar*)name)) {
                ++num;
            }
        }
        tmp = getNext(tmp);
    }
    return num;
}

xmlNodePtr xXMLParser::getNext(xmlNodePtr node) {
    if (!node) { return 0; }

    xmlNodePtr tmp = node->next;
    while (tmp) {
        if (tmp->type == XML_ELEMENT_NODE) {
            return tmp;
        }
        tmp = tmp->next;
    }
    return 0;
}

xmlNodePtr xXMLParser::getNext(xmlNodePtr node, const char* name) {
    if (!node) { return 0; }

    xmlNodePtr tmp = node->next;
    while (tmp) {
        if (tmp->type == XML_ELEMENT_NODE) {
            if (!xmlStrcmp(tmp->name, (const xmlChar*)name)) {
                return tmp;
            }
        }
        tmp = getNext(tmp);
    }
    return 0;
}

xmlNodePtr xXMLParser::getNodeByPath(const char* path) {
    std::vector<std::string> v;
    stringTok(path, "/", v);
    if (v.size() <= 1) { return 0; }

    uint32 i = 0;
    xmlNodePtr tmp = getRoot();
    while (tmp) {
        i++;
        tmp = getChild(tmp, (char*)v[i].c_str());
        if (i == v.size() - 1) { return tmp; }
    }

    return 0;
}

bool xXMLParser::getPropStr(xmlNodePtr node, const char* name, std::string& out) const {
    //
    if (!node) { return false; }
    if (!xmlHasProp(node, BAD_CAST name)) { return false; }
    xmlChar* utf = xmlGetProp(node, BAD_CAST name);
    out += (char*)utf;
    xmlFree(utf);
    return true;
}

bool xXMLParser::InnerXml(xmlNodePtr node, std::string& out) const {
    if (!node) { return false; }
    xmlChar* utf = xmlNodeGetContent(node);
    out += (char*)utf;
    xmlFree(utf);
    return true;
}



