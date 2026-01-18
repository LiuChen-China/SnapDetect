#include "WindowCapture.h"
#include <memory>
// 在文件开头添加
#include <mutex> // 添加线程同步支持
#include <exception>

// 构造函数，初始化所有成员变量
ScreenCapturer::ScreenCapturer()
    : m_pDevice(nullptr)          // Direct3D设备初始化为nullptr
    , m_pDeviceContext(nullptr)   // 设备上下文初始化为nullptr
    , m_pOutputDuplication(nullptr) // 桌面复制接口初始化为nullptr
    , m_pDesktopTexture(nullptr)  // 桌面纹理初始化为nullptr
    , m_pStagingTexture(nullptr)  // 暂存纹理初始化为nullptr
    , m_desktopWidth(0)           // 桌面宽度初始化为0
    , m_desktopHeight(0)          // 桌面高度初始化为0
    , m_zoomFactor(1.0) {         // 缩放因子初始化为1.0(无缩放)
    std::cout << "[ScreenCapturer] init instance" << std::endl;
}

// 析构函数，释放所有资源
ScreenCapturer::~ScreenCapturer() {
    releaseResources(); // 调用资源释放函数
}

// 初始化DXGI和Direct3D资源
bool ScreenCapturer::initialize() {
    releaseResources(); // 确保释放之前分配的资源

    // 获取系统缩放因子和调整后的桌面尺寸
    m_zoomFactor = getZoomFactor();
    m_desktopWidth = static_cast<int>(GetSystemMetrics(SM_CXSCREEN) * m_zoomFactor);
    m_desktopHeight = static_cast<int>(GetSystemMetrics(SM_CYSCREEN) * m_zoomFactor);

    // 初始化D3D设备，尝试多种驱动类型
    D3D_DRIVER_TYPE driverTypes[] = {
        D3D_DRIVER_TYPE_HARDWARE,  // 首选硬件驱动
        D3D_DRIVER_TYPE_WARP,      // 备选WARP驱动(软件渲染)
        D3D_DRIVER_TYPE_REFERENCE  // 参考驱动(最慢，仅用于调试)
    };
    UINT numDriverTypes = ARRAYSIZE(driverTypes);

    // 支持的Direct3D特性级别
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,  // Direct3D 11.0
        D3D_FEATURE_LEVEL_10_1,  // Direct3D 10.1
        D3D_FEATURE_LEVEL_10_0,  // Direct3D 10.0
        D3D_FEATURE_LEVEL_9_1    // Direct3D 9.1
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);

    D3D_FEATURE_LEVEL featureLevel; // 实际获得的特性级别
    HRESULT hr;                     // COM函数返回结果

    // 尝试不同的驱动类型创建D3D设备
    for (UINT i = 0; i < numDriverTypes; i++) {
        hr = D3D11CreateDevice(
            nullptr,                // 默认适配器
            driverTypes[i],          // 驱动类型
            nullptr,                // 软件驱动句柄(不使用)
            0,                     // 运行时层标志
            featureLevels,          // 支持的特性级别
            numFeatureLevels,       // 特性级别数量
            D3D11_SDK_VERSION,     // SDK版本
            &m_pDevice,             // 返回的设备指针
            &featureLevel,          // 返回的特性级别
            &m_pDeviceContext);     // 返回的设备上下文

        if (SUCCEEDED(hr))          // 如果创建成功则跳出循环
            break;
    }

    // 检查设备创建是否成功
    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D11 device. HRESULT: 0x" << std::hex << hr << std::endl;
        return false;
    }

    // 获取DXGI设备接口(用于桌面复制)
    IDXGIDevice* pDxgiDevice = nullptr;
    hr = m_pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&pDxgiDevice));
    if (FAILED(hr)) {
        std::cerr << "Failed to get DXGI device. HRESULT: 0x" << std::hex << hr << std::endl;
        return false;
    }

    // 获取DXGI适配器(代表显卡)
    IDXGIAdapter* pDxgiAdapter = nullptr;
    hr = pDxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&pDxgiAdapter));
    pDxgiDevice->Release(); // 释放DXGI设备接口
    if (FAILED(hr)) {
        std::cerr << "Failed to get DXGI adapter. HRESULT: 0x" << std::hex << hr << std::endl;
        return false;
    }

    // 获取主显示器输出(索引0)
    IDXGIOutput* pDxgiOutput = nullptr;
    hr = pDxgiAdapter->EnumOutputs(0, &pDxgiOutput);
    pDxgiAdapter->Release(); // 释放适配器接口
    if (FAILED(hr)) {
        std::cerr << "Failed to get DXGI output. HRESULT: 0x" << std::hex << hr << std::endl;
        return false;
    }

    // 获取输出描述信息
    DXGI_OUTPUT_DESC outputDesc;
    pDxgiOutput->GetDesc(&outputDesc);

    // 获取Output1接口(支持桌面复制功能)
    IDXGIOutput1* pDxgiOutput1 = nullptr;
    hr = pDxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&pDxgiOutput1));
    pDxgiOutput->Release(); // 释放输出接口
    if (FAILED(hr)) {
        std::cerr << "Failed to get DXGI output1. HRESULT: 0x" << std::hex << hr << std::endl;
        return false;
    }

    // 创建桌面复制接口(核心功能，用于捕获桌面图像)
    hr = pDxgiOutput1->DuplicateOutput(m_pDevice, &m_pOutputDuplication);
    pDxgiOutput1->Release(); // 释放Output1接口
    if (FAILED(hr)) {
        std::cerr << "Failed to create desktop duplication. HRESULT: 0x" << std::hex << hr << std::endl;
        return false;
    }
    getDesktopMat();//不加这行 第一次截图会是全黑色
    return true; // 所有初始化步骤成功完成
}

