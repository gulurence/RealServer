#pragma once

#include "xTools.h"

/// @brief An extension of STL's string providing additional functionality that is often availiable in
///        higher-level languages such as C#, php, Python.
/// www.gotroot.ca/ext_string/
class StringTool
{
private:
    StringTool() {}
public:
    const static std::string WHITE_SPACE;
    const static int DEFAULT_BUFFER_SIZE = 1024;
public:
    static std::string& format(std::string& resultOUT, const char* fmt, ...);
    static std::string format(const char* fmt, ...);
    static std::string& vformat(std::string& resultOUT, const char* fmt, va_list argptr, int default_size = DEFAULT_BUFFER_SIZE);
    static inline std::string vformat(const char* fmt, va_list argptr, int default_size = DEFAULT_BUFFER_SIZE) {
        std::string resultOUT;
        return vformat(resultOUT, fmt, argptr, default_size);
    }
    static bool format_index(std::string& resultOUT, const char* fmt, const std::vector<std::string>& argList);
    static inline bool format_index(std::string& resultOUT, const char* fmt) {
        return format_index(resultOUT, fmt, std::vector<std::string>());
    }
    template<typename Arg0>
    static bool format_index(std::string& resultOUT, const char* fmt, Arg0 arg0) {
        std::vector<std::string> args(1);
        tostring(args[0], arg0);
        return format_index(resultOUT, fmt, args);
    }
    template<typename Arg0, typename Arg1>
    static bool format_index(std::string& resultOUT, const char* fmt, Arg0 arg0, Arg1 arg1) {
        std::vector<std::string> args(2);
        tostring(args[0], arg0); tostring(args[1], arg1);
        return format_index(resultOUT, fmt, args);
    }
    template<typename Arg0, typename Arg1, typename Arg2>
    static bool format_index(std::string& resultOUT, const char* fmt, Arg0 arg0, Arg1 arg1, Arg2 arg2) {
        std::vector<std::string> args(3);
        tostring(args[0], arg0); tostring(args[1], arg1); tostring(args[2], arg2);
        return format_index(resultOUT, fmt, args);
    }
    template<typename Arg0, typename Arg1, typename Arg2, typename Arg3>
    static bool format_index(std::string& resultOUT, const char* fmt, Arg0 arg0, Arg1 arg1, Arg2 arg2, Arg3 arg3) {
        std::vector<std::string> args(4);
        tostring(args[0], arg0); tostring(args[1], arg1); tostring(args[2], arg2); tostring(args[3], arg3);
        return format_index(resultOUT, fmt, args);
    }
    template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
    static bool format_index(std::string& resultOUT, const char* fmt, Arg0 arg0, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4) {
        std::vector<std::string> args(5);
        tostring(args[0], arg0); tostring(args[1], arg1); tostring(args[2], arg2); tostring(args[3], arg3); tostring(args[4], arg4);
        return format_index(resultOUT, fmt, args);
    }
    template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
    static bool format_index(std::string& resultOUT, const char* fmt, Arg0 arg0, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5) {
        std::vector<std::string> args(6);
        tostring(args[0], arg0); tostring(args[1], arg1); tostring(args[2], arg2); tostring(args[3], arg3); tostring(args[4], arg4); tostring(args[5], arg5);
        return format_index(resultOUT, fmt, args);
    }
    template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6>
    static bool format_index(std::string& resultOUT, const char* fmt, Arg0 arg0, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6) {
        std::vector<std::string> args(7);
        tostring(args[0], arg0); tostring(args[1], arg1); tostring(args[2], arg2); tostring(args[3], arg3); tostring(args[4], arg4); tostring(args[5], arg5); tostring(args[6], arg6);
        return format_index(resultOUT, fmt, args);
    }
    template<typename Arg0, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5, typename Arg6, typename Arg7>
    static bool format_index(std::string& resultOUT, const char* fmt, Arg0 arg0, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5, Arg6 arg6, Arg7 arg7) {
        std::vector<std::string> args(8);
        tostring(args[0], arg0); tostring(args[1], arg1); tostring(args[2], arg2); tostring(args[3], arg3); tostring(args[4], arg4); tostring(args[5], arg5); tostring(args[6], arg6); tostring(args[7], arg7);
        return format_index(resultOUT, fmt, args);
    }

    static inline std::string& tostring(std::string& resultOUT, char c) { resultOUT = c; return resultOUT; }
    static inline std::string& tostring(std::string& resultOUT, const char* pstr) { resultOUT = pstr == NULL ? "" : pstr; return resultOUT; }
    static inline std::string& tostring(std::string& resultOUT, const std::string& str) { resultOUT = str; return resultOUT; }
    static inline std::string& tostring(std::string& resultOUT, bool value) { resultOUT = value ? "true" : "false"; return resultOUT; }
    template<typename ValueType>
    static inline std::string& tostring(std::string& resultOUT, ValueType value) {
        std::stringstream oss;
        oss << value;
        resultOUT = oss.str();
        return resultOUT;
    }

