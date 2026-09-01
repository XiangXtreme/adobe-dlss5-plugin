# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Fixed
- Prevented concurrent Adobe frame requests from competing for the single neural runtime context.

### Changed
- Limited CPU pixel conversion and composition to four workers, with single-threaded processing for small previews.
- Added a direct 8-bit processed-output path for the common After Effects and Premiere Pro workflow.
- Disabled AEX and NVIDIA runtime file logging in Release builds.

## [1.0.2] - 2026-09-01

### Fixed
- Restored Premiere Pro's default ARGB 8-bit render path to prevent corrupted output.
- Made `Intensity` values above 1.0 visibly amplify the neural difference while preserving the original 1.0 behavior.

### Changed
- Windows archives contain the AEX, both runtime DLLs, and the manual installation guide.

## [1.0.1] - 2026-09-01

### Changed
- Expanded `Intensity`, `Local Tone`, `Local Structure`, and `Skin Structure` to a 0.0 to 5.0 range.

## [1.0.0] - 2026-09-01

### Added
- **Native Adobe Dual-Host Support**: Single unified `.aex` plugin compatible with Adobe After Effects (2022-2026) and Adobe Premiere Pro (2022-2026).
- **DLSS 5 Neural Rendering Engine (Feature 18)**: Real-time same-resolution AI neural reconstruction, texture refinement, and edge denoising.
- **SmartRender Pipeline**: Supports AE ARGB 8/16/32-bpc and Premiere BGRA 8-bit/32f with explicit format dispatch.
- **Interactive UI Parameters**:
  - `Enable DLSS Neural Rendering` (Global Bypass toggle)
  - `Style` (Default / Natural / Cinema / Style 3)
  - `Intensity`, `Local Tone`, `Local Structure`, and `Skin Structure` (0.0 to 2.0 neural reconstruction, tone, detail, and skin weights)
  - `Output View` (`Processed`, `Difference x10` for inspection, `Left / Right Compare` split view)
  - `Output Mix` (Wet/Dry blending)
- **High-Performance Optimizations**:
  - OpenMP parallel SIMD pixel conversion.
  - Persistent reusable RGBA8 bridge buffers.
  - Multi-Frame Rendering (MFR) safety with synchronized runtime access.
  - Per-instance temporal history reset on timeline jumps and out-of-order frames.
- **Distribution**:
  - Windows archive with the plugin and required runtime components.