// 获取当前显示器的缩放因子(用于高DPI显示)
double ScreenCapturer::getZoomFactor() {
    // 获取桌面窗口和主显示器句柄
    HWND hWnd = GetDesktopWindow();
    HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);

    // 获取显示器信息
    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfo(hMonitor, &monitorInfo);

    // 计算逻辑分辨率(不考虑缩放)
    int cxLogical = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;

    // 获取显示设置(物理分辨率)
    DEVMODE dm;
    dm.dmSize = sizeof(dm);
    dm.dmDriverExtra = 0;
    EnumDisplaySettings(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &dm);
    int cxPhysical = dm.dmPelsWidth;

    // 计算缩放因子 = 物理分辨率 / 逻辑分辨率
    return cxPhysical * 1.0 / cxLogical;
}

// 枚举窗口回调函数，收集所有可见窗口的信息
BOOL CALLBACK ScreenCapturer::enumWindowsCallback(HWND hwnd, LPARAM lParam) {
    // 从lParam获取ScreenCapturer实例指针
    ScreenCapturer* pThis = reinterpret_cast<ScreenCapturer*>(lParam);

    // 检查窗口是否有效、启用且可见
    if (IsWindow(hwnd) && IsWindowEnabled(hwnd) && IsWindowVisible(hwnd)) {
        char windowText[256];
        GetWindowTextA(hwnd, windowText, sizeof(windowText)); // 获取窗口标题

        // 创建窗口数据并添加到列表
        WindowData windowData;
        windowData.handle = hwnd;
        strncpy_s(windowData.title, windowText, sizeof(windowData.title));
        pThis->m_windowDatas.push_back(windowData);
    }
    return TRUE; // 继续枚举
}

// 根据窗口标题片段获取窗口句柄
HWND ScreenCapturer::getWindowHWND(const std::string& titleSection) {
    m_windowDatas.clear(); // 清空窗口列表

    // 枚举所有窗口，使用回调函数收集信息
    EnumWindows(enumWindowsCallback, reinterpret_cast<LPARAM>(this));

    // 遍历收集到的窗口，查找标题包含指定字符串的窗口
    for (const auto& window : m_windowDatas) {
        if (strstr(window.title, titleSection.c_str()) != nullptr) {
            return window.handle; // 返回匹配的窗口句柄
        }
    }
    return nullptr; // 未找到匹配窗口
}