    static std::string& append(std::string& str, const char* fmt, ...);
    static inline std::string& append(std::string& str, const std::string& cat) {
        str += cat;
        return str;
    }
    static inline std::string& append(std::string& str, char c) {
        str += c;
        return str;
    }
    static inline std::string& append(std::string& str, bool value) {
        str += value ? "true" : "false";
        return str;
    }
    template<typename ValueType>
    static inline std::string& append(std::string& str, ValueType value) {
        std::stringstream oss;
        oss << value;
        str += oss.str();
        return str;
    }
    static inline std::string trimbegin(std::string& str, const std::string& whiteSpace = WHITE_SPACE) {
        return str.erase(0, str.find_first_not_of(whiteSpace));
    }
    static inline std::string trimend(std::string& str, const std::string& whiteSpace = WHITE_SPACE) {
        return str.erase(str.find_last_not_of(whiteSpace) + 1);
    }
    static inline std::string trim(std::string& str, const std::string& whiteSpace = WHITE_SPACE) {
        str.erase(str.find_last_not_of(whiteSpace) + 1);
        return str.erase(0, str.find_first_not_of(whiteSpace));
    }
    static inline bool contains(const std::string& str, const std::string& tosearch) {
        return str.find(tosearch) != std::string::npos;
    }
    static inline bool contains(const std::string& str, const char* tosearch) {
        if (tosearch == NULL)
            return false;
        return str.find(tosearch) != std::string::npos;
    }
    static inline bool startswith(const std::string& str, const std::string& needle) {
        return str.compare(0, needle.size(), needle) == 0;
    }
    static inline bool endwith(const std::string& str, const std::string& needle) {
        std::string::size_type size = needle.size();
        return str.compare(str.size() - size, size, needle) == 0;
    }
    static inline std::string& padleft(std::string& str, std::string::size_type size, std::string::value_type paddingchar = ' ') {
        if (size > str.size())
            str.insert(0, size - str.size(), paddingchar);
        return str;
    }
    static inline std::string& padright(std::string& str, std::string::size_type size, std::string::value_type paddingchar = ' ') {
        if (size > str.size())
            str.resize(size, paddingchar);
        return str;
    }
    template <typename StringListContainerFrom, typename StringListContainerTo>
    static void unique(const StringListContainerFrom& strList, StringListContainerTo& resultOUT) {
        resultOUT.clear();
        resultOUT.reserve(strList.size());
        for (typename StringListContainerFrom::const_iterator it_from = strList.begin();
            it_from != strList.end(); ++it_from) {
            bool repeat = false;
            for (typename StringListContainerFrom::const_iterator it_to = resultOUT.begin();
                it_to != resultOUT.end(); ++it_to) {
                if (it_from->compare(*it_to) == 0) {
                    repeat = true;
                    break;
                }
            }
            if (repeat == false)
                resultOUT.push_back(*it_from);
        }
    }
    template<typename StringListContainer>
    static void unique(StringListContainer& strList) {
        StringListContainer result;
        unique(strList, result);
        if (strList.size() != result.size())
            strList.assign(result.begin(), result.end());
    }

