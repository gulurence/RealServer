#include "xStringTool.h"
#include <cstdio>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

/// @brief 空白字符 依次为: 空格, 水平制表符, 回车, 换行, 走纸换行, 垂直制表符
const std::string StringTool::WHITE_SPACE = " \t\r\n\f\v";


std::string& StringTool::format(std::string& resultOUT, const char* fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    vformat(resultOUT, fmt, vl);
    va_end(vl);
    return resultOUT;
}
std::string StringTool::format(const char* fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    std::string resultOUT;
    vformat(resultOUT, fmt, vl);
    va_end(vl);
    return resultOUT;
}
std::string& StringTool::vformat(std::string& resultOUT, const char *fmt, va_list argptr, int default_size /*= DEFAULT_BUFFER_SIZE*/)
{
    std::vector<char> buffer(default_size, 0);
    for(;;)
    {
        int len = vsnprintf(&(buffer[0]), buffer.size(), fmt, argptr);
        if(len == -1)
        {
            buffer.resize(buffer.size() * 2, 0);
            continue;
        }
        if(len == static_cast<int>(buffer.size()))
            buffer.push_back(0);
        break;
    }
    resultOUT.assign(&(buffer[0]));
    return resultOUT;
}
bool StringTool::format_index(std::string& resultOUT, const char* fmt, const std::vector<std::string>& argList)
{
    // 解析状态机的状态
    enum FormatState
    {
        FS_NORMAL,
        FS_R_BRACKE,
        FS_L_BRACKE,
        FS_NUMBER,
    };

    // 出错处理代码
#ifdef _DEBUG
#define FS_ERROR_MESSAGE(msg) \
    do{\
        resultOUT = (msg);\
        return false;\
    } while(0)
#else
#define FS_ERROR_MESSAGE(msg) \
    do{\
        resultOUT.clear();\
        return false;\
    } while(0)
#endif

    // 预处理
    resultOUT.clear();
    if(fmt == NULL || fmt[0] == 0)
        return true;
    // 正式解析
    const char* p = fmt;
    FormatState state = FS_NORMAL;
    std::string index;
    for(;;)
    {
        const char c = *p++;
        if(c == 0)
        {
            if(state == FS_NORMAL)
                break; // OK 算法执行完毕
            else
                FS_ERROR_MESSAGE("ERROR！格式化字符末尾可能有残缺，不能正确解析");
        }
        switch(state)
        {
            case FS_NORMAL:
                switch(c)
                {
                    case '}':
                        state = FS_R_BRACKE;
                        break;
                    case '{':
                        state = FS_L_BRACKE;
                        break;
                    default:
                        resultOUT += c;
                        break;
                }
                break;
            case FS_R_BRACKE:
                if(c == '}')
                {
                    state = FS_NORMAL;
                    resultOUT += '}';
                }
                else
                {
                    FS_ERROR_MESSAGE("ERROR！在格式化字符串中发现不成对的\'}\'字符");
                }
                break;
            case FS_L_BRACKE:
                if(c == '{')
                {
                    resultOUT += '}';
                    state = FS_NORMAL;
                }
                else if(is_numeric(c))
                {
                    index = c;
                    state = FS_NUMBER;
                }
                else
                {
                    FS_ERROR_MESSAGE("ERROR！在格式化字符串中发现\'{\'后的非数字字符 ");
                }
                break;
            case FS_NUMBER:
                if(c == '}')
                {
                    long int idx = 0;
                    if(integer(index, idx))
                    {
                        if(idx >= 0 && static_cast<size_t>(idx) < argList.size())
                        {
                            resultOUT += argList[idx];
                            state = FS_NORMAL;
                        }
                        else
                        {
                            FS_ERROR_MESSAGE("ERROR！在格式化字符串中的参数下标超出参数列表范围 ");
                        }
                    }
                    else
                    {
                        FS_ERROR_MESSAGE("ERROR！在格式化字符串中的参数下标不能正确转换为数字 ");
                    }
                }
                else if(is_numeric(c))
                {
                    index += c;
                }
                else
                {
                    FS_ERROR_MESSAGE("ERROR！在格式化字符串中发现\'{\'后的非数字字符 ");
                }
                break;
            default:
                assert(0);//所有状态签名已经包含，程序不应该执行到此处
                break;
        }
    }
    return true;
}
std::string& StringTool::append(std::string& str, const char* fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    std::string temp;
    vformat(temp, fmt, vl);
    va_end(vl);
    str += temp;
    return str;
}
void StringTool::split(const std::string& str, std::string::value_type separator, std::vector<std::string>& resultOUT, size_t resultLimit /*= -1*/)
{
    resultOUT.clear();
    if(resultLimit == 0) 
        return;
    resultOUT.push_back(std::string());
    for (std::string::const_iterator it = str.begin(); it != str.end(); ++it)
    {
        if(*it == separator)
        {
            if(resultOUT.back().empty() == false)
            {
                if(resultOUT.size() == resultLimit) 
                    return;
                resultOUT.push_back(std::string());
            }
        }
        else
        {
            resultOUT.back().push_back(*it);
        }
    }
    if(resultOUT.back().empty())
        resultOUT.pop_back();
    assert(resultOUT.size() <= resultLimit);
}
void StringTool::split(const std::string& str, const std::string& separator, std::vector<std::string>& resultOUT, size_t resultLimit/* = -1*/)
{
    resultOUT.clear();
    if(resultLimit == 0) 
        return;
    if(separator.empty())
    {
        resultOUT.push_back(str);
        return;
    }
    std::string::size_type begin = 0, index;
    for(;;)
    {
        index = str.find(separator, begin);
        if(index == std::string::npos)
            break;
        size_t len = index - begin;
        if(len != 0)
        {
            resultOUT.push_back(str.substr(begin, len));
            if(resultOUT.size() == resultLimit)
                return;
        }
        begin = index + separator.size();
    }
    if(begin < str.size() && resultOUT.size() < resultLimit)
        resultOUT.push_back(str.substr(begin));
    assert(resultOUT.size() <= resultLimit);
}
void StringTool::split_white_space(const std::string& str, std::vector<std::string>& resultOUT, size_t resultLimit/* = -1*/)
{
    std::vector<std::string> white_space;
    white_space.push_back(" ");
    white_space.push_back("\t");
    white_space.push_back("\r");
    white_space.push_back("\n");
    white_space.push_back("\f");
    white_space.push_back("\v");
    split(str, white_space, resultOUT, resultLimit);
}
bool StringTool::split_fix_size(const std::string& str, std::string::size_type fix_size, std::vector<std::string>& resultOUT)
{
    resultOUT.clear();
    if(fix_size == 0)//长度为0时无法处理
        return false;
    else if(fix_size == 1)// 当长度为1时不能处理汉字
    {
        for(std::string::const_iterator iter = str.begin(); iter != str.end(); ++iter)
        {
            if(*iter < 0)
                return false;
        }
    }
    else if(fix_size >= str.size())//长度足够大时不必要处理
    {
        resultOUT.push_back(str);
        return true;
    }
    resultOUT.reserve(str.size() / fix_size + 1);

    bool fullword = true;// 是否是完整字的标志，用于处理完整的汉字
    std::string::const_iterator it = str.begin();
    std::string::const_iterator last = it;
    for (; it != str.end(); ++it)
    {
        if(*it >= 0)
            fullword = true;
        else
            fullword = !fullword;
        if (it - last == static_cast<int>(fix_size))
        {
            // 发现分界处有汉字断字现象则回退一个字节
            if(fullword && *it < 0)
            {
                --it;
                fullword = false;
            }
            resultOUT.push_back(std::string(last, it));
            last = it;
        }
    }

    if (last != it)
        resultOUT.push_back(std::string(last, it));
    return true;
}
std::string& StringTool::replace(std::string& str, const std::string& from, const std::string& to)
{
    std::string::size_type lastpos = 0, thispos;
    while ((thispos = str.find(from, lastpos)) != std::string::npos)
    {
        str.replace(thispos, from.length(), to);
        lastpos = thispos + 1;
    }
    return str;
}
std::string& StringTool::replace(std::string& str, std::string::value_type from, std::string::value_type to)
{
    for (std::string::iterator i = str.begin(); i != str.end(); i++)
    {
        if (*i == from)
        {
            *i = to;
        }
    }
    return str;
}
long int StringTool::integer(const std::string& str)
{
    long int retval = 0;
    integer(str, retval);
    return retval;
}

