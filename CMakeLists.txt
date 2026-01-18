cmake_minimum_required (VERSION 3.8)
project ("SnapDetect") #工程名
SET(CMAKE_BUILD_TYPE "Debug") #Debug或Release模式

#C++版本的标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 添加UTF-8编码支持
add_compile_options("/utf-8")


# 项目文件路径配置
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build) #项目源码构建路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) #存放可执行文件的目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib) #默认存放静态库的文件夹位置
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib) #默认存放动态库的文件夹位置

#opencv配置
set(MY_OPENCV_DIR "F:/opencv480")
list(APPEND CMAKE_PREFIX_PATH "${MY_OPENCV_DIR}/opencv/build/x64/vc16/lib")
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})
file(COPY "${MY_OPENCV_DIR}/opencv/build/x64/vc16/bin/opencv_world480d.dll" DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE})
file(COPY "${MY_OPENCV_DIR}/opencv/build/x64/vc16/bin/opencv_world480.dll" DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE})

#项目文件夹配置
include_directories(${CMAKE_SOURCE_DIR}/include) 

#自动添加所有源文件
file(GLOB SRC_FILES "source/*.cpp")
# 创建可执行文件
add_executable(${PROJECT_NAME} main.cpp ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})

#生成静态库
add_library(${PROJECT_NAME}Lib STATIC ${SRC_FILES})
target_link_libraries(${PROJECT_NAME}Lib ${OpenCV_LIBS})