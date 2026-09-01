# Changelog

All notable changes to this project will be documented in this file.

## [1.0.0] - 2026-09-01

### Added
- **Native Adobe Dual-Host Support**: Single unified `.aex` plugin compatible with Adobe After Effects (2022-2026) and Adobe Premiere Pro (2022-2026).
- **DLSS 5 Neural Rendering Engine (Feature 18)**: Real-time same-resolution AI neural reconstruction, texture refinement, and edge denoising.
- **SmartRender Pipeline**: Supports AE ARGB 8/16/32-bpc and Premiere BGRA 8-bit/32f with explicit format dispatch.
- **Interactive UI Parameters**:
  - `Enable DLSS Neural Rendering` (Global Bypass toggle)
  - `Style` (Default / Natural / Cinema / Style 3)
  - `Intensity`, `Local Tone`, `Local Structure`, and `Skin Structure` (0.0 to 2.5 neural reconstruction, tone, detail, and skin weights)
  - `Output View` (`Processed`, `Difference x10` for inspection, `Left / Right Compare` split view)
  - `Output Mix` (Wet/Dry blending)
- **High-Performance Optimizations**:
  - OpenMP parallel SIMD pixel conversion.
  - Persistent reusable RGBA8 bridge buffers.
  - Multi-Frame Rendering (MFR) safety with synchronized runtime access.
  - Per-instance temporal history reset on timeline jumps and out-of-order frames.
- **Packaging & Tooling**:
  - Automated 1-click Windows installer (`Install.bat` / `Install.ps1`).
  - Automated distribution release packager.