// 获取窗口的实际位置和大小(包括边框)
RECT ScreenCapturer::getWindowLoc(HWND hwnd) {
    RECT frame;
    // 使用DWM API获取窗口扩展边框(包括阴影等)
    DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof(RECT));
    return frame;
}

// 获取指定窗口的矩形区域(考虑缩放因子)
WindowRect ScreenCapturer::getWindowRect(const std::string& titleSection) {
    m_currentWindowRect = { 0, 0, 0, 0 }; // 初始化矩形

    // 如果请求的是整个桌面
    if (titleSection == "screen") {
        m_currentWindowRect = { 0, 0, m_desktopWidth, m_desktopHeight };
        return m_currentWindowRect;
    }

    // 获取窗口句柄
    HWND hwnd = getWindowHWND(titleSection);
    if (hwnd == nullptr) {
        std::cerr << "Cannot find window containing: " << titleSection << std::endl;
        return m_currentWindowRect;
    }

    // 如果窗口最小化，先恢复它
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }
    // 将窗口置于前台
    SetForegroundWindow(hwnd);

    // 获取窗口位置和大小
    RECT rect = getWindowLoc(hwnd);

    // 应用缩放因子计算实际像素坐标
    m_currentWindowRect = {
        static_cast<int>(rect.left * m_zoomFactor),      // X坐标
        static_cast<int>(rect.top * m_zoomFactor),       // Y坐标
        static_cast<int>((rect.right - rect.left) * m_zoomFactor),  // 宽度
        static_cast<int>((rect.bottom - rect.top) * m_zoomFactor)   // 高度
    };

    return m_currentWindowRect;
}

