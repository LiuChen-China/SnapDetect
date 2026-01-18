# include <iostream>
# include <opencv2/opencv.hpp>
# include "WindowCapture.h"
# include "TemplateDetect.h"
# include "SnapDetect.h"
# include "Utils.h"
#include <fstream>
#include <windows.h> // 添加Windows API支持

void main()
{
    // ///////////////////////////////测试固定区域截图
    // WindowRect areaRect = {566, 100, 640, 540};
    // //算下截取100次的时间
    // MatInfo areaMatInfo = getDesktopAreaMatInfo(areaRect);
    // clock_t start = clock();
    // for (int i = 0; i < 100; i++)
    // {
    //     areaMatInfo = getDesktopAreaMatInfo(areaRect);
    //     cv::Mat mat(areaMatInfo.windowHeight, areaMatInfo.windowWidth, CV_8UC3, areaMatInfo.pMatUchar);
    // }
    // clock_t end = clock();
    // printf("area snap 100 times cost: %fms\n", (double)(end - start) / CLOCKS_PER_SEC * 1000);
    
    // ////////////////////////////////测试窗口截图
    // MatInfo matInfo = getWindowMatInfo("测试");
    // cv::Mat mat(matInfo.windowHeight, matInfo.windowWidth, CV_8UC3, matInfo.pMatUchar);
    // cv::imshow("mat", mat);
    // cv::waitKey(0);
    // //算一下截图100次的时间
    // clock_t start = clock();
    // for (int i = 0; i < 100; i++)
    // {
    //     matInfo = getWindowMatInfo("测试");
    //     mat = cv::Mat(matInfo.windowHeight, matInfo.windowWidth, CV_8UC3, matInfo.pMatUchar);
    // }
    // clock_t end = clock();
    // printf("snap 100 times cost: %fms\n", (double)(end - start) / CLOCKS_PER_SEC * 1000);
    // cv::imshow("mat", mat);
    // cv::waitKey(0);


    // ///////////////////////////测试窗口模板匹配
    const char* templateDir = "G:/Desktop/SnapDetect/鼠标";
    const char* imgPath = "G:/Desktop/SnapDetect/测试.png";
    addTemplateByDir(templateDir);
    MatchResult matchResult = matchInWindow("鼠标","测试");
    printf("matchResult.score: %f\n", matchResult.score);
    printf("matchResult.rect: %d, %d, %d, %d\n", matchResult.xmin, matchResult.ymin, matchResult.xmax, matchResult.ymax);

    // //////////////////////////测试固定区域模板匹配
    // const char* templateDir = "E:/desktop/SnapDetect/鼠标";
    // const char* imgPath = "E:/desktop/SnapDetect/测试.png";
    // addTemplateByDir(templateDir);
    // WindowRect areaRect = {522, 250, 640, 540};
    // //匹配10次 计算时间
    // clock_t start = clock();
    // MatchResult matchResult;
    // for (int i = 0; i < 10; i++)
    // {
    //     matchResult = matchInDeskArea("鼠标",areaRect);
    // }
    // clock_t end = clock();
    // printf("match 10 times cost: %dms\n", (int)(end - start) / CLOCKS_PER_SEC * 1000/10);
    // matchResult = matchInDeskArea("鼠标",areaRect);
    // printf("matchResult.score: %f\n", matchResult.score);
    // printf("matchResult.rect: %d, %d, %d, %d\n", matchResult.xmin, matchResult.ymin, matchResult.xmax, matchResult.ymax);
}