# Installation

## 中文

1. 关闭 Adobe After Effects 和 Adobe Premiere Pro。
2. 解压下载的 ZIP。
3. 将以下三个文件复制到 `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore`：
   - `DLSS_Neural_Video.aex`
   - `dlssnr_host.dll`
   - `nvngx_dlssnr.dll`
4. 重新打开 AE 或 PR，在 `效果 > DLSS Experimental > DLSS Neural Video` 中使用插件。

更新插件时，请同时替换这三个文件。若系统中存在其他位置的
`DLSS_Neural_Video.aex`，请先移走旧副本，避免 Adobe 加载错误版本。

## English

1. Close Adobe After Effects and Adobe Premiere Pro.
2. Extract the downloaded ZIP.
3. Copy these three files to `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore`:
   - `DLSS_Neural_Video.aex`
   - `dlssnr_host.dll`
   - `nvngx_dlssnr.dll`
4. Restart AE or Premiere and open `Effect > DLSS Experimental > DLSS Neural Video`.

Replace all three files together when updating. Move any other copy of
`DLSS_Neural_Video.aex` out of Adobe plug-in folders so the host loads the intended version.
