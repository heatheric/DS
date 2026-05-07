cmake_minimum_required(VERSION 3.16)
project(ScreenRecorderPlayer VERSION 1.0.0 LANGUAGES CXX)

# ============================================================================
# 构建模式
# ============================================================================

# 仅支持 RelWithDebInfo（开发调试）和 Release（发布）
if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(WARNING "Debug is not supported, falling back to RelWithDebInfo")
    endif()
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

if(NOT CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo)$")
    message(FATAL_ERROR "Unsupported build type: ${CMAKE_BUILD_TYPE}. Use Release or RelWithDebInfo.")
endif()

# 覆盖 CMake 默认的编译选项（在 project() 之后覆盖，避免被重置）
# RelWithDebInfo: 无优化 + 调试符号（便于调试）
set(CMAKE_C_FLAGS_RELWITHDEBINFO   "-O0 -g")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O0 -g")
# Release: O2 优化 + 去除断言（生产发布）
set(CMAKE_C_FLAGS_RELEASE   "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# ============================================================================
# 外部依赖路径检查
# ============================================================================

if(NOT Qt5_DIR)
    message(FATAL_ERROR "Qt5_DIR is not set. Pass -DQt5_DIR=<path>")
endif()

if(NOT FFMPEG_ROOT)
    message(FATAL_ERROR "FFMPEG_ROOT is not set. Pass -DFFMPEG_ROOT=<path>")
endif()

# ============================================================================
# 查找 Qt5
# ============================================================================

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui OpenGL Concurrent)

# ============================================================================
# 查找 FFmpeg 库
# ============================================================================

set(FFMPEG_INCLUDE_DIR ${FFMPEG_ROOT}/include)
set(FFMPEG_LIB_DIR ${FFMPEG_ROOT}/lib)

set(FFMPEG_LIB_NAMES avcodec avformat avdevice avutil avfilter swresample swscale)
foreach(LIB ${FFMPEG_LIB_NAMES})
    find_library(FFMPEG_${LIB}_LIBRARY
        NAMES ${LIB}
        PATHS ${FFMPEG_LIB_DIR}
        REQUIRED
    )
    list(APPEND FFMPEG_LIBRARIES ${FFMPEG_${LIB}_LIBRARY})
endforeach()

# ============================================================================
# Windows 系统库（项目级全局链接依赖，所有目标共用）
# ============================================================================

set(WIN32_SYSTEM_LIBS ws2_32 user32 gdi32)

# ============================================================================
# 目标: ScreenRecorderPlayer（主程序）
# ============================================================================

add_executable(ScreenRecorderPlayer)

target_sources(ScreenRecorderPlayer PRIVATE
    cpp/main.cpp
    cpp/mainwindow.cpp
    cpp/recordwidget.cpp
    cpp/screenrecorder.cpp
    cpp/videoplayer.cpp
    cpp/filemanager.cpp
    cpp/tabmanager.cpp
    hpp/mainwindow.hpp
    hpp/recordwidget.hpp
    hpp/screenrecorder.hpp
    hpp/videoplayer.hpp
    hpp/filemanager.hpp
    hpp/tabmanager.hpp
)

target_compile_features(ScreenRecorderPlayer PRIVATE cxx_std_17)

target_include_directories(ScreenRecorderPlayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/hpp
    ${FFMPEG_INCLUDE_DIR}
)

target_compile_definitions(ScreenRecorderPlayer PRIVATE
    QT_DEPRECATED_WARNINGS
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
)

target_compile_options(ScreenRecorderPlayer PRIVATE
    -Wall -Wextra -m64
    -ffunction-sections -fdata-sections
)

target_link_options(ScreenRecorderPlayer PRIVATE
    -Wl,--gc-sections -m64
)

target_link_libraries(ScreenRecorderPlayer PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    Qt5::OpenGL
    Qt5::Concurrent
    ${FFMPEG_LIBRARIES}
    ${WIN32_SYSTEM_LIBS}
    opengl32
    d3d11
    dxgi
)

