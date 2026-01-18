#include "Utils.h"

std::string utf8_to_gbk(const char* utf8_str) {
    // 第一步：UTF8转宽字符
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, nullptr, 0);
    wchar_t* wstr = new wchar_t[wlen];
    MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wstr, wlen);
    
    // 第二步：宽字符转GBK（CP_ACP）
    int len = WideCharToMultiByte(CP_ACP, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    char* cstr = new char[len];
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, cstr, len, nullptr, nullptr);
    
    // 用cstr构造string（会复制数据），然后释放临时内存
    std::string gbk_str(cstr);
    delete[] wstr;
    delete[] cstr;
    
    return gbk_str;  // 返回string副本，安全有效
}

std::string gbk_to_utf8(const char* gbk_str) {
    // 第一步：GBK转宽字符（使用CP_ACP代表系统默认ANSI编码，通常为GBK）
    int wlen = MultiByteToWideChar(CP_ACP, 0, gbk_str, -1, nullptr, 0);
    wchar_t* wstr = new wchar_t[wlen];
    MultiByteToWideChar(CP_ACP, 0, gbk_str, -1, wstr, wlen);
    
    // 第二步：宽字符转UTF-8
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    char* cstr = new char[len];
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, cstr, len, nullptr, nullptr);
    
    // 构造结果字符串并释放临时内存
    std::string utf8_str(cstr);
    delete[] wstr;
    delete[] cstr;
    
    return utf8_str;
}

std::string getFileName(const char* filePath){
    //拆分取文件名 不包含文件后缀
    //如果输入是文件夹路径 就会返回文件夹名
    std::string path(filePath);

    // 1. 提取文件名（含扩展名）：从最后一个路径分隔符（/或\）后面开始取
    size_t lastSep = path.find_last_of("/\\");
    std::string filename = (lastSep != std::string::npos) 
        ? path.substr(lastSep + 1) 
        : path; // 若没有分隔符，直接用完整路径作为文件名
    // 2. 去除扩展名：从最后一个点（.）前面截止
    size_t lastDot = filename.find_last_of(".");
    std::string nameWithoutExt = (lastDot != std::string::npos)
        ? filename.substr(0, lastDot)
        : filename; // 若没有扩展名，直接用文件名
    return nameWithoutExt;
}