bool StringTool::integer(const std::string& str, long int& valueOut)
{
    valueOut = 0;
    if(str.empty())
        return false;

    bool neg = false;
    bool error = false;
    for (std::string::const_iterator i = str.begin(); i != str.end(); i++)
    {
        if (i == str.begin())
        {
            if (*i == '-')
            {
                neg = true;
                continue;
            }
            else if (*i == '+')
                continue;
        }
        if (*i >= '0' && *i <= '9')
        {
            valueOut *= 10;
            valueOut += *i - '0';
        }
        else
        {
            error = true;
            break;
        }
    }
    if(neg)
        valueOut = -valueOut;
    return !error;
}

std::string& StringTool::repeat(const std::string& str, std::string::size_type times, std::string& resultOUT)
{
    resultOUT.clear();
    for (std::string::size_type i = 0; i < times; i++)
    {
        resultOUT.append(str);
    }
    return resultOUT;
}

std::string& StringTool::tolower(std::string& str)
{
    for (std::string::iterator i = str.begin(); i != str.end(); i++)
    {
        if (*i >= 'A' && *i <= 'Z')
            *i = (*i) + ('a' - 'A');
    }
    return str;
}

std::string& StringTool::toupper(std::string& str)
{
    for (std::string::iterator i = str.begin(); i != str.end(); i++)
    {
        if (*i >= 'a' && *i <= 'z')
            *i = (*i) - ('a' - 'A');
    }
    return str;
}
std::string& StringTool::swapcase(std::string& str)
{
    for (std::string::iterator i = str.begin(); i != str.end(); i++)
    {
        if (*i >= 'A' && *i <= 'Z')
            *i += ('a' - 'A');
        else if (*i >= 'a' && *i <= 'z')
            *i -= ('a' - 'A');
    }
    return str;
}

