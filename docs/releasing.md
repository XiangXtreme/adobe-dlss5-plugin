# Release Automation

The `Package Release` workflow creates the Windows archive and publishes it to GitHub Releases.

## Inputs

Release packaging uses a tested binary component archive containing:

- `DLSS_Neural_Video.aex`
- `dlssnr_host.dll`
- `nvngx_dlssnr.dll`

The component archive is stored in the `runtime-v1.0.3` prerelease. Its filename and SHA-256 digest are pinned in `.github/workflows/package-release.yml`. The workflow stops before packaging if the digest or required file list does not match.

## Publish a Version

Run the workflow manually:

```powershell
gh workflow run package-release.yml -f version=1.0.3
```

The workflow validates the version, downloads and verifies the component archive, packages the three runtime files with `INSTALL.md`, checks the ZIP contents, and creates or updates the matching GitHub Release. Enable its release gate only after the exact AEX has passed host validation in both After Effects and Premiere Pro.

## Update Binary Components

Compile and host-test the plugin, create a new component archive with the three required files, and publish it under a new runtime prerelease tag. Update these workflow values in the same commit:

- `RUNTIME_RELEASE_TAG`
- `RUNTIME_ARCHIVE_NAME`
- `RUNTIME_ARCHIVE_SHA256`

Run the local Release build and pixel pipeline tests before publishing the updated workflow.