    static void split(const std::string& str, std::string::value_type separator, std::vector<std::string>& resultOUT, size_t resultLimit = -1);
    static void split(const std::string& str, const std::string& separator, std::vector<std::string>& resultOUT, size_t resultLimit = -1);
    template<typename StringListContainer>
    static void split(const std::string& str, const StringListContainer& separatorList,
        std::vector<std::string>& resultOUT, size_t resultLimit = -1) {
        typedef std::vector<std::string> container;

        resultOUT.clear();
        if (resultLimit == 0) {
            return;
        }
        std::vector<std::string> splitSeparatorList;
        unique(separatorList, splitSeparatorList);
        if (separatorList.empty()) {
            resultOUT.push_back(str);
            return;
        }

        std::string::size_type begin = 0;
        for (std::string::size_type i = 0; i < str.size(); i++) {
            for (container::const_iterator it = splitSeparatorList.begin(); it != splitSeparatorList.end(); ++it) {
                if (it->compare(0, it->size(), str, i, it->size()) == 0) {
                    if (i != begin) {
                        resultOUT.push_back(str.substr(begin, i - begin));
                        if (resultOUT.size() == resultLimit)
                            return;
                    }
                    begin = i + it->size();
                    i = begin - 1;
                    break;
                }
            }
        }
        if (begin < str.size() && resultOUT.size() < resultLimit)
            resultOUT.push_back(str.substr(begin));
        assert(resultOUT.size() <= resultLimit);
    }
    static void split_white_space(const std::string& str, std::vector<std::string>& resultOUT, size_t resultLimit = -1);
    static bool split_fix_size(const std::string& str, std::string::size_type fix_size, std::vector<std::string>& resultOUT);
    template <class InputIterator>
    static inline std::string& join(const std::string& glue, InputIterator first, InputIterator last, std::string& resultOUT) {
        resultOUT.clear();
        if (first == last)
            return resultOUT;
        for (; first != last; first++) {
            resultOUT.append(*first);
            resultOUT.append(glue);
        }
        resultOUT.erase(resultOUT.size() - glue.size());
        return resultOUT;
    }
    template <class InputIterator>
    static inline std::string join(const std::string& glue, InputIterator first, InputIterator last) {
        std::string resultOUT;
        return join(glue, first, last);
    }
    static std::string& replace(std::string& str, const std::string& from, const std::string& to);
    static std::string& replace(std::string& str, std::string::value_type from, std::string::value_type to);
    static long int integer(const std::string& str);
    static bool integer(const std::string& str, long int& valueOut);
    static std::string& repeat(const std::string& str, std::string::size_type times, std::string& resultOUT);
    static inline std::string repeat(const std::string& str, std::string::size_type times) {
        std::string resultOUT;
        return repeat(str, times, resultOUT);
    }
    static std::string& tolower(std::string& str);
    static inline char tolower(char c) { return is_upper(c) ? c - 'A' + 'a' : c; }
    static std::string& toupper(std::string& str);
    static inline char toupper(char c) { return is_lower(c) ? c - 'a' + 'A' : c; }
    static std::string& swapcase(std::string& str);
    static inline char swapcase(char c) {
        if (is_upper(c)) return c - 'A' + 'a';
        if (is_lower(c)) return c - 'a' + 'A';
        return c;
    }
    static bool is_lower(const std::string& str);
    static inline bool is_lower(char c) { return c >= 'a' && c <= 'z'; }
    static bool is_upper(const std::string& str);
    static inline bool is_upper(char c) { return c >= 'A' && c <= 'Z'; }
    static bool is_alpha(const std::string& str);
    static inline bool is_alpha(char c) { return is_lower(c) || is_upper(c); }
    static bool is_numeric(const std::string& str);
    static inline bool is_numeric(char c) { return c >= '0' && c <= '9'; }
    static bool is_alnum(const std::string& str);
    static inline bool is_alnum(char c) { return is_alpha(c) || is_numeric(c); }
    static inline bool is_whitespace(char c) { return WHITE_SPACE.find(c) != std::string::npos; }
    static std::string::size_type count(const std::string& str, const std::string& module);
    static inline int compare(const char* str1, const char* str2) {
        if (str1 == NULL)
            if (str2 == NULL) return 0; else    return -1;
        else
            if (str2 == NULL) return 1; else return strcmp(str1, str2);
    }
    static inline int compare(const char* str1, const std::string& str2) {
        return -compare(str2, str1);
    }
    static inline int compare(const std::string& str1, const char* str2) {
        if (str2 == NULL) return 1; else return str1.compare(str2);
    }
    static inline int compare(const std::string& str1, const std::string& str2) {
        return str1.compare(str2);
    }
    static inline int compare(char c1, char c2) {
        if (c1 > c2)
            return 1;
        else if (c1 < c2)
            return -1;
        else
            return 0;
    }
    static int icompare(const std::string& str1, const std::string& str2);
    static inline int icompare(const char* str1, const char* str2) {
        if (str1 == str2)
            return 0;
        if (str1 == NULL)
            return -1;
        if (str2 == NULL)
            return 1;

        return strcasecmp(str1, str2);
    }
    static inline int icompare(char c1, char c2) {
        if (c1 == c2)
            return 0;
        if (is_alpha(c1))
            c1 = tolower(c1);
        if (is_alpha(c2))
            c2 = tolower(c2);
        if (c1 > c2)
            return 1;
        else// if(c1 < c2)
            return -1;
    }
    static int compare_nature(const char* str1, const char* str2, bool case_sensitive = true);
    static inline int compare_nature(const std::string& str1, const std::string& str2, bool case_sensitive = true) {
        return compare_nature(str1.c_str(), str2.c_str(), case_sensitive);
    }

    static bool match_wildcard(const char* str, const char* wildcard);

    static inline bool empty(const char* pstr) { return pstr == NULL || pstr[0] == '\0'; }
    static inline bool empty(const std::string& str) { return str.empty(); }
    static inline bool empty(char c) { return c == '\0'; }

    static inline size_t strlen(const char* pstr) { return empty(pstr) ? 0 : ::strlen(pstr); }
    static inline size_t strlen(const std::string& str) { return str.size(); }


    static bool try_parse_int(const char* str, int* ret, char** end);

    template<typename CharType>
    static inline int hash_code(const CharType* str) {
        unsigned int hash = 0;
        unsigned int x = 0;
        while (*str) {
            hash = (hash << 4) + (*str++);
            if ((x == hash & 0xF0000000L) != 0) {
                hash ^= (x >> 24);
                hash &= ~x;
            }
        }
        return (hash & 0x7FFFFFFF);
    }
};



