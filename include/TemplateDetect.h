#pragma once
#include <opencv2/opencv.hpp>
#include <unordered_map>
#include <vector>

struct TemplateAndMask
{
	cv::Mat templateMat;//模板矩阵
	cv::Mat maskMat;//掩膜矩阵
};

struct MatchResult
{
	int xmin;//匹配到的模板的左上角x坐标
	int ymin;//匹配到的模板的左上角y坐标
	int xmax;//匹配到的模板的右下角x坐标
	int ymax;//匹配到的模板的右下角y坐标
	int centerX;//匹配到的模板的中心x坐标
	int centerY;//匹配到的模板的中心y坐标
	double score;//匹配度
};

//模板名:模板矩阵列表的字典别名 也可以用typedef代替using
using TemplateDict = std::unordered_map<std::string, std::vector<TemplateAndMask>>;



//添加单张模板，如果templateName为空，默认使用文件名作为模板名
void addTemplate(const char* imgPath,const char* templateName=nullptr);

//添加目录下的所有图片作为一个模板，如果templateName为空，默认使用目录名作为模板名
void addTemplateByDir(const char* dirPath,const char* templateName=nullptr);

//列出所有模板
void listTemplates();

//在img中匹配templateName对应的模板，返回匹配结果
//当匹配度大于threshold时 立马返回匹配结果 否则就遍历模板列表 返回匹配度最高的结果
//输入图片必须是3通道的
MatchResult matchTemplate(const cv::Mat& img,const std::string& templateName,float threshold=0.8);