bool StringTool::is_lower(const std::string& str)
{
    if (str.empty())
        return false;

    for (std::string::const_iterator i = str.begin(); i != str.end(); i++)
    {
        if (*i < 'a' || *i < 'z')
            return false;
    }

    return true;
}

bool StringTool::is_upper(const std::string& str)
{
    if (str.empty())
        return false;

    for (std::string::const_iterator i = str.begin(); i != str.end(); i++)
    {
        if (*i < 'A' || *i > 'Z')
            return false;
    }

    return true;
}

bool StringTool::is_alpha(const std::string& str)
{
    if (str.empty())
        return false;

    for (std::string::const_iterator i = str.begin(); i != str.end(); i++)
    {
        if (*i < 'A' || (*i > 'Z' && (*i < 'a' || *i > 'z')))
            return false;
    }
    return true;
}

bool StringTool::is_numeric(const std::string& str)
{
    if (str.empty())
        return false;

    for (std::string::const_iterator i = str.begin(); i != str.end(); i++)
    {
        if (*i < '0' || *i > '9')
            return false;
    }
    return true;
}
bool StringTool::is_alnum(const std::string& str)
{
    if (str.empty())
        return false;

    for (std::string::const_iterator i = str.begin(); i != str.end(); i++)
    {
        if (*i < 'A' || *i > 'Z')
            if (*i < '0' || *i > '9')
                if (*i < 'a' || *i > 'z')
                    return false;
    }
    return true;
}

std::string::size_type StringTool::count(const std::string& str, const std::string &module)
{
    std::string::size_type count = 0, last = 0, cur = 0;

    while ((cur = str.find(module, last)) != std::string::npos)
    {
        count++;
        last = cur + module.size();
    }
    return count;
}

int StringTool::icompare(const std::string& str1, const std::string& str2)
{
    int ret = (int)str1.size() - (int)str2.size();
    if(ret != 0)
        return ret;
    std::string::const_iterator it1 = str1.begin();
    std::string::const_iterator it2 = str2.begin();
    if(it1 == it2)
        return 0;

    for(; it1 < str1.end()/* && it2 < str2.end()*/; ++it1, ++it2)
    {
        if(*it1 == *it2)
            continue;
        char c1 = *it1;
        char c2 = *it2;
        if(c1 >= 'A' && c1 <= 'Z')
        {
            if(c2 >= 'a' && c2 <= 'z')
            {
                if(c1 - 'A' == c2 - 'a')
                    continue;
            }
        }
        else if(c1 >= 'a' && c1 <= 'z')
        {
            if(c2 >= 'A' && c2 <= 'Z')
            {
                if(c1 - 'a' == c2 - 'A')
                    continue;
            }
        }
        return c1 - c2;
    }
    return 0;
}

