# AmazingPlayer 构建说明

## 概述

AmazingPlayer 是一个基于 C++ 的多媒体播放器，使用以下技术栈：

- **FFmpeg 6.1.1** - 音视频解码
- **SDL2** - 窗口管理和音频输出
- **GLAD** - OpenGL 函数加载
- **OpenGL** - 视频渲染
- **Conan** - 依赖管理
- **CMake** - 构建系统

## 功能特性

### 播放器功能
- 支持主流视频格式（MP4、AVI、MKV等）
- 硬件加速渲染（OpenGL）
- 音视频同步播放
- 多线程解码架构
- 基本播放控制（播放/暂停/快进/快退）

### 控制说明
- **空格键** - 播放/暂停
- **左方向键** - 快退10秒
- **右方向键** - 快进10秒
- **ESC键** - 退出播放器

## 系统要求

- **操作系统**: Windows 10+, macOS 10.14+, Linux (Ubuntu 18.04+)
- **编译器**: 支持 C++17 的编译器
  - GCC 7.0+
  - Clang 5.0+
  - MSVC 2019+
- **CMake**: 3.15+
- **Python**: 3.6+ (用于 Conan)

## 构建步骤

### 1. 安装依赖工具

#### macOS
```bash
# 安装 Homebrew (如果没有)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 安装构建工具
brew install cmake python3

# 安装 Conan
pip3 install conan
```

#### Ubuntu/Debian
```bash
# 更新包管理器
sudo apt update

# 安装构建工具
sudo apt install cmake python3 python3-pip build-essential

# 安装 Conan
pip3 install conan
```

#### Windows
```bash
# 使用 Chocolatey 安装 (推荐)
choco install cmake python3

# 或者手动下载安装 CMake 和 Python
# 安装 Conan
pip install conan
```

### 2. 配置 Conan

```bash
# 添加默认远程仓库
conan remote add conancenter https://center.conan.io

# 配置默认 profile
conan profile detect --force
```

### 3. 构建项目

```bash
# 克隆项目（如果需要）
git clone <your-repo-url>
cd AmazingPlayer

# 安装依赖
conan install . --output-folder=build --build=missing

# 配置 CMake
cmake --preset conan-default

# 构建项目
cmake --build --preset conan-release
```

### 4. 运行播放器

```bash
# Linux/macOS
./build/Release/AmazingPlayer path/to/your/video.mp4

# Windows
.\build\Release\AmazingPlayer.exe path\to\your\video.mp4
```

## 项目结构

```
AmazingPlayer/
├── src/
│   ├── main.cpp                 # 主程序入口
│   └── Render/
│       ├── PlayerRender.h       # 播放器渲染类头文件
│       ├── PlayerRender.cpp     # 播放器渲染类实现
│       ├── TriangleRenderer.h   # 三角形渲染器（示例）
│       └── TriangleRenderer.cpp # 三角形渲染器实现
├── CMakeLists.txt              # CMake 配置文件
├── conanfile.py               # Conan 配置文件
├── conandata.yml              # Conan 依赖列表
└── README.md                  # 项目说明
```

## 播放器架构

### 核心组件

1. **PlayerRender 类**
   - 主要的播放器渲染类
   - 负责音视频解码、渲染和同步

2. **FFmpeg 集成**
   - 使用 FFmpeg 进行音视频解码
   - 支持多种编解码器

3. **SDL2 集成**
   - 窗口管理和事件处理
   - 音频输出

4. **OpenGL 渲染**
   - 使用 OpenGL 进行视频帧渲染
   - 支持硬件加速

### 多线程架构

- **主线程**: UI 渲染和事件处理
- **解码线程**: 音视频解码
- **音频线程**: 音频播放回调

## 故障排除

### 常见问题

1. **依赖安装失败**
   ```bash
   # 清理 Conan 缓存
   conan remove "*" --confirm
   
   # 重新安装
   conan install . --output-folder=build --build=missing
   ```

2. **编译错误**
   - 确保编译器支持 C++17
   - 检查 CMake 版本 (需要 3.15+)

3. **运行时错误**
   - 确保视频文件路径正确
   - 检查视频文件格式是否被支持

### 性能优化

1. **编译优化**
   ```bash
   # 使用 Release 模式编译
   cmake --build --preset conan-release
   ```

2. **运行时优化**
   - 使用 SSD 存储视频文件
   - 确保足够的内存
   - 关闭不必要的后台程序

## 支持的格式

### 视频格式
- MP4, AVI, MKV, MOV, WMV, FLV

### 音频格式
- MP3, AAC, WAV, FLAC, OGG

### 编解码器
- H.264, H.265, VP8, VP9
- AAC, MP3, AC3, DTS

## 开发和贡献

### 开发环境设置

1. 安装开发工具
2. 配置 IDE (推荐 CLion, VS Code, 或 Visual Studio)
3. 设置调试配置

### 代码规范

- 使用 C++17 标准
- 遵循 Google C++ 风格指南
- 适当添加注释和文档

## 许可证

本项目基于 MIT 许可证开源。

## 联系方式

如有问题或建议，请通过 GitHub Issues 联系我们。 