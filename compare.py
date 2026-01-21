import cv2
import ctypes
import numpy as np
import mss
import time


# -*- coding: utf-8 -*-
import os
import time
from glob import glob
import win32gui
import pyautogui
import numpy as np
import mss
import cv2

import threading

class ScreenOperator():
    '''一些对整个屏幕的操作'''
    def __init__(self):
        super(ScreenOperator, self).__init__()
        self.templateDict = {}#key 模板标题 value {'template':mat,'mask':mat or None}
        # 使用线程本地存储来保存mss实例
        self.local = threading.local()
        
    def _get_mss_instance(self):
        '''获取当前线程的mss实例，如果不存在则创建'''
        if not hasattr(self.local, 'mssCamera'):
            self.local.mssCamera = mss.mss()
        return self.local.mssCamera

    def getWindowList(self):
        '''当前所有窗口的(句柄,标题名)列表'''
        titles = []
        windowList = []
        def get_all_hwnd(hwnd, mouse):
            '''获得当前活动标题'''
            if win32gui.IsWindow(hwnd) and win32gui.IsWindowEnabled(hwnd) and win32gui.IsWindowVisible(hwnd):
                title = win32gui.GetWindowText(hwnd)
                if title not in titles:
                    titles.append(title)
                    windowList.append((hwnd,title))
        win32gui.EnumWindows(get_all_hwnd, 0)
        return windowList

    def findWindow(self,content):
        '''根据部分文本简单获得相似度最高的窗口句柄和标题'''
        windowList = self.getWindowList()
        simTitle = ""
        simHWND = None
        minDistance = 1000
        for hwnd,title in windowList:
            if content in title:
                distance = abs(len(title)-len(content))
                if distance<minDistance:
                    minDistance = distance
                    simTitle = title
                    simHWND = hwnd
        return simHWND,simTitle

    def getWindowLoc(self,content="screen",front=True):
        '''根据标题所含内容定位窗口 返回 xmin, ymin, xmax, ymax'''
        if content == "screen":
            return (0, 0, desktopW, desktopH)
        hwnd,title = self.findWindow(content)
        # 最小化的窗口变大
        if win32gui.IsIconic(hwnd):
            win32gui.ShowWindow(hwnd, True)
        # 无法前置 则用tab置换激活
        if front:
            try:
                win32gui.SetForegroundWindow(hwnd)
            except:
                if hwnd is None:
                    raise Exception(f"未找到窗口{content}")
                pyautogui.keyDown('alt')
                pyautogui.press('tab')
                pyautogui.keyUp('alt')
                return False
        window = pyautogui.getWindowsWithTitle(title)[0]
        return window.left,window.top,window.right,window.bottom

    def areaShot(self,rect):
        '''选区截图，返回BGR图片numpy数组'''
        if type(rect) is dict:
            xmin, ymin, xmax, ymax = rect['xmin'], rect['ymin'], rect['xmax'], rect['ymax']
        else:
            xmin, ymin, xmax, ymax = rect
        # 获取当前线程的mss实例
        mssCamera = self._get_mss_instance()
        img = mssCamera.grab({"left": xmin, "top": ymin, "width": xmax - xmin, "height": ymax - ymin})
        img = np.array(img)[:,:,:3]
        return img

    def getWindowMat(self,content="screen"):
        '''根据窗口标题包含文本返回窗口矩阵'''
        #全屏
        if content == "screen":
            img = self.areaShot([0,0,desktopW,desktopH])
        #窗口
        else:
            rect = self.getWindowLoc(content)
            img = self.areaShot(rect)
        return img

    def addTemplate(self,title,templatePath):
        '''
        添加模板矩阵到模板字典，如果模板有4通道，默认使用第4通道作为掩膜
        :param title: 模板标题
        :param templatePath: 模板路径，默认彩图
        '''
        template = cv2.imdecode(np.fromfile(templatePath, dtype=np.uint8), -1)#可能是灰度图
        if len(template.shape)<3:
            template = np.stack([template]*3,axis=-1)
        mask = None
        if template.shape[-1]==4:
            mask = template[:, :, 3]
            mask[np.where(mask != 0)] = 1
            if mask.sum()!=(mask.shape[0]*mask.shape[1]):
                #print("识别到带掩膜的模板 %s"%title)
                pass
            else:
                mask = None
        template = template[:, :, :3] if len(template.shape) == 3 else template#彩图就只要前3通道
        self.templateDict[title] = {'template':template,'mask':mask}
        #print("加入模板:",title)

    def addFolderTemplates(self,folderPath):
        '''
        加入一个文件夹的模板 模板名就是图片名 如果模板有4通道，默认使用第4通道作为掩膜
        :param folderPath: 文件夹路径
        '''
        paths = glob(folderPath.strip("/").strip("\\")+"/*.png")
        titles = []
        for path in paths:
            title = os.path.split(path)[-1].split(".")[0]
            self.addTemplate(title,path)
            titles.append(title)
        return titles

    def matchInImg(self,templateTitle,img):
        '''相似度计算 从图片中匹配模板 这里会默认模板 原图没有缩放，返回的坐标是原图大小的坐标'''
        template = self.templateDict[templateTitle]["template"]
        mask = self.templateDict[templateTitle]["mask"]
        if mask is None:
            res = cv2.matchTemplate(img, template, cv2.TM_CCOEFF_NORMED)
        else:
            res = cv2.matchTemplate(img, template, cv2.TM_CCOEFF_NORMED, mask=mask)
        res[np.where(np.abs(res) == np.inf)] = 0
        minVal, maxVal, minLoc, maxLoc = cv2.minMaxLoc(res)#取最小值的位置即差异化最小
        xmin,ymin = maxLoc
        sim = maxVal
        xmax = xmin + template.shape[1]
        ymax = ymin + template.shape[0]
        return xmin,ymin,xmax,ymax,sim

    def matchTargetsInImg(self,templateTitle,img,simThreshold=0.9):
        '''相似度计算 从图片中匹配多个模板 相似度降序排序 返回坐标框列表'''
        template = self.templateDict[templateTitle]["template"]
        mask = self.templateDict[templateTitle]["mask"]
        if mask is None:
            scores = cv2.matchTemplate(img, template, cv2.TM_CCOEFF_NORMED)
        else:
            scores = cv2.matchTemplate(img, template, cv2.TM_CCOEFF_NORMED, mask=mask)
        scores[np.where(scores == np.inf)] = 0
        #过滤符合差异化阈值的位置
        locs = np.where(scores>=simThreshold)
        scores = [scores[locs[0][i], locs[1][i]] for i in range(len(locs[0]))]
        #非极大抑制法 目的就是过滤过多的重合图片块
        bboxs = [[locs[1][i],locs[0][i],locs[1][i]+template.shape[1],locs[0][i]+template.shape[0]] for i in range(len(locs[0]))]
        bboxs,scores = nms(bboxs,scores)
        rects = []
        for i,bbox in enumerate(bboxs):
            xmin,ymin,xmax,ymax = int(bbox[0]),int(bbox[1]),int(bbox[2]),int(bbox[3])
            rects.append([xmin,ymin,xmax,ymax,scores[i]])
        rects = sorted(rects, key=lambda x: x[-1],reverse=True)#相似度降序
        rects = [{"xmin":rect[0],"ymin":rect[1],"xmax":rect[2],"ymax":rect[3],"sim":rect[-1]} for rect in rects]
        return rects

    def matchTemplate(self,templateTitle,windowTitle="screen",img:np.ndarray=None):
        '''窗口中的模板匹配 '''
        if img is None:
            #窗口在整个桌面的位置 窗口也可能就是整个桌面
            windowRect = self.getWindowLoc(windowTitle)
            img = self.getWindowMat(windowTitle)
        else:
            windowRect = (0,0,img.shape[1],img.shape[0])
        #这里返回的是 模板匹配结果在窗口的坐标 差异值
        xminInWindow, yminInWindow, xmaxInWindow, ymaxInWindow, sim = self.matchInImg(templateTitle,img)
        centerInWindow = ((xminInWindow+xmaxInWindow)//2,(yminInWindow+ymaxInWindow)//2,sim)
        rectInWindow = (xminInWindow, yminInWindow, xmaxInWindow, ymaxInWindow,sim)
        #在桌面的坐标值
        xminInDesktop, yminInDesktop, xmaxInDesktop, ymaxInDesktop = xminInWindow+windowRect[0], yminInWindow+windowRect[1], xmaxInWindow+windowRect[0], ymaxInWindow+windowRect[1]
        centerInDesktop = ((xminInDesktop+xmaxInDesktop)//2,(yminInDesktop+ymaxInDesktop)//2,sim)
        rectInDesktop = (xminInDesktop, yminInDesktop, xmaxInDesktop, ymaxInDesktop,sim)
        #整理好结果...
        result = {"rectInWindow":{"xmin":rectInWindow[0],"ymin":rectInWindow[1],"xmax":rectInWindow[2],"ymax":rectInWindow[3],"sim":sim},
                  "centerInWindow":{"x":centerInWindow[0],"y":centerInWindow[1],"sim":sim},
                  "rectInDesktop":{"xmin":rectInDesktop[0],"ymin":rectInDesktop[1],"xmax":rectInDesktop[2],"ymax":rectInDesktop[3],"sim":sim},
                  "centerInDesktop":{"x":centerInDesktop[0],"y":centerInDesktop[1],"sim":sim},
                }
        return result

    def matchTargets(self,templateTitle,windowTitle="screen",simThreshold=0.9,img:np.ndarray=None):
        '''窗口中的模板多目标匹配'''
        if img is None:
            #窗口在整个桌面的位置 窗口也可能就是整个桌面
            windowRect = self.getWindowLoc(windowTitle)
            img = self.getWindowMat(windowTitle)
        else:
            windowRect = (0,0,img.shape[1],img.shape[0])
        #窗口中的坐标矩阵 包含了匹配差异值
        rectsInWindow = self.matchTargetsInImg(templateTitle,img,simThreshold=simThreshold)
        centersInWindow = [{"x":(rect["xmin"]+rect["xmax"])//2,"y":(rect["ymin"]+rect["ymax"])//2,"sim":rect["sim"]} for rect in rectsInWindow]
        #桌面坐标
        rectsInDesktop = [{"xmin":rect["xmin"]+windowRect[0],"ymin":rect["ymin"]+windowRect[1],"xmax":rect["xmax"]+windowRect[0],"ymax":rect["ymax"]+windowRect[1],"sim":rect["sim"]} for rect in rectsInWindow]
        centersInDesktop = [{"x":(rect["xmin"]+rect["xmax"])//2,"y":(rect["ymin"]+rect["ymax"])//2,"sim":rect["sim"]} for rect in rectsInDesktop]
        result = {"rectsInWindow":rectsInWindow,"centersInWindow":centersInWindow,"rectsInDesktop":rectsInDesktop,"centersInDesktop":centersInDesktop,"templateTitle":templateTitle}
        return result

    def matchTemplates(self,templateTitles,windowTitle="screen",simThreshold=0.9,img:np.ndarray=None):
        '''窗口中的模板列表匹配，返回第一个在列表能匹配成功的坐标'''
        for templateTitle in templateTitles:
            result = self.matchTemplate(templateTitle,windowTitle,img=img)
            sim = result["rectInWindow"]["sim"]
            if sim >= simThreshold:
                result["success"] = True
                result["templateTitle"] = templateTitle
                return result
        result["success"] = False
        result["templateTitle"] = templateTitle        
        return result

    def waitTemplates(self,templateTitles,windowTitle="screen",timeout=3,simThreshold=0.9):
        '''等待模板列表中某一个的出现'''
        if type(templateTitles) is not list:
            templateTitles = [templateTitles]
        start = time.time()
        while (time.time() - start)<timeout:
            result = self.matchTemplates(templateTitles,windowTitle,simThreshold=simThreshold)
            if result["success"]:
                return result
            time.sleep(0.1)
        return result

    def windowPosToDesktop(self,windowTitle,pos):
        '''窗口坐标转桌面坐标'''
        windowX,windowY = pos
        xmin, ymin, xmax, ymax = self.getWindowLoc(windowTitle)
        desktopX = windowX+xmin
        desktopY = windowY+ymin
        return desktopX,desktopY

    def desktopPosToWindowPos(self,windowTitle,desktopPos):
        '''桌面坐标转窗口坐标'''
        desktopX,desktopY = desktopPos
        xmin, ymin, xmax, ymax = self.getWindowLoc(windowTitle)
        windowX = desktopX - xmin
        windowY = desktopY - ymin
        return windowX,windowY

    def getWindowArea(self,windowTitle,windowAreaRect):
        '''
        获得窗口部分区域截图
        :param windowTitle: 窗口标题
        :param windowAreaRect: 窗口（非桌面）xmin ymin xmax ymax
        '''
        if type(windowAreaRect) is dict:
            windowAreaRect = (windowAreaRect["xmin"],windowAreaRect["ymin"],windowAreaRect["xmax"],windowAreaRect["ymax"])
        #窗口坐标转桌面
        xmin,ymin = self.windowPosToDesktop(windowTitle,(windowAreaRect[0],windowAreaRect[1]))
        xmax, ymax = self.windowPosToDesktop(windowTitle, (windowAreaRect[2], windowAreaRect[3]))
        img = self.areaShot(rect=(xmin,ymin,xmax, ymax))
        return img


snapDetect = ctypes.WinDLL('E:/desktop/test/SnapDetectLib.dll')

templateDir = 'E:/desktop/SnapDetect-main/鼠标'
templateName = "鼠标"
#转换为ctypes.POINTER(ctypes.c_char_p)
templateDir = ctypes.c_char_p(templateDir.encode('utf-8'))
templateName = ctypes.c_char_p(templateName.encode('utf-8'))
snapDetect.addTemplateByDir(templateDir,templateName)
snapDetect.listTemplates()

class MatchResult(ctypes.Structure):
    _fields_ = [("xmin",ctypes.c_int),("ymin",ctypes.c_int),("xmax",ctypes.c_int),("ymax",ctypes.c_int),
                ("centerX",ctypes.c_int),("centerY",ctypes.c_int),("score",ctypes.c_double)]
snapDetect.matchInWindow.argtypes = [ctypes.c_char_p,ctypes.c_char_p]
snapDetect.matchInWindow.restype = MatchResult

matchResult = snapDetect.matchInWindow(ctypes.c_char_p("鼠标".encode('utf-8')),ctypes.c_char_p("测试".encode('utf-8')))
t1 = time.time()
for i in range(10):
    matchResult = snapDetect.matchInWindow(ctypes.c_char_p("鼠标".encode('utf-8')),ctypes.c_char_p("测试".encode('utf-8')))
t2 = time.time()
print((t2-t1)/10*1000)

screenOperator = ScreenOperator()
screenOperator.addFolderTemplates("E:/desktop/SnapDetect-main/鼠标")
r = screenOperator.matchTemplate(templateTitle="鼠标1号",windowTitle="测试")
t1 = time.time()
for i in range(10):
    r = screenOperator.matchTemplate(templateTitle="鼠标1号",windowTitle="测试")
t2 = time.time()
print((t2-t1)/10*1000)
print(r)