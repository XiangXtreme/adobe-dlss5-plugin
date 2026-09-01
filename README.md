# Adobe DLSS 5 Neural Video Plugin (AE & PR)

<div align="center">

![License](https://img.shields.io/badge/License-MIT-green.svg)
![Platform](https://img.shields.io/badge/Platform-Windows%20x64-blue.svg)
![Adobe](https://img.shields.io/badge/Adobe-AE%20%7C%20PR%202022--2026-orange.svg)
![NVIDIA](https://img.shields.io/badge/NVIDIA-RTX%20TensorRT-76B900.svg)

**Experimental DLSS 5 Neural Rendering (Feature 18) Plugin for Adobe After Effects & Premiere Pro.**

Provides AI-powered same-resolution neural reconstruction, micro-structure synthesis, and video denoising.

[English](README.md) | [中文说明](README_ZH.md)

</div>

---

## ✨ Features

- **⚡ Native Adobe Dual-Host Support**: A single unified `.aex` plugin automatically loaded by both **Adobe After Effects** and **Adobe Premiere Pro** (2022 ~ 2026).
- **🧠 DLSS 5 Feature 18 Architecture**: Harnesses NVIDIA Tensor Cores for real-time same-resolution neural rendering and texture enhancement.
- **🎨 Explicit Host Pixel Formats**: SmartRender supports AE ARGB 8/16/32-bpc and Premiere's default ARGB 8-bit path without row-pitch guessing. The neural runtime uses an RGBA8 bridge; float bypass and original-side comparisons retain host values.
- **🔍 Realtime Visual Comparison Modes**:
  - `Processed`: Full neural reconstruction output.
  - `Difference x10`: 10x amplified difference view to inspect subtle neural details.
  - `Left / Right Compare`: Real-time split-screen side-by-side comparison.
- **🚀 Ultra High Performance**:
  - Bounded OpenMP pixel conversion with a direct 8-bit processed-output path.
  - No steady-state frame-buffer allocation after the current resolution is cached.
  - Serialized Adobe frame scheduling protects the single neural context and avoids competing runtime resizes.

---

## Parameters

| Parameter | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| **Enable DLSS Neural Rendering** | Checkbox | `True` | Master bypass switch for the effect. |
| **Style** | Popup | `Default` | Rendering style (`Default`, `Natural`, `Cinema`, `Style 3`). |
| **Intensity** | Float Slider | `1.00` | Neural strength from 0.0 to 1.0, with visible output overdrive from 1.0 to 5.0. |
| **Local Tone** | Float Slider | `1.00` | Micro-contrast and local tonal enhancement (0.0 to 5.0). |
| **Local Structure** | Float Slider | `1.00` | Fine texture synthesis and edge reconstruction (0.0 to 5.0). |
| **Skin Structure** | Float Slider | `1.00` | Skin tone texture preservation weight (0.0 to 5.0). |
| **Output View** | Popup | `Processed` | Output mode (`Processed`, `Difference x10`, `Left / Right Compare`). |
| **Output Mix** | Float Slider | `1.00` | Wet/Dry mix between processed result and original frame. |

---

## 📦 Installation

Download the Windows ZIP from [Releases](https://github.com/XiangXtreme/adobe-dlss5-plugin/releases), then copy `DLSS_Neural_Video.aex`, `dlssnr_host.dll`, and `nvngx_dlssnr.dll` to:

`C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore`

Close AE and Premiere before replacing the files. See [INSTALL.md](INSTALL.md) for the complete steps.

---

## 🔨 Building from Source

### Requirements
- Windows 10 / 11 (64-bit)
- Visual Studio 2022 Community (MSVC C++20)
- CMake 3.25+
- Adobe After Effects SDK, extracted under `third_party/Adobe_AE_SDK`
- Runtime files under `third_party/runtime` when running or packaging the plugin

```powershell
# 1. Clone repository
git clone https://github.com/XiangXtreme/adobe-dlss5-plugin.git
cd adobe-dlss5-plugin

# 2. Extract the Adobe SDK so this file exists:
#    third_party/Adobe_AE_SDK/Examples/Headers/AE_Effect.h

# 3. Configure and Build
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure

```

GitHub releases are packaged by the checksum-pinned [Package Release workflow](.github/workflows/package-release.yml). See [Release Automation](docs/releasing.md) for the publishing process.

---

## 📄 License & Acknowledgements

- Licensed under the [MIT License](LICENSE).
- Based on the pioneering research and OpenFX implementation in [SAOG0721/DaVinci-Resolve-DLSS5](https://github.com/SAOG0721/DaVinci-Resolve-DLSS5).
- Adobe After Effects & Premiere Pro are registered trademarks of Adobe Systems Incorporated.
- NVIDIA, DLSS, and TensorRT are registered trademarks of NVIDIA Corporation.
