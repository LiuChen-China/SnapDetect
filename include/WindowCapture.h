#pragma once
// 包含必要的Windows API和图形接口头文件
#include <dwmapi.h>       // 桌面窗口管理器API
#include <windows.h>      // Windows基础API
#include <vector>         // STL向量容器
#include <string>         // 字符串处理
#include <iostream>       // 输入输出流
#include <d3d11.h>        // Direct3D 11图形API
#include <dxgi1_2.h>      // DXGI 1.2接口(用于桌面复制)
#include <stdio.h>        // 标准输入输出
#include <opencv2/opencv.hpp> // OpenCV计算机视觉库
#include <chrono>         // 新增：时间相关（用于延迟）
#include <thread>         // 新增：线程休眠
#include <unordered_map>  // 新增：窗口句柄缓存

// 链接必要的库文件
#pragma comment(lib, "d3d11.lib")   // Direct3D 11库
#pragma comment(lib, "dxgi.lib")    // DXGI库
#pragma comment(lib, "Dwmapi.lib")  // 桌面窗口管理器库

#include <time.h>         // 时间函数
#include "Utils.h"        // 包含工具函数头文件

// 窗口数据结构，存储窗口句柄和标题
struct WindowData {
    HWND handle;         // 窗口句柄
    char title[256];     // 窗口标题(最大255字符+null终止符)
};

// 窗口矩形结构，存储位置和尺寸
struct WindowRect {
    int x;              // 窗口左上角X坐标
    int y;              // 窗口左上角Y坐标
    int width;         // 窗口宽度
    int height;        // 窗口高度
};

// 图像信息结构，用于返回窗口截图及其相关信息
struct MatInfo {
    uchar* pMatUchar;  // 指向OpenCV图像数据的指针
    int windowX;       // 窗口在桌面的x坐标
    int windowY;       // 窗口在桌面的Y坐标
    int windowWidth;   // 窗口宽度
    int windowHeight;  // 窗口高度
};

// 屏幕捕获器类，提供桌面和窗口截图功能
class ScreenCapturer {
public:
    ScreenCapturer();   // 构造函数
    ~ScreenCapturer();  // 析构函数

    // 初始化DXGI资源，创建必要的Direct3D设备和桌面复制接口
    bool initialize();

    // 获取整个桌面的截图，返回OpenCV的Mat对象
    cv::Mat getDesktopMat();

    // 获取指定窗口的截图，可以通过窗口标题部分字符串匹配
    // 参数titleSection: 窗口标题包含的字符串片段，默认为"screen"(表示整个桌面)
    cv::Mat getWindowMat(const std::string& titleSection = "screen");

    // 获取窗口截图信息，返回包含图像数据和窗口位置信息的MatInfo结构
    MatInfo getWindowMatInfo(const std::string titleSection = "screen");

    // 获取指定窗口的矩形区域信息
    WindowRect getWindowRect(const std::string& titleSection);

    //获取桌面矩形区域矩阵
    cv::Mat getDesktopAreaMat(const WindowRect& rect);

private:
    // 枚举窗口回调函数，用于收集所有可见窗口的信息
    static BOOL CALLBACK enumWindowsCallback(HWND hwnd, LPARAM lParam);

    // 根据窗口标题片段获取窗口句柄
    HWND getWindowHWND(const std::string& titleSection);

    // 获取窗口的位置和大小(考虑窗口边框)
    RECT getWindowLoc(HWND hwnd);

    // 获取当前显示器的缩放因子(用于高DPI显示)
    double getZoomFactor();

    // 释放所有分配的资源
    void releaseResources();
    
    // DXGI相关资源
    ID3D11Device* m_pDevice;              // Direct3D 11设备
    ID3D11DeviceContext* m_pDeviceContext; // Direct3D 11设备上下文
    IDXGIOutputDuplication* m_pOutputDuplication; // 桌面复制接口
    ID3D11Texture2D* m_pDesktopTexture;    // 桌面纹理
    ID3D11Texture2D* m_pStagingTexture;    // 暂存纹理(用于CPU访问)
    ID3D11Texture2D* m_pAreaStagingTexture; // 新增：区域截图专用暂存纹理

    // 桌面尺寸
    int m_desktopWidth;    // 桌面宽度(考虑缩放因子)
    int m_desktopHeight;   // 桌面高度(考虑缩放因子)
    int m_lastAreaWidth;   // 新增：记录上次区域截图的宽度
    int m_lastAreaHeight;  // 新增：记录上次区域截图的高度

    // 窗口列表，存储所有可见窗口的句柄和标题
    std::vector<WindowData> m_windowDatas;

    // 当前窗口矩形，记录最近获取的窗口位置和尺寸
    WindowRect m_currentWindowRect;

    // 图像缓存
    cv::Mat m_desktopMat;  // 桌面截图缓存
    cv::Mat m_windowMat;   // 窗口截图缓存
    cv::Mat m_areaMat;     // 桌面区域截图缓存

    // 缩放因子，用于高DPI显示适配
    double m_zoomFactor;
};

// 外部调用函数声明（保持不变）
MatInfo getWindowMatInfo(const char* titleSection="screen");
cv::Mat getDesktopAreaMat(const WindowRect& rect);