// 获取整个桌面的截图
cv::Mat ScreenCapturer::getDesktopMat() {
    // 如果桌面复制接口未初始化，则初始化
    if (!m_pOutputDuplication) {
        if (!initialize()) {
            return cv::Mat(); // 初始化失败返回空Mat
        }
    }

    DXGI_OUTDUPL_FRAME_INFO frameInfo; // 帧信息结构
    IDXGIResource* pDesktopResource = nullptr; // 桌面资源

    // 获取下一帧(超时时间（毫秒），0 表示立即返回，INFINITE 表示无限等待。)
    HRESULT hr = m_pOutputDuplication->AcquireNextFrame(100, &frameInfo, &pDesktopResource);

    // 处理获取帧的结果
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            // 超时则返回缓存的桌面图像(如果有)
            return m_desktopMat.empty() ? cv::Mat() : m_desktopMat.clone();
        }
        else if (hr == DXGI_ERROR_ACCESS_LOST) {
            // 访问丢失(如显示器分辨率改变)，重新初始化
            releaseResources();
            if (!initialize()) {
                return cv::Mat();
            }
            return getDesktopMat(); // 递归调用
        }
        else {
            // 其他错误
            std::cerr << "Failed to acquire next frame. HRESULT: 0x" << std::hex << hr << std::endl;
            return cv::Mat();
        }
    }

    // 从资源获取桌面纹理接口
    hr = pDesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&m_pDesktopTexture));
    pDesktopResource->Release(); // 释放资源接口
    if (FAILED(hr)) {
        std::cerr << "Failed to get desktop texture. HRESULT: 0x" << std::hex << hr << std::endl;
        m_pOutputDuplication->ReleaseFrame();
        return cv::Mat();
    }

    // 获取纹理描述
    D3D11_TEXTURE2D_DESC desc;
    m_pDesktopTexture->GetDesc(&desc);

    // 检查是否需要创建或调整暂存纹理
    if (!m_pStagingTexture || desc.Width != m_desktopWidth || desc.Height != m_desktopHeight) {
        if (m_pStagingTexture) {
            m_pStagingTexture->Release();
            m_pStagingTexture = nullptr;
        }

        // 设置暂存纹理描述(CPU可读)
        D3D11_TEXTURE2D_DESC stagingDesc;
        ZeroMemory(&stagingDesc, sizeof(stagingDesc));
        stagingDesc.Width = desc.Width;
        stagingDesc.Height = desc.Height;
        stagingDesc.MipLevels = 1;
        stagingDesc.ArraySize = 1;
        stagingDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // BGRA格式
        stagingDesc.SampleDesc.Count = 1;
        stagingDesc.SampleDesc.Quality = 0;
        stagingDesc.Usage = D3D11_USAGE_STAGING;       // CPU可读
        stagingDesc.BindFlags = 0;                     // 不绑定到管线
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; // CPU读权限
        stagingDesc.MiscFlags = 0;

        // 创建暂存纹理
        hr = m_pDevice->CreateTexture2D(&stagingDesc, nullptr, &m_pStagingTexture);
        if (FAILED(hr)) {
            std::cerr << "Failed to create staging texture. HRESULT: 0x" << std::hex << hr << std::endl;
            m_pDesktopTexture->Release();
            m_pOutputDuplication->ReleaseFrame();
            return cv::Mat();
        }
    }

    // 将桌面纹理复制到暂存纹理(CPU可访问)
    m_pDeviceContext->CopyResource(m_pStagingTexture, m_pDesktopTexture);
    m_pDesktopTexture->Release();

    // 映射暂存纹理到内存，以便CPU读取
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = m_pDeviceContext->Map(m_pStagingTexture, 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr)) {
        std::cerr << "Failed to map staging texture. HRESULT: 0x" << std::hex << hr << std::endl;
        m_pOutputDuplication->ReleaseFrame();
        return cv::Mat();
    }

    // 使用映射的数据创建完整的桌面图像(用于缓存)
    cv::Mat fullDesktopFrame(desc.Height, desc.Width, CV_8UC4, mappedResource.pData, mappedResource.RowPitch);
    cv::cvtColor(fullDesktopFrame, m_desktopMat, cv::COLOR_BGRA2BGR);

    // 解除纹理映射并释放帧
    m_pDeviceContext->Unmap(m_pStagingTexture, 0);
    m_pOutputDuplication->ReleaseFrame();

    return m_desktopMat.clone(); // 返回克隆的图像(确保数据独立)
}

// 获取指定窗口的截图
cv::Mat ScreenCapturer::getWindowMat(const std::string& titleSection) {
    // 先获取整个桌面的截图
    cv::Mat desktop = getDesktopMat();
    if (desktop.empty()) {
        return cv::Mat(); // 桌面截图失败返回空Mat
    }

    // 如果请求的是整个桌面
    if (titleSection == "screen") {
        m_currentWindowRect = { 0, 0, desktop.cols, desktop.rows };
        return desktop;
    }

    // 获取窗口矩形区域
    WindowRect rect = getWindowRect(titleSection);
    if (rect.width == 0 || rect.height == 0) {
        return cv::Mat(); // 无效窗口区域返回空Mat
    }

    // 确保窗口区域在桌面范围内
    rect.x = std::max(0, rect.x);
    rect.y = std::max(0, rect.y);
    rect.width = std::min(rect.width, desktop.cols - rect.x);
    rect.height = std::min(rect.height, desktop.rows - rect.y);

    // 检查调整后的区域是否有效
    if (rect.width <= 0 || rect.height <= 0) {
        return cv::Mat();
    }

    // 从桌面截图中提取窗口区域
    m_windowMat = desktop(cv::Rect(rect.x, rect.y, rect.width, rect.height)).clone();
    return m_windowMat;
}


