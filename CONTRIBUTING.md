# Contributing to Adobe DLSS 5 Neural Video Plugin

We welcome contributions, issues, and feature requests!

## Development Environment Setup

1. **System Requirements**:
   - Windows 10/11 x64
   - Visual Studio 2022 Community (with Desktop development with C++ & MSVC v143+)
   - CMake 3.25+
   - NVIDIA GPU (RTX 20/30/40/50 Series with updated Game Ready / Studio drivers)
   - Adobe After Effects / Premiere Pro (2022 ~ 2026)

2. **SDK Setup**:
   - Ensure `third_party/Adobe_AE_SDK` contains the unpacked After Effects SDK headers and `PiPLtool.exe`.

3. **Building from Source**:
   ```powershell
   cmake -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release
   ctest --test-dir build -C Release --output-on-failure
   ```

4. **Testing in After Effects**:
   ```powershell
   pwsh -File .\scripts\install-plugin.ps1
   ```

## Pull Request Guidelines

- Follow Modern C++20 conventions.
- Keep host pixel-format dispatch explicit; do not infer channel depth from row pitch.
- Preserve input alpha exactly and verify 8/16/32-bpc conversion tests before submitting.
- Keep neural runtime state changes inside the synchronized render section.
