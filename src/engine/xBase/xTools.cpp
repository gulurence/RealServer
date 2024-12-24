#include "xTools.h"
#include "xXMLParser.h"


std::map<std::string, std::string> Tools::global;

bool Tools::initGlobal()
{
    xXMLParser p;
    if (!p.parseDoc("config.xml"))
    {
        XERR("[Tools]闁瑰灚鎸哥槐鎴︽煀瀹ュ洨鏋傚鎯扮簿鐟?config.xml");
        return false;
    }
    XDBG("[Tools] 闁告梻濮惧ù?config.xml");

    xmlNodePtr root = p.getRoot();
    if (!root)
    {
        XERR("[Tools] 闂佹澘绉堕悿鍡涘棘閸ワ附顐介梺鎸庣懆椤?婵炲备鍓濆﹢涔簅ot闁煎搫鍊婚崑?");
        return false;
    }

    xmlNodePtr globalNode = p.getChild(root,"global");
    if (!globalNode)
    {
        XERR("[Tools] 婵炲备鍓濆﹢涔琹obal闁煎搫鍊婚崑?");
        return false;
    }
    std::string ss;
    xmlNodePtr node = p.getChild(globalNode);
    while (node)
    {
        ss.clear();
        if (p.getPropValue(node, "value", &ss))
        {
            global[(char *)node->name] = ss;
            XDBG("[global] %s, %s", (char *)node->name, ss.c_str());
        }
        //else
            //XERR("[global] %s 婵炲备鍓濆﹢?value 闁煎搫鍊婚崑?, (char *)node->name);

        node = p.getNext(node);
    }
    return true;
}

uint32 stringTok(std::string s, std::string k, std::vector<std::string> &v)
{
    std::string::size_type len = s.length();
    std::string::size_type i = 0, j = 0;

    while (i<len)
    {
        i = s.find_first_not_of(k, i);
        if (i==std::string::npos) break;

        j = s.find_first_of(k, i);
        if (j==std::string::npos)
        {
            v.push_back(s.substr(i, s.length()-i));
            break;
        }
        else
        {
            v.push_back(s.substr(i, j-i));
            i = j+1;
        }
    }

    return (uint32)v.size();
}
uint32 split(std::string str,std::string pattern,std::vector<std::string> &result)
{
    std::string::size_type pos;
    //std::vector<std::string> result;
    str+=pattern;//闁圭鏅涢惈宥団偓娑欘殘椤戜焦绋夐煫顓濈鞍闁哄倿鈧稓鈹掗柟鍨С缂?
    size_t size=str.size();

    for(size_t i=0; i<size; i++)
    {
        pos=str.find(pattern,i);
        if(pos<size)
        {
            std::string s=str.substr(i,pos-i);
            result.push_back(s);
            i=pos+pattern.size()-1;
        }
    }
    return (uint32)result.size();
}

const char* printBinary(char* data,int dataLen)
{
    std::string charData;
    snprintf(&(charData[0]),1024,"%s","0x");
    int pos = 2;
    for(int i=0;i<dataLen;++i)
    {   
        snprintf(&(charData[pos]),3,"%02x",*(char*)(data+i));
        pos += 2;
    }   
    return charData.c_str();
}


#ifdef _WINDOWS
int strcasecmp(const char *s1, const char *s2)
{
    while  (toupper((unsigned char)*s1) == toupper((unsigned char)*s2++))
        if (*s1++ == '\0') return 0;
    return(toupper((unsigned char)*s1) - toupper((unsigned char)*--s2));
}

int strncasecmp(const char *s1, const char *s2, int n)
{
    while (--n >= 0 && toupper((unsigned char)*s1) == toupper((unsigned char)*s2++))
        if (*s1++ == '\0')  return 0;
    return(n < 0 ? 0 : toupper((unsigned char)*s1) - toupper((unsigned char)*--s2));
}
#endif

#ifdef _WINDOWS
void* __builtin_return_address (int iLevel)
{
//     __asm
//     {
//         push ebx;
// 
//         mov eax, ebp;
//         mov ebx, DWORD PTR [esp + 8]; // iLevel
// 
// __Next:
//         test ebx, ebx;
//         je  __break;
//         dec ebx;
//         mov eax, DWORD PTR [eax];
//         jmp __Next;
// __break:
// 
//         mov eax, DWORD PTR [eax + 4];
//         pop ebx;
//         ret;
//     }
    return nullptr;
} 

#endif






