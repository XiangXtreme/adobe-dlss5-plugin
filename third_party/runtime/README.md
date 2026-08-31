# Runtime Files

Place the runtime files required for local testing and release packaging in this directory:

- `dlssnr_host.dll`
- `nvngx_dlssnr.dll`

The build produces the Adobe plugin without linking these files. They are loaded dynamically when the effect runs. The release packaging script requires both files to be present.