namespace detail
{
    static int compare_right(const char *a, const char *b)
    {
        int bias = 0;

        /* The longest run of digits wins.  That aside, the greatest
           value wins, but we can't know that it will until we've scanned
           both numbers to know that they have the same magnitude, so we
           remember it in BIAS. */
        for (;; a++, b++) 
        {
            if (!StringTool::is_numeric(*a)  &&  !StringTool::is_numeric(*b))
                return bias;
            else if (!StringTool::is_numeric(*a))
                return -1;
            else if (!StringTool::is_numeric(*b))
                return +1;
            else if (*a < *b)
            {
                if (!bias)
                    bias = -1;
            } 
            else if (*a > *b) 
            {
                if (!bias)
                    bias = +1;
            }
            else if (!*a  &&  !*b)
                return bias;
        }
    }


    static int compare_left(const char *a, const char *b)
    {
        /* Compare two left-aligned numbers: the first to have a
           different value wins. */
        for (;; a++, b++) 
        {
            if (!StringTool::is_numeric(*a)  &&  !StringTool::is_numeric(*b))
                return 0;
            else if (!StringTool::is_numeric(*a))
                return -1;
            else if (!StringTool::is_numeric(*b))
                return +1;
            else if (*a < *b)
                return -1;
            else if (*a > *b)
                return +1;
        }
    }
}

int StringTool::compare_nature(const char *str1, const char *str2, bool case_sensitive /*= true*/) 
{
    if(str1 == str2)
        return 0;
    if(str1 == NULL)
        return -1;
    else if(str2 == NULL)
        return 1;

    int ai(0), bi(0);
    char ca, cb;
    int fractional, result;
    for(;;)
    {
        ca = str1[ai]; cb = str2[bi];

        /* skip over leading spaces or zeros */
        while (StringTool::is_whitespace(ca))
            ca = str1[++ai];

        while (StringTool::is_whitespace(cb))
            cb = str2[++bi];

        /* process run of digits */
        if (StringTool::is_numeric(ca)  &&  StringTool::is_numeric(cb)) 
        {
            fractional = (ca == '0' || cb == '0');

            if (fractional) 
            {
                if ((result = detail::compare_left(str1+ai, str2+bi)) != 0)
                    return result;
            }
            else 
            {
                if ((result = detail::compare_right(str1+ai, str2+bi)) != 0)
                    return result;
            }
        }

        if (!ca && !cb)
        {
            /* The strings compare the same.  Perhaps the caller
               will want to call strcmp to break the tie. */
            return 0;
        }
        if(!case_sensitive)
        {
            ca = StringTool::toupper(ca);
            cb = StringTool::toupper(cb);
        }
        if (ca < cb)
            return -1;
        else if (ca > cb)
            return +1;

        ++ai; ++bi;
    }
}
bool StringTool::match_wildcard(const char* str, const char* wildcard)
{
    if(str == NULL || wildcard == NULL)
        return false;
    if(str == wildcard)
        return true;
    // Written by Jack Handy - jakkhandy@hotmail.com
    const char *cp = NULL, *mp = NULL;
    while ((*str) && (*wildcard != '*')) 
    {
        if ((*wildcard != *str) && (*wildcard != '?')) 
            return false;
        wildcard++;
        str++;
    }
    while (*str) 
    {
        if (*wildcard == '*') 
        {
            if (!*++wildcard) 
                return true;
            mp = wildcard;
            cp = str+1;
        } 
        else if ((*wildcard == *str) || (*wildcard == '?')) 
        {
            wildcard++;
            str++;
        }
        else 
        {
            wildcard = mp;
            str = cp++;
        }
    }

    while (*wildcard == '*') 
    {
        wildcard++;
    }
    return (*wildcard) ? false : true;
}

bool StringTool::try_parse_int(const char* str,int* ret,char** end)
{
    char* p = NULL;
    long int n = 0;
    if(str == NULL || ret == NULL)
        goto L_ErrorEnd;
    n = strtol(str,&p,0);
    if(n == LONG_MAX || n == LONG_MIN)
    {
        if(errno == ERANGE)
            goto L_ErrorEnd;
    }
    else if(n == 0)
    {
        if(p == str)
            goto L_ErrorEnd;
    }
    if(end != NULL)
        *end = p;
    *ret = n;
    return true;
L_ErrorEnd:
    if(end != NULL)
        *end = const_cast<char *>(str);
    if(ret != NULL)
        *ret = 0;
    return false;
}