cv::Mat ScreenCapturer::getDesktopAreaMat(const WindowRect& rect) {
    // 如果桌面复制接口未初始化，则初始化
    if (!m_pOutputDuplication) {
        if (!initialize()) {
            return cv::Mat(); // 初始化失败返回空Mat
        }
    }
    
    DXGI_OUTDUPL_FRAME_INFO frameInfo; // 帧信息结构
    IDXGIResource* pDesktopResource = nullptr; // 桌面资源

    // 获取下一帧(超时时间（毫秒），0 表示立即返回，INFINITE 表示无限等待。)
    HRESULT hr = m_pOutputDuplication->AcquireNextFrame(100, &frameInfo, &pDesktopResource);

    // 处理获取帧的结果
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            // 超时则返回空Mat（或从缓存中裁剪）
            return m_desktopMat.empty() ? cv::Mat() : m_desktopMat(cv::Rect(rect.x, rect.y, rect.width, rect.height)).clone();
        }
        else if (hr == DXGI_ERROR_ACCESS_LOST) {
            // 访问丢失(如显示器分辨率改变)，重新初始化
            releaseResources();
            if (!initialize()) {
                return cv::Mat();
            }
            return getDesktopAreaMat(rect); // 递归调用
        }
        else {
            // 其他错误
            std::cerr << "Failed to acquire next frame. HRESULT: 0x" << std::hex << hr << std::endl;
            return cv::Mat();
        }
    }

    // 从资源获取桌面纹理接口
    hr = pDesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&m_pDesktopTexture));
    pDesktopResource->Release(); // 释放资源接口
    if (FAILED(hr)) {
        std::cerr << "Failed to get desktop texture. HRESULT: 0x" << std::hex << hr << std::endl;
        m_pOutputDuplication->ReleaseFrame();
        return cv::Mat();
    }

    // 获取纹理描述
    D3D11_TEXTURE2D_DESC desc;
    m_pDesktopTexture->GetDesc(&desc);

    // 验证矩形区域是否在桌面范围内
    if (rect.x < 0 || rect.y < 0 || rect.width <= 0 || rect.height <= 0 ||
        rect.x + rect.width > static_cast<int>(desc.Width) ||
        rect.y + rect.height > static_cast<int>(desc.Height)) {
        std::cerr << "Invalid desktop area rect." << std::endl;
        m_pDesktopTexture->Release();
        m_pOutputDuplication->ReleaseFrame();
        return cv::Mat();
    }

    // 创建一个与指定区域大小相同的暂存纹理
    D3D11_TEXTURE2D_DESC stagingDesc;
    ZeroMemory(&stagingDesc, sizeof(stagingDesc));
    stagingDesc.Width = rect.width;
    stagingDesc.Height = rect.height;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // BGRA格式
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.SampleDesc.Quality = 0;
    stagingDesc.Usage = D3D11_USAGE_STAGING;       // CPU可读
    stagingDesc.BindFlags = 0;                     // 不绑定到管线
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; // CPU读权限
    stagingDesc.MiscFlags = 0;

    ID3D11Texture2D* pStagingTexture = nullptr;
    hr = m_pDevice->CreateTexture2D(&stagingDesc, nullptr, &pStagingTexture);
    if (FAILED(hr)) {
        std::cerr << "Failed to create staging texture. HRESULT: 0x" << std::hex << hr << std::endl;
        m_pDesktopTexture->Release();
        m_pOutputDuplication->ReleaseFrame();
        return cv::Mat();
    }

    // 只复制指定区域的纹理数据
    m_pDeviceContext->CopySubresourceRegion(
        pStagingTexture,   // 目标纹理
        0,                 // 目标子资源索引
        0, 0, 0,           // 目标左上角坐标
        m_pDesktopTexture, // 源纹理
        0,                 // 源子资源索引
        &CD3D11_BOX(rect.x, rect.y, 0, rect.x + rect.width, rect.y + rect.height, 1) // 源区域
    );

    m_pDesktopTexture->Release();

    // 映射暂存纹理到内存，以便CPU读取
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = m_pDeviceContext->Map(pStagingTexture, 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr)) {
        std::cerr << "Failed to map staging texture. HRESULT: 0x" << std::hex << hr << std::endl;
        pStagingTexture->Release();
        m_pOutputDuplication->ReleaseFrame();
        return cv::Mat();
    }

    // 使用映射的数据创建OpenCV矩阵(BGRA格式)
    cv::Mat frame(rect.height, rect.width, CV_8UC4, mappedResource.pData, mappedResource.RowPitch);
    // 转换为BGR格式(去掉alpha通道)
    cv::cvtColor(frame, m_areaMat, cv::COLOR_BGRA2BGR);
    // 解除纹理映射并释放资源
    m_pDeviceContext->Unmap(pStagingTexture, 0);
    pStagingTexture->Release();
    m_pOutputDuplication->ReleaseFrame();
    
    return m_areaMat;
}


