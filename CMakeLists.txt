# CMakeLists.txt - Qt5+FFmpeg8屏幕录制与播放软件构建配置
# 项目名称: Screen Recorder & Player
# 平台: Windows 64位
# 编译器: MinGW64 (GCC 12)

cmake_minimum_required(VERSION 3.16)
project(ScreenRecorderPlayer VERSION 1.0.0 LANGUAGES CXX)

# 设置编译器路径（更新为新的MinGW路径）
set(MINGW_DIR "D:/Program/mingw64_gcc12")
set(CMAKE_C_COMPILER "${MINGW_DIR}/bin/gcc.exe")
set(CMAKE_CXX_COMPILER "${MINGW_DIR}/bin/g++.exe")

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型（只支持Release和RelWithDebInfo）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 验证构建类型
if(NOT CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo)$")
    message(FATAL_ERROR "只支持Release和RelWithDebInfo构建模式，当前模式: ${CMAKE_BUILD_TYPE}")
endif()

# 设置输出目录（项目根目录下的bin文件夹）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
# 移除静态库输出目录设置（本项目不生成静态库）

# 设置编译器标志（64位优化）
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # 通用标志
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -m64")
    
    # Release模式：最大优化，无调试信息
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -ffunction-sections -fdata-sections")
    
    # RelWithDebInfo模式：不优化，包含调试信息（方便调试）
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O0 -g -ffunction-sections -fdata-sections")
    
    # 禁用Debug模式
    set(CMAKE_CXX_FLAGS_DEBUG "")
    
    # 链接时优化
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections -m64")
endif()

# Qt5配置
set(QT5_DIR "X:/Qt/5.15.13/gcc12")
set(CMAKE_PREFIX_PATH ${QT5_DIR} ${CMAKE_PREFIX_PATH})

# 查找Qt5组件
find_package(Qt5 REQUIRED COMPONENTS
    Core
    Widgets
    Gui
    OpenGL
    Concurrent
)

# 启用Qt5的MOC、UIC、RCC自动处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# FFmpeg8配置
set(FFMPEG_DIR "D:/Program/ffmpeg")

# 设置FFmpeg包含目录
set(FFMPEG_INCLUDE_DIRS
    ${FFMPEG_DIR}/include
)

# 设置FFmpeg库目录
set(FFMPEG_LIBRARY_DIRS
    ${FFMPEG_DIR}/lib
)

# 查找FFmpeg库
find_library(AVCODEC_LIBRARY
    NAMES avcodec
    PATHS ${FFMPEG_LIBRARY_DIRS}
    REQUIRED
)

find_library(AVFORMAT_LIBRARY
    NAMES avformat
    PATHS ${FFMPEG_LIBRARY_DIRS}
    REQUIRED
)

find_library(AVDEVICE_LIBRARY
    NAMES avdevice
    PATHS ${FFMPEG_LIBRARY_DIRS}
    REQUIRED
)

find_library(AVUTIL_LIBRARY
    NAMES avutil
    PATHS ${FFMPEG_LIBRARY_DIRS}
    REQUIRED
)

find_library(AVFILTER_LIBRARY
    NAMES avfilter
    PATHS ${FFMPEG_LIBRARY_DIRS}
    REQUIRED
)

find_library(SWRESAMPLE_LIBRARY
    NAMES swresample
    PATHS ${FFMPEG_LIBRARY_DIRS}
    REQUIRED
)

find_library(SWSCALE_LIBRARY
    NAMES swscale
    PATHS ${FFMPEG_LIBRARY_DIRS}
    REQUIRED
)

# 设置FFmpeg库变量
set(FFMPEG_LIBRARIES
    ${AVCODEC_LIBRARY}
    ${AVFORMAT_LIBRARY}
    ${AVDEVICE_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${AVFILTER_LIBRARY}
    ${SWRESAMPLE_LIBRARY}
    ${SWSCALE_LIBRARY}
)

# 设置包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/hpp
    ${FFMPEG_INCLUDE_DIRS}
)

# 设置源文件
set(SOURCES
    cpp/main.cpp
    cpp/mainwindow.cpp
    cpp/screenrecorder.cpp
    cpp/videoplayer.cpp
    cpp/filemanager.cpp
    cpp/tabmanager.cpp
)

# 设置头文件（用于MOC处理）
set(HEADERS
    hpp/mainwindow.hpp
    hpp/screenrecorder.hpp
    hpp/videoplayer.hpp
    hpp/filemanager.hpp
    hpp/tabmanager.hpp
)

# 创建可执行文件
add_executable(ScreenRecorderPlayer
    ${SOURCES}
    ${HEADERS}
)

# 链接Qt5库
target_link_libraries(ScreenRecorderPlayer
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    Qt5::OpenGL
    Qt5::Concurrent
)

# 链接FFmpeg库
target_link_libraries(ScreenRecorderPlayer
    ${FFMPEG_LIBRARIES}
)

# 设置目标属性
target_compile_definitions(ScreenRecorderPlayer
    PRIVATE
        QT_DEPRECATED_WARNINGS
        QT_DISABLE_DEPRECATED_BEFORE=0x060000
)

# Windows特定设置
if(WIN32)
    # 设置Windows子系统为控制台（便于调试）
    set_target_properties(ScreenRecorderPlayer PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
    
    # 添加Windows库
    target_link_libraries(ScreenRecorderPlayer
        ws2_32
        user32
        gdi32
        opengl32
    )
    
    # 设置运行时库
    if(MSVC)
        set_target_properties(ScreenRecorderPlayer PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    endif()
endif()

# workdir目录由运行时程序自行检查创建

# 构建后信息显示
message(STATUS "项目配置完成:")
message(STATUS "  项目名称: ${PROJECT_NAME}")
message(STATUS "  版本: ${PROJECT_VERSION}")
message(STATUS "  构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Qt5路径: ${QT5_DIR}")
message(STATUS "  FFmpeg路径: ${FFMPEG_DIR}")
message(STATUS "  输出目录: ${CMAKE_SOURCE_DIR}/bin")

# 构建帮助信息
message("\n构建说明:")
message("  构建项目: cmake --build . --config Release")
message("  清理构建: cmake --build . --target clean")
message("  运行程序: ./bin/ScreenRecorderPlayer.exe")

# 检查依赖项
if(NOT Qt5_FOUND)
    message(WARNING "Qt5库未找到，请检查QT5_DIR路径设置")
endif()

if(NOT AVCODEC_LIBRARY)
    message(WARNING "FFmpeg库未找到，请检查FFMPEG_DIR路径设置")
endif()