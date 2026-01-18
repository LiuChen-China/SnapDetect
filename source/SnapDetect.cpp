#include "SnapDetect.h"


//在指定区域内匹配模板
MatchResult matchInDeskArea(const char* templateName,WindowRect areaRect)
{
    //获取桌面区域矩阵
    MatInfo areaMatInfo = getDesktopAreaMatInfo(areaRect);
    cv::Mat areaMat(areaMatInfo.windowHeight, areaMatInfo.windowWidth, CV_8UC3, areaMatInfo.pMatUchar);
    //在区域内匹配模板
    MatchResult matchResult = matchTemplate(areaMat,templateName);
    return matchResult;
}

//在指定窗口内匹配模板
MatchResult matchInWindow(const char* templateName,const char* windowName)
{
    //获取窗口矩阵
    MatInfo windowMatInfo = getWindowMatInfo(windowName);
    cv::Mat windowMat(windowMatInfo.windowHeight, windowMatInfo.windowWidth, CV_8UC3, windowMatInfo.pMatUchar);
    //在窗口内匹配模板
    MatchResult matchResult = matchTemplate(windowMat,templateName);
    return matchResult;
}