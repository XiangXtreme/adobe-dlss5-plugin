#Requires -Version 5.1
[CmdletBinding()]
param(
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version = "1.0.1",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$rootDirectory = Split-Path -Parent $PSScriptRoot
$distributionRoot = Join-Path $rootDirectory "dist"
$packageName = "Adobe-DLSS5-Neural-Video-v$Version"
$packageDirectory = Join-Path $distributionRoot $packageName
$archivePath = Join-Path $distributionRoot "$packageName-Win64.zip"

$files = [ordered]@{
    (Join-Path $rootDirectory "build\$Configuration\DLSS_Neural_Video.aex") = "DLSS_Neural_Video.aex"
    (Join-Path $rootDirectory "third_party\runtime\dlssnr_host.dll") = "dlssnr_host.dll"
    (Join-Path $rootDirectory "third_party\runtime\nvngx_dlssnr.dll") = "nvngx_dlssnr.dll"
    (Join-Path $rootDirectory "README.md") = "README.md"
    (Join-Path $rootDirectory "README_ZH.md") = "README_ZH.md"
    (Join-Path $rootDirectory "LICENSE") = "LICENSE"
    (Join-Path $PSScriptRoot "install-package.ps1") = "Install.ps1"
    (Join-Path $PSScriptRoot "uninstall-plugin.ps1") = "Uninstall.ps1"
}

foreach ($sourcePath in $files.Keys) {
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Required package input not found: $sourcePath"
    }
}

if (Test-Path -LiteralPath $packageDirectory) {
    Remove-Item -LiteralPath $packageDirectory -Recurse -Force
}
New-Item -ItemType Directory -Path $packageDirectory -Force | Out-Null

foreach ($entry in $files.GetEnumerator()) {
    Copy-Item -LiteralPath $entry.Key -Destination (Join-Path $packageDirectory $entry.Value) -Force
}

@'
@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install.ps1"
exit /b %errorlevel%
'@ | Out-File -LiteralPath (Join-Path $packageDirectory "Install.bat") -Encoding ascii

@'
@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Uninstall.ps1"
exit /b %errorlevel%
'@ | Out-File -LiteralPath (Join-Path $packageDirectory "Uninstall.bat") -Encoding ascii

if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}
Compress-Archive -Path (Join-Path $packageDirectory "*") -DestinationPath $archivePath -CompressionLevel Optimal

Write-Host "Created release package: $archivePath" -ForegroundColor Green