set_target_properties(ScreenRecorderPlayer PROPERTIES
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    WIN32_EXECUTABLE FALSE
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

# ============================================================================
# 目标: test_dxgi_recorder（录制功能测试，无 Qt 依赖）
# ============================================================================

add_executable(test_dxgi_recorder)

target_sources(test_dxgi_recorder PRIVATE
    cpp/test_dxgi_recorder.cpp
)

target_compile_features(test_dxgi_recorder PRIVATE cxx_std_17)

target_include_directories(test_dxgi_recorder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/hpp
    ${FFMPEG_INCLUDE_DIR}
)

target_compile_definitions(test_dxgi_recorder PRIVATE
    QT_DEPRECATED_WARNINGS
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
)

target_compile_options(test_dxgi_recorder PRIVATE
    -Wall -Wextra -m64
    -ffunction-sections -fdata-sections
)

target_link_options(test_dxgi_recorder PRIVATE
    -Wl,--gc-sections -m64
)

target_link_libraries(test_dxgi_recorder PRIVATE
    ${FFMPEG_LIBRARIES}
    d3d11
    dxgi
    ${WIN32_SYSTEM_LIBS}
)

set_target_properties(test_dxgi_recorder PROPERTIES
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    WIN32_EXECUTABLE FALSE
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

# ============================================================================
# 目标: test_ffmpeg_player（播放功能测试）
# ============================================================================

add_executable(test_ffmpeg_player)

target_sources(test_ffmpeg_player PRIVATE
    cpp/test_ffmpeg_player.cpp
)

target_compile_features(test_ffmpeg_player PRIVATE cxx_std_17)

target_include_directories(test_ffmpeg_player PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/hpp
    ${FFMPEG_INCLUDE_DIR}
)

target_compile_definitions(test_ffmpeg_player PRIVATE
    QT_DEPRECATED_WARNINGS
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
)

target_compile_options(test_ffmpeg_player PRIVATE
    -Wall -Wextra -m64
    -ffunction-sections -fdata-sections
)

target_link_options(test_ffmpeg_player PRIVATE
    -Wl,--gc-sections -m64
)

target_link_libraries(test_ffmpeg_player PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    Qt5::OpenGL
    ${FFMPEG_LIBRARIES}
    ${WIN32_SYSTEM_LIBS}
    opengl32
)

set_target_properties(test_ffmpeg_player PROPERTIES
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    AUTOMOC ON
    WIN32_EXECUTABLE FALSE
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)
# 原属性（无 AUTOUIC/AUTORCC）:
# set_target_properties(test_ffmpeg_player PROPERTIES
#     CXX_STANDARD_REQUIRED ON
#     CXX_EXTENSIONS OFF
#     AUTOMOC ON
#     WIN32_EXECUTABLE FALSE
#     RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
# )

# ============================================================================
# 安装规则（已禁用 — 发布通过 bin/run.bat 启动）
#   cmake --install build/Release --prefix ${custom.installBase}
# ============================================================================
#
# install(TARGETS ScreenRecorderPlayer RUNTIME DESTINATION .)
# install(DIRECTORY ${FFMPEG_ROOT}/bin/
#     DESTINATION .
#     FILES_MATCHING PATTERN "*.dll"
# )
# install(DIRECTORY ${Qt5_DIR}/../../bin/
#     DESTINATION .
#     FILES_MATCHING PATTERN "Qt5*.dll"
# )

# ============================================================================
# 编译命令导出（供 IDE 智能提示）
# ============================================================================

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# 构建概览
# ============================================================================

message(STATUS "========================================")
message(STATUS "  Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "  Build:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++:     C++17")
message(STATUS "  Make:    ${CMAKE_MAKE_PROGRAM}")
message(STATUS "  Qt5:     ${Qt5_DIR}")
message(STATUS "  FFmpeg:  ${FFMPEG_ROOT}")
message(STATUS "  Output:  ${CMAKE_SOURCE_DIR}/bin")
message(STATUS "========================================")
