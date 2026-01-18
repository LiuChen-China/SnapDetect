#pragma once
#include "TemplateDetect.h"
#include "WindowCapture.h"
#include "Utils.h"

//在指定区域内匹配模板
MatchResult matchInDeskArea(const char* templateName,WindowRect areaRect);

//在指定窗口内匹配模板
MatchResult matchInWindow(const char* templateName,const char* windowName);
