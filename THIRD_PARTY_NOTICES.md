# Third-Party Notices and Acknowledgements

This project incorporates, links with, or builds upon concepts from the following third-party software and specifications:

---

### 1. Adobe After Effects & Premiere Pro SDK
- **Provider**: Adobe Systems Incorporated
- **Usage**: Plugin headers and SmartRender API suites expected locally under `third_party/Adobe_AE_SDK` for builds.
- **License**: Proprietary Adobe SDK License Agreement.

---

### 2. NVIDIA NGX / DLSS Technology
- **Provider**: NVIDIA Corporation
- **Usage**: Direct3D 12 Feature 18 Neural Rendering pipeline (`nvsdk_ngx.h`, `nvngx_dlssnr.dll`).
- **Notice**: DLSS, TensorRT, and NVIDIA are registered trademarks of NVIDIA Corporation. The binary model `nvngx_dlssnr.dll` is provided separately for research and non-commercial video enhancement experimentation.

---

### 3. DaVinci Resolve DLSS 5 OpenFX Filter
- **Repository**: [SAOG0721/DaVinci-Resolve-DLSS5](https://github.com/SAOG0721/DaVinci-Resolve-DLSS5)
- **Author**: SAOG0721 & Community Contributors
- **License**: MIT License
- **Usage**: Reference implementation for same-resolution Feature 18 parameters, project ID registration, and zero-guidance neural reconstruction.

---

### 4. DLSS5Tool & Magpie
- **Reference**: Magpie & DLSS5Tool community Direct3D 12 feature host bridging techniques.
