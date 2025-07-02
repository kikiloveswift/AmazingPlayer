# AmazingPlayer

一个基于 SDL2、OpenGL 和 FFmpeg 的跨平台媒体播放器项目。

## 技术栈

- **编程语言**: C++17
- **构建系统**: CMake 3.31+
- **包管理器**: Conan 2.x
- **图形渲染**: OpenGL (通过 glad)
- **窗口管理**: SDL2
- **音视频处理**: FFmpeg

## 依赖库

| 库名称 | 版本 | 用途 |
|--------|------|------|
| SDL2 | 2.30.5 | 窗口管理和事件处理 |
| glad | 0.1.36 | OpenGL 加载器 |
| FFmpeg | 6.1.1 | 音视频解码和处理 |

## 项目结构

```
AmazingPlayer/
├── cmake-build-debug/      # CMake 构建目录
├── src/                    # 源代码目录
│   └── main.cpp           # 主程序入口
├── CMakeLists.txt         # CMake 配置文件
├── conanfile.py           # Conan 配置文件
├── conandata.yml          # Conan 依赖版本信息
└── conan_provider.cmake   # Conan CMake 集成
```

## 环境要求

- C++ 编译器支持 C++17 标准
- CMake 3.31 或更高版本
- Conan 2.x 包管理器
- CLion IDE（推荐，已配置好 Conan 插件）

## 构建说明

### 使用 CLion（推荐）

如果您已经在 CLion 中配置好了 Conan 插件：

1. 使用 CLion 打开项目
2. CLion 会自动检测 CMakeLists.txt 并配置项目
3. Conan 插件会自动安装依赖
4. 点击运行按钮即可构建并运行项目

### 命令行构建

如果您想在命令行中构建项目：

```bash
# 1. 安装依赖
conda create -n conanCpp python=3.10
source activate conanCpp
pip install conan
# 2. 配置 conan
Clion 插件找到 conan，配置路径如下：
/home/<your_username>/.conda/envs/conanCpp/bin/conan
```

## 功能特性

当前版本是一个技术验证程序，主要功能包括：

- ✅ SDL2 窗口创建和管理
- ✅ OpenGL 上下文初始化
- ✅ FFmpeg 库集成
- ✅ 版本信息显示

## 开发计划

- [ ] 实现基本的视频文件播放功能
- [ ] 添加音频播放支持
- [ ] 实现播放控制（播放/暂停/停止）
- [ ] 添加进度条和时间显示
- [ ] 支持多种视频格式
- [ ] 添加字幕支持
- [ ] 实现播放列表功能

## 贡献指南

欢迎提交 Issue 和 Pull Request！

## 许可证

待定

## 联系方式

如有问题或建议，请通过 Issue 与我们联系。 