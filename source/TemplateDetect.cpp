#include "TemplateDetect.h"
#include "WindowCapture.h"
#include "Utils.h"
#include <cstring> // 记得添加这个头文件来使用strcmp
#include <filesystem> 



/**
 * 列出文件夹下所有文件（不包含子文件夹，仅文件）
 * @param folderPath 文件夹路径
 * @param fileList 输出参数，存储文件的完整路径
 * @return 成功返回true，失败（如路径不存在）返回false
 */
namespace fs = std::filesystem;
bool listFilesInFolder(const std::string& folderPath, std::vector<std::string>& fileList) {
    try {
        // 检查路径是否存在且为文件夹
        if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
            return false;
        }

        // 遍历文件夹
        for (const auto& entry : fs::directory_iterator(folderPath)) {
            // 只保留文件（跳过子文件夹）
            if (entry.is_regular_file()) {
                // 获取文件的完整路径并添加到列表
                // 将GBK编码转utf-8
                std::string filePathGBK = entry.path().string();
                std::string filePathUTF8 = gbk_to_utf8(filePathGBK.c_str());
                // 替换路径分隔符为Unix风格的斜杠
                std::replace(filePathUTF8.begin(), filePathUTF8.end(), '\\', '/');
                fileList.push_back(filePathUTF8);
            }
        }
        return true;
    } catch (const fs::filesystem_error& e) {
        // 捕获文件系统错误（如权限问题）
        return false;
    }
}


static TemplateDict templateDict; // 模板字典，用于存储所有模板

void addTemplate(const char* imgPath,const char* templateName){
    std::string imgPathGBK = utf8_to_gbk(imgPath);
    cv::String imgPathStr(imgPathGBK.c_str());
    // 使用IMREAD_UNCHANGED模式，保留原始通道数    
    cv::Mat templateMat = cv::imread(imgPathStr, cv::IMREAD_UNCHANGED);
    //如果没传入模板名，默认使用文件名作为模板名
    std::string templateNameStr;
    if(templateName == nullptr)
    {
        templateNameStr = getFileName(imgPath);
    }else{
        //const char* 转 std::string
        templateNameStr = templateName;//隐式转换
    }
    // 如果字典不存在该模板名，添加到字典 空列表
    if(templateDict.find(templateNameStr) == templateDict.end())
    {
        templateDict[templateNameStr] =  std::vector<TemplateAndMask>();
    }
    cv::Mat maskMat;
    // 如果是4通道图片，将alpha通道作为掩膜
    if (templateMat.channels()==4){
        // 提取第4个通道（索引从0开始，所以3对应alpha通道）
        cv::extractChannel(templateMat, maskMat, 3);
        cv::threshold(maskMat, maskMat, 0, 1, cv::THRESH_BINARY);
        // 计算总像素数：高度（rows）× 宽度（cols）
        int total_pixels = maskMat.rows * maskMat.cols;

        // 计算掩膜所有像素的总和
        double sum = cv::sum(maskMat)[0]; // sum返回的是Scalar，取第一个元素（单通道）

        // 比较总和是否不等于总像素数 说明这是一个有alpha通道的图片，需要将alpha通道作为掩膜
        if (sum == total_pixels) {
            // 如果相等，则说明只是有第四个通道，没有透明部分，并不适合做掩膜 将掩膜置空
            maskMat.release(); // 释放内存，矩阵变为空
        }
    }
    //保留模板前3通道
    cv::cvtColor(templateMat, templateMat, cv::COLOR_BGRA2BGR);
    //掩膜*255
    maskMat = maskMat*255;
    TemplateAndMask templateAndMask{templateMat, maskMat};
    templateDict[templateNameStr].push_back(templateAndMask);
    if(!maskMat.empty()){
        printf("SnapDetect添加模板: %s, 路径: %s, 掩膜: %s\n", templateNameStr.c_str(), imgPath, "有");
    }else{
        printf("SnapDetect添加模板: %s, 路径: %s, 掩膜: %s\n", templateNameStr.c_str(), imgPath, "无");
    }
}

void addTemplateByDir(const char* dirPath,const char* templateName){
    //如果没传入模板名，默认使用文件夹名作为模板名
    std::string templateNameStr;
    if(templateName == nullptr)
    {
        templateNameStr = getFileName(dirPath);
    }else{
        //const char* 转 std::string
        templateNameStr = templateName;//隐式转换
    }
    // 列出目录下所有文件
    std::vector<std::string> fileList;
    std::string dirPathGBK = utf8_to_gbk(dirPath);
    listFilesInFolder(dirPathGBK, fileList);
    //将列表按路径升序排序 这样1号模板会被优先检测到
    std::sort(fileList.begin(), fileList.end());
    // 遍历所有文件，添加到模板字典
    for (const auto& filePath : fileList) {
        addTemplate(filePath.c_str(), templateNameStr.c_str());
    }
}

void listTemplates(){
    for (const auto& pair : templateDict) {
        //遍历模板列表
        int templateCount = 0;
        int maskCount = 0;
        for (const auto& templateAndMask : pair.second) {
            templateCount++;
            if(!templateAndMask.maskMat.empty()){
                maskCount++;
            }
        }
        printf("模板名: %s, 模板数量: %d, 掩膜数量: %d\n", pair.first.c_str(), templateCount, maskCount);
    }
}

MatchResult matchTemplate(const cv::Mat& img,const std::string& templateName,float threshold){  
    //取出模板
    auto it = templateDict.find(templateName);
    if(it == templateDict.end()){
        printf("模板名: %s 不存在\n", templateName.c_str());
        return MatchResult();
    }
    const std::vector<TemplateAndMask>& templateAndMaskList = it->second;
    MatchResult result;
    result.score = 0;
    //逐个模板遍历匹配
    cv::Mat resultMat;
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    double bigVal = 1e20;
    for (const auto& templateAndMask : templateAndMaskList) {
        if(templateAndMask.maskMat.empty()){
            //如果掩膜为空 说明这是一个普通的模板 直接匹配
            cv::matchTemplate(img, templateAndMask.templateMat, resultMat, cv::TM_CCOEFF_NORMED);
        }else{
            //如果掩膜不为空 说明这是一个有alpha通道的模板 需要使用带掩膜的匹配方法
            cv::matchTemplate(img, templateAndMask.templateMat, resultMat, cv::TM_CCOEFF_NORMED, templateAndMask.maskMat);
        }
        //将 resultMat里的inf值转0
        // 使用THRESH_TOZERO_INV模式：大于thresholdVal的元素设为0，其他保留
        cv::threshold(resultMat, resultMat, bigVal, 0, cv::THRESH_TOZERO_INV);
        cv::minMaxLoc(resultMat, &minVal, &maxVal, &minLoc, &maxLoc);
        if (maxVal > result.score){
            result.score = maxVal;
            result.xmin = maxLoc.x;
            result.ymin = maxLoc.y;
            result.xmax = maxLoc.x + templateAndMask.templateMat.cols;
            result.ymax = maxLoc.y + templateAndMask.templateMat.rows;
            result.centerX = maxLoc.x + templateAndMask.templateMat.cols / 2;
            result.centerY = maxLoc.y + templateAndMask.templateMat.rows / 2;
        }
        //如果满足阈值，则直接返回
        if (maxVal >= threshold){
            return result;
        }
    }
    return result;
}