import cv2
import ctypes
import mss
import time
from pydantic import BaseModel, Field
from typing import Literal, Optional, List, Dict, Any, Union
import os
from glob import glob
import win32gui
import pyautogui
import numpy as np
import sys
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.dirname(os.path.abspath(__file__))+"/dll")

class Rect(BaseModel):
    '''框信息'''
    xmin: int = Field(..., description="框左上角x坐标")
    ymin: int = Field(..., description="框左上角y坐标")
    xmax: int = Field(..., description="框右下角x坐标")
    ymax: int = Field(..., description="框右下角y坐标")

class Center(BaseModel):
    '''中心信息'''
    x: int = Field(..., description="中心x坐标")
    y: int = Field(..., description="中心y坐标")

class MatchOneRes(BaseModel):
    '''一个匹配结果'''
    rect: Rect = Field(..., description="匹配框")
    center: Center = Field(..., description="匹配框中心")
    score: float = Field(..., description="匹配得分")


class ScreenOperator():
    '''一些对整个屏幕的操作'''
    def __init__(self):
        super(ScreenOperator, self).__init__()
        self.templateDict = {}#key 模板标题 value {'template':mat,'mask':mat or None}
        self.camera = mss.mss()
        
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
        img = self.camera.grab({"left": rect.xmin, "top": rect.ymin, "width": rect.xmax - rect.xmin, "height": rect.ymax - rect.ymin})
        img = np.array(img)[:,:,:3]
        return img

    def getWindowMat(self,content="screen"):
        '''根据窗口标题包含文本返回窗口矩阵'''
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
        xmax = xmin + self.templateDict[name]["template"].shape[1]
        ymax = ymin + self.templateDict[name]["template"].shape[0]
        rect = Rect(xmin=xmin,ymin=ymin,xmax=xmax,ymax=ymax)
        center = Center(x=xmin+self.templateDict[name]["template"].shape[1]//2,y=ymin+self.templateDict[name]["template"].shape[0]//2)
        return MatchOneRes(rect=rect,center=center,score=maxVal)

    def matchOneInWindowByOneTemplate(self,name,windowTitle="screen")->MatchOneRes:
        '''
        使用一个模板 匹配出窗口中最相似的一个位置
        默认使用模板组的第一个模板
        name: 模板名称
        windowTitle: 窗口标题包含文本
        '''
        img = self.getWindowMat(windowTitle)
        return self.matchOneInImgByOneTemplate(name,img)

templatePath = "鼠标.png"
templateName = "鼠标"
windowName = "测试"
loop = 10

#############################################C++测试部分#############################################################
snapDetect = ctypes.WinDLL('dll/SnapDetectLib.dll')
snapDetect.addTemplate(ctypes.c_char_p(templatePath.encode('utf-8')),ctypes.c_char_p(templateName.encode('utf-8')))
class MatchRes(ctypes.Structure):
    _fields_ = [("xmin",ctypes.c_int),("ymin",ctypes.c_int),("xmax",ctypes.c_int),("ymax",ctypes.c_int),
                ("centerX",ctypes.c_int),("centerY",ctypes.c_int),("score",ctypes.c_double)]
snapDetect.matchInWindow.argtypes = [ctypes.c_char_p,ctypes.c_char_p]
snapDetect.matchInWindow.restype = MatchRes
def matchByCPP(name,windowTitle="screen")->MatchOneRes:
    '''C++版本的窗口模板匹配'''
    res = snapDetect.matchInWindow(ctypes.c_char_p(name.encode('utf-8')),ctypes.c_char_p(windowTitle.encode('utf-8')))
    return MatchOneRes(rect=Rect(xmin=res.xmin,ymin=res.ymin,xmax=res.xmax,ymax=res.ymax),center=Center(x=res.centerX,y=res.centerY),score=res.score)
matchByCPP(name=templateName,windowTitle=windowName)
t1 = time.time()
for i in range(loop):
    cppResult = matchByCPP(name=templateName,windowTitle=windowName)
t2 = time.time()
print(f'C++进行{loop}次匹配耗时={(t2-t1)/loop*1000:.4f}ms')
print(f'C++匹配结果：{cppResult}')

############################################python测试部分###########################################################
screenOperator = ScreenOperator()
screenOperator.addTemplate(path=templatePath,name=templateName)
screenOperator.matchOneInWindowByOneTemplate(name=templateName,windowTitle=windowName)
t1 = time.time()
for i in range(loop):
    pyResult = screenOperator.matchOneInWindowByOneTemplate(name=templateName,windowTitle=windowName)
t2 = time.time()
print(f'python进行{loop}次匹配耗时={(t2-t1)/loop*1000:.4f}ms')
print(f'python匹配结果：{pyResult}')

