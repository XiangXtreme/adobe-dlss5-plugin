#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$Destination = (Join-Path ${env:ProgramFiles} "Adobe\Common\Plug-ins\7.0\MediaCore")
)

$ErrorActionPreference = "Stop"

function Test-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

$standardDestination = [IO.Path]::GetFullPath(
    (Join-Path ${env:ProgramFiles} "Adobe\Common\Plug-ins\7.0\MediaCore")
)
$legacyDestination = [IO.Path]::GetFullPath(
    (Join-Path ${env:CommonProgramFiles} "Adobe\Plug-ins\7.0\MediaCore")
)
$programFilesRoots = @(${env:ProgramFiles}, ${env:CommonProgramFiles}) |
    Where-Object { $_ } |
    ForEach-Object { [IO.Path]::GetFullPath($_).TrimEnd('\') + '\' }
$destinationPath = [IO.Path]::GetFullPath($Destination)
$requiresElevation = $programFilesRoots | Where-Object {
    $destinationPath.StartsWith($_, [StringComparison]::OrdinalIgnoreCase)
}

if ($requiresElevation -and -not (Test-IsAdministrator)) {
    $powerShellPath = (Get-Process -Id $PID).Path
    $arguments = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", "`"$PSCommandPath`"",
        "-Destination", "`"$Destination`""
    )
    $process = Start-Process -FilePath $powerShellPath -Verb RunAs -ArgumentList $arguments -Wait -PassThru
    exit $process.ExitCode
}

$fileNames = @("DLSS_Neural_Video.aex", "dlssnr_host.dll", "nvngx_dlssnr.dll")
foreach ($fileName in $fileNames) {
    $sourcePath = Join-Path $PSScriptRoot $fileName
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Required file not found: $sourcePath"
    }
}

New-Item -ItemType Directory -Path $destinationPath -Force | Out-Null
foreach ($fileName in $fileNames) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $fileName) -Destination $destinationPath -Force
}

if ($destinationPath -eq $standardDestination -and $legacyDestination -ne $standardDestination) {
    foreach ($fileName in $fileNames) {
        $legacyPath = Join-Path $legacyDestination $fileName
        if (Test-Path -LiteralPath $legacyPath -PathType Leaf) {
            Remove-Item -LiteralPath $legacyPath -Force
        }
    }
}

Write-Host "Installed DLSS Neural Video to $destinationPath" -ForegroundColor Green
