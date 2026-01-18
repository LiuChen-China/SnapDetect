#pragma once
#include <string>
#include <windows.h>      // Windows基础API
// 包含必要的Windows API和图形接口头文件
#include <windows.h>      // Windows基础API
#include <vector>         // STL向量容器
#include <string>         // 字符串处理
#include <iostream>       // 输入输出流
#include <stdio.h>        // 标准输入输出

// 转换UTF-8字符串为GBK编码
std::string utf8_to_gbk(const char* utf8_str);

// 转换GBK编码字符串为UTF-8
std::string gbk_to_utf8(const char* gbk_str);

//取出文件名（不包含文件后缀） 如果入参是文件夹路径 则返回文件夹名
std::string getFileName(const char* filePath);