// 获取窗口截图信息(包含图像数据和窗口位置)
MatInfo ScreenCapturer::getWindowMatInfo(std::string titleSection) {
    m_windowMat = getWindowMat(titleSection);
    
    MatInfo info = {0}; // 初始化为零
    
    if (m_windowMat.empty()) {
        std::cerr << "Failed to capture window: " << titleSection << std::endl;
        return info;
    }
    
    info.windowX = m_currentWindowRect.x;
    info.windowY = m_currentWindowRect.y;
    info.windowWidth = m_windowMat.cols;
    info.windowHeight = m_windowMat.rows;
    info.pMatUchar = static_cast<uchar*>(m_windowMat.data);
    return info;
}

// 线程安全的单例模式
static ScreenCapturer& getCapturer() {
    static ScreenCapturer instance;
    static std::once_flag initFlag;
    
    std::call_once(initFlag, []() {
        if (!instance.initialize()) {
            std::cerr << "Failed to initialize ScreenCapturer instance" << std::endl;
        }
    });
    return instance;
}

// 释放所有分配的资源
void ScreenCapturer::releaseResources() {
    if (m_pStagingTexture) {
        m_pStagingTexture->Release();
        m_pStagingTexture = nullptr;
    }

    if (m_pDesktopTexture) {
        m_pDesktopTexture->Release();
        m_pDesktopTexture = nullptr;
    }

    // 修复：正确释放桌面复制接口
    if (m_pOutputDuplication) {
        m_pOutputDuplication->Release();
        m_pOutputDuplication = nullptr;
    }

    if (m_pDeviceContext) {
        m_pDeviceContext->ClearState();
        m_pDeviceContext->Flush();
        m_pDeviceContext->Release();
        m_pDeviceContext = nullptr;
    }

    if (m_pDevice) {
        m_pDevice->Release();
        m_pDevice = nullptr;
    }

    m_desktopWidth = 0;
    m_desktopHeight = 0;
    m_zoomFactor = 1.0;
    m_windowDatas.clear();
    m_desktopMat.release(); // 释放OpenCV矩阵
    m_windowMat.release();  // 释放OpenCV矩阵
}



MatInfo getWindowMatInfo(const char* titleSection) {
    // 获取截屏器实例并执行截图 输入需要确保是GBK字符集编码
    std::string titleSectionGBK = utf8_to_gbk(titleSection);
    // 获取截屏器实例并执行截图
    ScreenCapturer& capturer = getCapturer();
    MatInfo matInfo = capturer.getWindowMatInfo(titleSectionGBK);
    return matInfo;
}

MatInfo getDesktopAreaMatInfo(const WindowRect& rect) {
    // 获取截屏器实例并执行截图
    ScreenCapturer& capturer = getCapturer();
    cv::Mat areaMat = capturer.getDesktopAreaMat(rect);
    MatInfo info = {};
    info.windowX = rect.x;
    info.windowY = rect.y;
    info.windowWidth = areaMat.cols;
    info.windowHeight = areaMat.rows;
    info.pMatUchar = static_cast<uchar*>(areaMat.data);
    return info;
}