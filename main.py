import cv2
import ctypes
import numpy as np
import mss
import time
from schemas.VirtualDeviceSchema import *

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

    def getWindowLoc(self,content="screen",front=True)->Rect:
        '''根据标题所含内容定位窗口 返回 xmin, ymin, xmax, ymax'''
        if content == "screen":
            return Rect(xmin=0,ymin=0,xmax=pyautogui.size()[0],ymax=pyautogui.size()[1])
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
                return Rect(xmin=0,ymin=0,xmax=0,ymax=0)
        window = pyautogui.getWindowsWithTitle(title)[0]
        return Rect(xmin=window.left,ymin=window.top,xmax=window.right,ymax=window.bottom)

    def areaShot(self,rect:Rect):
        '''选区截图，返回BGR图片numpy数组'''
        # 获取当前线程的mss实例
        mssCamera = self._get_mss_instance()
        img = mssCamera.grab({"left": rect.xmin, "top": rect.ymin, "width": rect.xmax - rect.xmin, "height": rect.ymax - rect.ymin})
        img = np.array(img)[:,:,:3]
        return img

    def getWindowMat(self,content="screen"):
        '''根据窗口标题包含文本返回窗口矩阵'''
        #全屏
        if content == "screen":
            img = self.areaShot(Rect(xmin=0,ymin=0,xmax=pyautogui.size()[0],ymax=pyautogui.size()[1]))
        #窗口
        else:
            rect = self.getWindowLoc(content)
            img = self.areaShot(rect)
        return img

    def addTemplate(self,name=None,path=''):
        '''
        添加模板
        :param name: 模板名称
        :param path: 模板路径，默认彩图
        '''
        name = name if name is not None else os.path.split(path)[-1].split(".")[0]
        template = cv2.imdecode(np.fromfile(path, dtype=np.uint8), -1)#可能是灰度图
        if len(template.shape)<3:
            template = np.stack([template]*3,axis=-1)
        mask = None
        if template.shape[-1]==4:
            mask = template[:, :, 3]
            mask[np.where(mask != 0)] = 1
            # 全部为0 则无掩膜
            if mask.sum()==(mask.shape[0]*mask.shape[1]):
                mask = None
        template = template[:, :, :3] if len(template.shape) == 3 else template#彩图就只要前3通道
        if name in self.templateDict:
            print(f'已经存在模板{name}，路径为{self.templateDict[name]["path"]}，新路径为{path}')
        self.templateDict[name] = {'template':template,'mask':mask,'path':path}

    def addFolderTemplates(self,folderPath):
        '''
        加入一个文件夹的模板 模板名就是图片名
        注意这里每张图片是独立的模板
        :param name: 模板名称
        :param folderPath: 文件夹路径
        '''
        paths = glob(folderPath.strip("/").strip("\\")+"/*.png")
        for path in paths:
            name = os.path.split(path)[-1].split(".")[0]
            self.addTemplate(name=name,path=path)

    def matchOneInImgByOneTemplate(self,name,img)->MatchOneRes:
        '''
        使用一个模板 匹配出图片中最相似的一个位置
        默认使用模板组的第一个模板
        name: 模板名称
        img: 图片矩阵 3通道BGR
        '''
        if self.templateDict[name]["mask"] is None:
            res = cv2.matchTemplate(img, self.templateDict[name]["template"], cv2.TM_CCOEFF_NORMED)
        else:
            res = cv2.matchTemplate(img, self.templateDict[name]["template"], cv2.TM_CCOEFF_NORMED, mask=self.templateDict[name]["mask"])
        res[np.where(np.abs(res) == np.inf)] = 0
        minVal, maxVal, minLoc, maxLoc = cv2.minMaxLoc(res)#取最小值的位置即差异化最小
        xmin,ymin = maxLoc
        xmax = xmin + self.templateDict[name][0]["template"].shape[1]
        ymax = ymin + self.templateDict[name][0]["template"].shape[0]
        rect = Rect(xmin=xmin,ymin=ymin,xmax=xmax,ymax=ymax)
        center = Center(x=xmin+self.templateDict[name]["template"].shape[1]//2,y=ymin+self.templateDict[name]["template"].shape[0]//2)
        return MatchOneRes(rect=rect,center=center,score=maxVal)

    def matchAllInImgByOneTemplate(self,name,img,threshold=0.9)->List[MatchOneRes]:
        '''
        使用一个模板 匹配出图片中所有符合相似度阈值的位置
        name: 模板名称
        img: 图片矩阵 3通道BGR
        threshold: 相似度阈值 默认0.9
        '''
        template = self.templateDict[name]["template"]
        mask = self.templateDict[name]["mask"]
        if self.templateDict[name]["mask"] is None:
            scores = cv2.matchTemplate(img, template, cv2.TM_CCOEFF_NORMED)
        else:
            scores = cv2.matchTemplate(img, template, cv2.TM_CCOEFF_NORMED, mask=mask)
        scores[np.where(scores == np.inf)] = 0
        #过滤符合差异化阈值的位置
        locs = np.where(scores>=threshold)
        scores = [scores[locs[0][i], locs[1][i]] for i in range(len(locs[0]))]
        #非极大抑制法 目的就是过滤过多的重合图片块
        bboxs = [[locs[1][i],locs[0][i],locs[1][i]+template.shape[1],locs[0][i]+template.shape[0]] for i in range(len(locs[0]))]
        bboxs,scores = nms(bboxs,scores)
        rects = []
        for i,bbox in enumerate(bboxs):
            xmin,ymin,xmax,ymax = int(bbox[0]),int(bbox[1]),int(bbox[2]),int(bbox[3])
            rects.append([xmin,ymin,xmax,ymax,scores[i]])
        rects = sorted(rects, key=lambda x: x[-1],reverse=True)#相似度降序
        results = []
        for rect in rects:
            r = Rect(xmin=rect[0],ymin=rect[1],xmax=rect[2],ymax=rect[3])
            c = Center(x=(rect[0]+rect[2])//2,y=(rect[1]+rect[3])//2)
            results.append(MatchOneRes(rect=r,center=c,score=rect[-1]))
        return results

    def matchOneInWindowByOneTemplate(self,name,windowTitle="screen",img:np.ndarray=np.zeros(0))->MatchOneInWindowByOneTemplateRes:
        '''
        窗口中的模板匹配 
        name: 模板名称
        windowTitle: 窗口标题 默认整个桌面
        img: 图片矩阵 3通道BGR 默认空 则从窗口中获取 用来测试用的
        '''
        #窗口在整个桌面的位置 窗口也可能就是整个桌面
        windowRect = self.getWindowLoc(windowTitle) if img.size==0 else Rect(xmin=0,ymin=0,xmax=img.shape[1],ymax=img.shape[0])
        img = self.getWindowMat(windowTitle) if img.size==0 else img
        #这里返回的是 模板匹配结果在窗口的坐标 差异值
        result = self.matchOneInImgByOneTemplate(name,img)
        rectInWindow = result.rect
        centerInWindow = Center(x=(rectInWindow.xmin+rectInWindow.xmax)//2,y=(rectInWindow.ymin+rectInWindow.ymax)//2)
        #在桌面的坐标值
        rectInDesktop = Rect(xmin=rectInWindow.xmin+windowRect.xmin,ymin=rectInWindow.ymin+windowRect.ymin,xmax=rectInWindow.xmax+windowRect.xmin,ymax=rectInWindow.ymax+windowRect.ymin)
        centerInDesktop = Center(x=(rectInDesktop.xmin+rectInDesktop.xmax)//2,y=(rectInDesktop.ymin+rectInDesktop.ymax)//2)
        return MatchOneInWindowByOneTemplateRes(rectInWindow=rectInWindow,centerInWindow=centerInWindow,rectInDesktop=rectInDesktop,centerInDesktop=centerInDesktop,score=result.score)

    def matchAllInWindowByOneTemplate(self,name,windowTitle="screen",threshold=0.9,img:np.ndarray=np.zeros(0))->MatchAllInWindow:
        '''通过一个模板 匹配出窗口中所有符合相似度阈值的位置'''
        #窗口在整个桌面的位置 窗口也可能就是整个桌面
        windowRect = self.getWindowLoc(windowTitle) if img.size==0 else Rect(xmin=0,ymin=0,xmax=img.shape[1],ymax=img.shape[0])
        img = self.getWindowMat(windowTitle) if img.size==0 else img
        results = self.matchAllInImgByOneTemplate(name,img,threshold=threshold)
        matchsInWindow = []
        matchsInDesktop = []
        for result in results:
            rectInWindow = result.rect
            centerInWindow = Center(x=(rectInWindow.xmin+rectInWindow.xmax)//2,y=(rectInWindow.ymin+rectInWindow.ymax)//2)
            rectInDesktop = Rect(xmin=rectInWindow.xmin+windowRect.xmin,ymin=rectInWindow.ymin+windowRect.ymin,xmax=rectInWindow.xmax+windowRect.xmin,ymax=rectInWindow.ymax+windowRect.ymin)
            centerInDesktop = Center(x=(rectInDesktop.xmin+rectInDesktop.xmax)//2,y=(rectInDesktop.ymin+rectInDesktop.ymax)//2)
            matchsInWindow.append(MatchOneRes(rect=rectInWindow,center=centerInWindow,score=result.score))
            matchsInDesktop.append(MatchOneRes(rect=rectInDesktop,center=centerInDesktop,score=result.score))
        return MatchAllInWindowByOneTemplateRes(matchsInWindow=matchsInWindow,matchsInDesktop=matchsInDesktop)

    def matchOneInWindowByMulTemplate(self,names,windowTitle="screen",threshold=0.9,img:np.ndarray=np.zeros(0))->MatchOneInWindowByMulTemplateRes:
        '''
        窗口中的模板列表匹配，返回第一个在列表能匹配成功的坐标
        names: 模板名称列表
        windowTitle: 窗口标题 默认整个桌面
        simThreshold: 相似度阈值 默认0.9
        img: 图片矩阵 3通道BGR 默认空 则从窗口中获取 用来测试用的
        '''
        for name in names:
            result = self.matchOneInWindowByOneTemplate(name,windowTitle,img=img)
            result = MatchOneInWindowByMulTemplateRes(rectInWindow=result.rectInWindow,centerInWindow=result.centerInWindow,rectInDesktop=result.rectInDesktop,centerInDesktop=result.centerInDesktop,score=result.score,name=name,success=False)
            if result.score >= threshold:
                result.success = True
                return result
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

class MatchRes(ctypes.Structure):
    _fields_ = [("xmin",ctypes.c_int),("ymin",ctypes.c_int),("xmax",ctypes.c_int),("ymax",ctypes.c_int),
                ("centerX",ctypes.c_int),("centerY",ctypes.c_int),("score",ctypes.c_double)]
snapDetect.matchInWindow.argtypes = [ctypes.c_char_p,ctypes.c_char_p]
snapDetect.matchInWindow.restype = MatchRes

matchRes = snapDetect.matchInWindow(ctypes.c_char_p("鼠标".encode('utf-8')),ctypes.c_char_p("测试".encode('utf-8')))
t1 = time.time()
for i in range(10):
    matchRes = snapDetect.matchInWindow(ctypes.c_char_p("鼠标".encode('utf-8')),ctypes.c_char_p("测试".encode('utf-8')))
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