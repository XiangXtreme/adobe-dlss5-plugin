# Adobe DLSS 5 神经渲染视频插件 (AE & PR)

<div align="center">

![License](https://img.shields.io/badge/License-MIT-green.svg)
![Platform](https://img.shields.io/badge/Platform-Windows%20x64-blue.svg)
![Adobe](https://img.shields.io/badge/Adobe-AE%20%7C%20PR%202022--2026-orange.svg)
![NVIDIA](https://img.shields.io/badge/NVIDIA-RTX%20TensorRT-76B900.svg)

**基于 NVIDIA DLSS 5 (Feature 18 神经渲染架构) 的 Adobe After Effects 与 Premiere Pro 原生滤镜插件。**

提供同分辨率视频画质 AI 神经重构、局部细节增强与视频智能降噪。

[English](README.md) | [中文说明](README_ZH.md)

</div>

---

## ✨ 核心特性

- **⚡ AE / PR 双宿主原生支持**：单个 `.aex` 插件文件，同时支持 **Adobe After Effects** 与 **Adobe Premiere Pro**（2022 ~ 2026）。
- **🧠 DLSS 5 神经渲染引擎**：充分利用 NVIDIA Tensor Core 执行同分辨率下的深度学习神经重建与纹理合成。
- **🎨 明确的宿主像素格式支持**：SmartRender 支持 AE ARGB 8/16/32-bpc 与 Premiere BGRA 8-bit/32f，不再通过行距猜测格式。神经运行时使用 RGBA8 桥接；浮点旁路及对比视图的原图部分保留宿主数值。
- **🔍 实时视觉对比与调试视图**：
  - `Processed`：完整神经网络重构输出。
  - `Difference x10`：10倍差异放大视图（直观查看 AI 修复/增补的微观细节）。
  - `Left / Right Compare`：实时左右分屏对比（左侧原图，右侧 DLSS）。
- **🚀 极速渲染与多线程优化**：
  - 基于 OpenMP 的并行 SIMD 像素转换加速。
  - 当前分辨率缓冲建立后，稳态渲染不再逐帧分配帧缓冲。
  - MFR 调用安全同步，并在效果实例切换或乱序帧时重置时序历史。

---

## 参数说明

| 参数名称 | 类型 | 默认值 | 说明 |
| :--- | :--- | :--- | :--- |
| **Enable DLSS Neural Rendering** | 复选框 | `开 (True)` | 插件全局总开关。 |
| **Style** | 下拉菜单 | `Default` | 渲染风格（Default / Natural / Cinema / Style 3）。 |
| **Intensity** | 浮点滑块 | `1.00` | AI 重构与增强强度（范围 0.0 ~ 2.0）。 |
| **Local Tone** | 浮点滑块 | `1.00` | 局部对比度与明暗层次增强。 |
| **Local Structure** | 浮点滑块 | `1.00` | 微观纹理重建与边缘锐化。 |
| **Skin Structure** | 浮点滑块 | `1.00` | 人像肤质细节与质感保护权重。 |
| **Output View** | 下拉菜单 | `Processed` | 输出视图（处理结果 / 10倍差异对比 / 左右分屏对比）。 |
| **Output Mix** | 浮点滑块 | `1.00` | 处理结果与原始画面的混合比率 (0.0 ~ 1.0)。 |

---

## 📦 安装方法

### 方式一：一键自动安装（推荐）
1. 从 [Releases](https://github.com/XiangXtreme/adobe-dlss5-plugin/releases) 页面下载发行包压缩包 `Adobe-DLSS5-Neural-Video-v1.0.0-Win64.zip`。
2. 解压后，双击运行 **`Install.bat`**。
3. 打开 After Effects 或 Premiere Pro，在以下位置找到插件：
   **`效果 (Effect) > DLSS Experimental > DLSS Neural Video`**

### 方式二：手动安装
将 `DLSS_Neural_Video.aex`、`dlssnr_host.dll` 和 `nvngx_dlssnr.dll` 复制到以下任意一个目录：
- **AE 专属目录**：`[你的AE安装路径]\Support Files\Plug-ins\Effects\`
- **通用公共目录**：`C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\`

---

## 💻 硬件与环境要求

- **操作系统**：Windows 10 / 11 (64-bit)
- **显卡**：NVIDIA GeForce RTX 20 / 30 / 40 / 50 系列（需支持 DirectX 12 与 Tensor Core）
- **驱动**：NVIDIA Game Ready / Studio 驱动 530+
- **Adobe 软件**：Adobe After Effects 2022 ~ 2026 / Adobe Premiere Pro 2022 ~ 2026

---

## 🔨 从源码构建

构建前请安装 Visual Studio 2022、CMake 3.25+，并将 Adobe After Effects SDK 解压到 `third_party/Adobe_AE_SDK`。运行或打包插件时，还需将运行时文件放入 `third_party/runtime`。

```powershell
# 1. 克隆代码仓库
git clone https://github.com/XiangXtreme/adobe-dlss5-plugin.git
cd adobe-dlss5-plugin

# 2. 确认 SDK 文件存在：
#    third_party/Adobe_AE_SDK/Examples/Headers/AE_Effect.h

# 3. 生成工程并编译
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure

# 4. 将运行时文件放入 third_party/runtime 后打包发布
pwsh -File .\scripts\package-release.ps1
```

---

## 📄 开源许可与致谢

- 本项目基于 [MIT 许可证](LICENSE) 开源。
- 致谢 [SAOG0721/DaVinci-Resolve-DLSS5](https://github.com/SAOG0721/DaVinci-Resolve-DLSS5) 项目提供的 Feature 18 架构研究与 OpenFX 灵感。
- After Effects 与 Premiere Pro 为 Adobe 公司的注册商标。
- NVIDIA 与 DLSS 为 NVIDIA 公司的注册商标。
