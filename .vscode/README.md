# VS Code 工作区配置说明

## 概述

此文件夹包含Qt5+FFmpeg8屏幕录制与播放软件的VS Code工作区配置，确保IntelliSense、代码补全、调试和构建功能正常工作。

## 配置文件说明

### 1. c_cpp_properties.json

**作用**: C/C++扩展的IntelliSense配置
**关键配置**:

- **includePath**: 包含Qt5、FFmpeg8、MinGW64和项目头文件路径
- **defines**: 定义Qt库宏和Windows平台宏
- **compilerPath**: 指定MinGW64编译器路径
- **cppStandard**: 设置为C++17标准
- **intelliSenseMode**: 使用Windows GCC x64模式

### 2. settings.json

**作用**: 工作区级别的VS Code设置
**关键配置**:

- **C/C++设置**: 包含路径、编译器路径、标准设置
- **CMake配置**: 自动配置、构建目录、生成器设置（仅支持Release和RelWithDebInfo）
- **文件关联**: .hpp/.cpp文件关联到C++语言
- **格式化**: 保存时自动格式化代码
- **终端环境**: 设置PATH环境变量包含所有工具路径

### 3. launch.json

**作用**: 调试配置
**提供两种调试模式**:

1. **Release模式调试**: 调试Release构建的可执行文件
2. **RelWithDebInfo模式调试**: 调试带调试信息的优化构建

### 4. tasks.json

**作用**: 任务配置
**提供以下任务**:

- **构建项目 (Release)**: 使用CMake构建Release版本
- **构建项目 (RelWithDebInfo)**: 使用CMake构建带调试信息的优化版本
- **清理构建**: 清理构建目录
- **重新配置CMake (Release)**: 重新生成Release配置
- **重新配置CMake (RelWithDebInfo)**: 重新生成RelWithDebInfo配置
- **运行程序 (Release)**: 运行Release版本程序
- **运行程序 (RelWithDebInfo)**: 运行RelWithDebInfo版本程序

## 环境要求

### 必需工具

1. **MinGW64**: D:/Program/mingw64_gcc12
2. **Qt5**: X:/Qt/5.15.13/gcc12
3. **FFmpeg8**: D:/Program/ffmpeg
4. **CMake**: 系统PATH中

### 推荐VS Code扩展

- C/C++ (ms-vscode.cpptools)
- CMake Tools (ms-vscode.cmake-tools)
- Qt Tools (qt-vscode.qt-vscode)
- Makefile Tools (ms-vscode.makefile-tools)

## 使用方法

### 1. 打开工作区

```bash
code e:\Temp\workdir\DS
```

### 2. 配置CMake（首次使用）

1. 按Ctrl+Shift+P
2. 输入"CMake: Configure"
3. 选择"MinGW Makefiles"生成器
4. 选择构建模式：Release 或 RelWithDebInfo

### 3. 构建项目

**Release版本（推荐用于生产环境）**

1. 按Ctrl+Shift+P
2. 输入"Tasks: Run Task"
3. 选择"构建项目 (Release)"

**RelWithDebInfo版本（推荐用于调试）**

1. 按Ctrl+Shift+P
2. 输入"Tasks: Run Task"
3. 选择"构建项目 (RelWithDebInfo)"

### 4. 调试程序

**调试Release版本**

1. 按F5或选择调试配置
2. 选择"调试 ScreenRecorderPlayer (Release)"

**调试RelWithDebInfo版本**

1. 按F5或选择调试配置
2. 选择"调试 ScreenRecorderPlayer (RelWithDebInfo)"

## 路径配置说明

所有路径都基于requirements.txt中的指定路径：

- **Qt5路径**: X:/Qt/5.15.13/gcc12
- **FFmpeg8路径**: D:/Program/ffmpeg
- **MinGW64路径**: D:/Program/mingw64_gcc12

如果实际路径不同，需要修改对应的配置文件。

## 故障排除

### IntelliSense不工作

1. 检查C/C++扩展是否安装
2. 验证路径配置是否正确
3. 重新加载窗口(Ctrl+Shift+P -> "Developer: Reload Window")

### CMake配置失败

1. 检查CMake是否安装并在PATH中
2. 验证编译器路径是否正确
3. 检查Qt5和FFmpeg8路径是否存在

### 构建失败

1. 检查依赖库是否正确链接
2. 验证CMakeLists.txt配置
3. 查看构建输出中的错误信息

## 注意事项

1. 所有路径使用正斜杠(/)分隔
2. 环境变量PATH在终端中自动设置
3. 构建目录设置为build/，输出目录设置为bin/
4. 工作区设置会覆盖用户设置，确保一致性
