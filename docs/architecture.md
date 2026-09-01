# Architecture & Technical Design

## Overview

The **Adobe DLSS 5 Neural Video Plugin** bridges After Effects ARGB 8/16/32-bpc and Premiere Pro's default ARGB 8-bit framebuffers to the neural runtime.

```
┌────────────────────────────────────────────────────────┐
│      Adobe After Effects / Premiere Pro Host           │
│  (PF_Cmd_SMART_PRE_RENDER / PF_Cmd_SMART_RENDER)       │
└───────────────────────────┬────────────────────────────┘
                            │ (AE ARGB 8/16/32-bpc, PR ARGB 8-bit)
                            ▼
┌────────────────────────────────────────────────────────┐
│        PixelPipeline + DLSSNRRenderer (C++20/OpenMP)   │
│  - Explicit Pixel Format Conversion                    │
│  - Persistent Reusable RGBA8 Frame Buffers             │
│  - Alpha-Preserving Output Composition                 │
└───────────────────────────┬────────────────────────────┘
                            │ (RGBA8 / RGBA16f)
                            ▼
┌────────────────────────────────────────────────────────┐
│             dlssnr_host.dll (D3D12 Bridge)             │
│  - Direct3D 12 Device & Command Queue Management       │
│  - NGX Feature 18 Context & Memory Barrier Sync        │
└───────────────────────────┬────────────────────────────┘
                            │ (D3D12 Texture2D)
                            ▼
┌────────────────────────────────────────────────────────┐
│            nvngx_dlssnr.dll (NVIDIA Model)             │
│  - Same-Resolution Neural Reconstruction               │
│  - Micro-Structure Synthesis & Tensor Core Denoising   │
└────────────────────────────────────────────────────────┘
```

## Key Architectural Decisions

1. **Adobe PiPL Integration**:
   - Compiles `.r` resource definitions through `PiPLtool.exe` to generate binary resources embedded in `.rc`.
   - Strictly synchronizes `out_flags = 0x2000000` and `out_flags2 = 0x8001400` across both resource and runtime layers.
   - Does not advertise pixel independence because neural reconstruction depends on neighboring pixels.

2. **Stable Frame Buffers**:
   - RGBA8 bridge buffers are cached and resized only when the frame dimensions change.
   - Float and 16-bpc host values are read directly during output composition; an exact zero-mix bypass copies the original active pixels.

3. **Multi-Frame Rendering (MFR) Safety**:
   - Runtime calls are synchronized because the bridge owns one neural feature context.
   - Temporal history is reused only for sequential frames from the same effect instance. Instance changes and out-of-order frames force a reset.

4. **Host Resource Ownership**:
   - Smart Render parameter and input-layer checkouts are always paired with checkins, including error paths.
   - `PF_Cmd_GLOBAL_SETDOWN` releases the neural feature and dynamically loaded host library.
