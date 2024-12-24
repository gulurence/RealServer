#include "main.h"
#include <iostream>
#include <libxml/parser.h>
#include <libxml/tree.h>

void parseXML(const char* filename) {
    // 解析 XML 文件
    xmlDocPtr doc = xmlReadFile(filename, NULL, 0);
    if (doc == NULL) {
        std::cerr << "Failed to parse " << filename << std::endl;
        return;
    }

    // 获取根元素
    xmlNodePtr root = xmlDocGetRootElement(doc);

    // 确保根元素是 bookstore
    if (root == NULL || xmlStrcmp(root->name, (const xmlChar*)"bookstore") != 0) {
        std::cerr << "Root element is not bookstore!" << std::endl;
        xmlFreeDoc(doc);
        return;
    }

    // 遍历所有的 <book> 元素
    for (xmlNodePtr bookNode = root->children; bookNode != NULL; bookNode = bookNode->next) {
        if (bookNode->type == XML_ELEMENT_NODE && xmlStrcmp(bookNode->name, (const xmlChar*)"book") == 0) {
            xmlChar* title = NULL;
            xmlChar* author = NULL;
            xmlChar* year = NULL;
            xmlChar* lang = NULL;

            // 遍历 <book> 元素的子节点
            for (xmlNodePtr childNode = bookNode->children; childNode != NULL; childNode = childNode->next) {
                if (childNode->type == XML_ELEMENT_NODE) {
                    if (xmlStrcmp(childNode->name, (const xmlChar*)"title") == 0) {
                        title = xmlNodeGetContent(childNode);
                        lang = xmlGetProp(childNode, (const xmlChar*)"lang");
                    } else if (xmlStrcmp(childNode->name, (const xmlChar*)"author") == 0) {
                        author = xmlNodeGetContent(childNode);
                    } else if (xmlStrcmp(childNode->name, (const xmlChar*)"year") == 0) {
                        year = xmlNodeGetContent(childNode);
                    }
                }
            }

            // 打印 <book> 元素的内容
            std::cout << "Book: " << std::endl;
            std::cout << "  Title: " << (title ? (char*)title : "N/A") << std::endl;
            std::cout << "  Author: " << (author ? (char*)author : "N/A") << std::endl;
            std::cout << "  Year: " << (year ? (char*)year : "N/A") << std::endl;
            std::cout << "  Language: " << (lang ? (char*)lang : "N/A") << std::endl;

            // 释放分配的内存
            xmlFree(title);
            xmlFree(author);
            xmlFree(year);
            xmlFree(lang);
        }
    }

    // 释放文档
    xmlFreeDoc(doc);
}

int xml2_main() {
    const char* filename = "example.xml";  // XML 文件名
    parseXML(filename);

    // 清理 libxml2 库的使用
    xmlCleanupParser();

    return 0;
}



