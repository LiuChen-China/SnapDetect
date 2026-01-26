cmake_minimum_required (VERSION 3.10) 
project ("SnapDetect") #工程名

#C++版本的标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 添加UTF-8编码支持
add_compile_options("/utf-8")

# ========== 固定输出目录（适配VSCode CMake Tools） ==========
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/build/Release")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/build/Debug")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/build/Release/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/build/Debug/lib")

# 自定义编译的OpenCV根目录
set(MY_OPENCV_DIR "D:/opencv-4.8.0")
set(OpenCV_DIR "${MY_OPENCV_DIR}/build")
find_package(OpenCV REQUIRED 
    COMPONENTS core imgcodecs imgproc highgui videoio
)

# ========== 优化：补全构建模式提示 ==========
if(CMAKE_BUILD_TYPE)
    message(STATUS "当前构建模式: ${CMAKE_BUILD_TYPE} (由CMake Tools控制)")
else()
    message(STATUS "当前构建模式: 配置阶段未指定（构建时由CMake Tools选择Release/Debug）")
endif()
message(STATUS "OpenCV_LIBS: ${OpenCV_LIBS}")
message(STATUS "可执行文件输出目录(Release): ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}")
message(STATUS "可执行文件输出目录(Debug): ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}")

include_directories(${CMAKE_SOURCE_DIR}/include) 
include_directories(${OpenCV_INCLUDE_DIRS})

#自动添加所有源文件
file(GLOB SRC_FILES "source/*.cpp")

# ========== 创建目标（核心功能） ==========
add_executable(${PROJECT_NAME} main.cpp ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS})

add_library(${PROJECT_NAME}Lib SHARED ${SRC_FILES})
target_link_libraries(${PROJECT_NAME}Lib PRIVATE ${OpenCV_LIBS})

# ========== 配置时复制Release DLL（失败仅警告） ==========
set(OPENCV_DLL_RELEASE
    "${MY_OPENCV_DIR}/build/bin/Release/opencv_core480.dll"
    "${MY_OPENCV_DIR}/build/bin/Release/opencv_imgcodecs480.dll"
    "${MY_OPENCV_DIR}/build/bin/Release/opencv_imgproc480.dll"
    "${MY_OPENCV_DIR}/build/bin/Release/opencv_highgui480.dll"
    "${MY_OPENCV_DIR}/build/bin/Release/opencv_videoio480.dll"
)

foreach(dll ${OPENCV_DLL_RELEASE})
    if(EXISTS ${dll})
        file(COPY ${dll} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE})
        message(STATUS "✅ 已复制Release DLL: ${dll}")
    else()
        message(WARNING "⚠️ 未找到Release DLL: ${dll}，请手动复制到 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}")
    endif()
endforeach()

# 链接库路径
link_directories("${MY_OPENCV_DIR}/build/lib/Release")
link_directories("${MY_OPENCV_DIR}/build/lib/Debug")

# 最终提示
message(STATUS "======================================")
message(STATUS "📌 编译完成后操作提示：")
message(STATUS "   1. 可执行文件路径：build/Release/SnapDetect.exe")
message(STATUS "   2. 若运行提示缺DLL，手动复制OpenCV的Release DLL到上述目录")
message(STATUS "   3. Debug模式同理，复制Debug版DLL到 build/Debug")
message(STATUS "======================================")