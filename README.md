# 项目描述
打包C++的dxgi截屏+opencv的模板匹配功能成dll，给python调用，打算给游戏脚本加速的，效果不尽人意...
# 使用方法
直接看compare.py吧，虽然效果不好，但学到了C++->dll->python调用的过程
# 环境要求
- 系统:win10
- 编译器:VS2019以上应该都可以
- IDE:VSCODE
- opencv4.8.0 编译版
# 注意
- 所有接口线程不安全
# 一些可能出现的问题
- 编译测试阶段VSCODE中文乱码，配置下终端输出编码，按这个博客https://blog.csdn.net/qq_52741275/article/details/140006241
# 对比数据
python端表现竟然超过python+dll...好像白干了/(ㄒoㄒ)/~~
```
python进行10次匹配耗时392ms
python匹配结果：Rect(xmin=489, ymin=80, xmax=518, ymax=108) Center(x=503, y=94) score=1.0
C++进行10次匹配耗时=519ms
C++匹配结果：Rect(xmin=481, ymin=72, xmax=510, ymax=100) Center(x=495, y=86) score=1.0
```
# 结论
- OpenCV、NumPy等主流 Python 库底层已是优化后的 C/C++ 实现，直接调用这些库的性能，通常优于自己手动封装的 C++ DLL（尤其是简单的内置功能，如模板匹配）。
- C++ 封装 DLL 的核心价值是 “补充 Python 生态的短板”，而非 “重写已有成熟库的功能”。