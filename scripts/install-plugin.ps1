#Requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [string]$Destination = (Join-Path ${env:ProgramFiles} "Adobe\Common\Plug-ins\7.0\MediaCore")
)

$ErrorActionPreference = "Stop"

function Test-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Restart-Elevated {
    $powerShellPath = (Get-Process -Id $PID).Path
    $arguments = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", "`"$PSCommandPath`"",
        "-Configuration", "`"$Configuration`"",
        "-Destination", "`"$Destination`""
    )
    $process = Start-Process -FilePath $powerShellPath -Verb RunAs -ArgumentList $arguments -Wait -PassThru
    exit $process.ExitCode
}

$rootDirectory = Split-Path -Parent $PSScriptRoot
$sourceFiles = @(
    (Join-Path $rootDirectory "build\$Configuration\DLSS_Neural_Video.aex"),
    (Join-Path $rootDirectory "third_party\runtime\dlssnr_host.dll"),
    (Join-Path $rootDirectory "third_party\runtime\nvngx_dlssnr.dll")
)

foreach ($sourceFile in $sourceFiles) {
    if (-not (Test-Path -LiteralPath $sourceFile -PathType Leaf)) {
        throw "Required file not found: $sourceFile"
    }
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
    Restart-Elevated
}

New-Item -ItemType Directory -Path $destinationPath -Force | Out-Null
foreach ($sourceFile in $sourceFiles) {
    Copy-Item -LiteralPath $sourceFile -Destination $destinationPath -Force
}

if ($destinationPath -eq $standardDestination -and $legacyDestination -ne $standardDestination) {
    foreach ($sourceFile in $sourceFiles) {
        $legacyPath = Join-Path $legacyDestination (Split-Path -Leaf $sourceFile)
        if (Test-Path -LiteralPath $legacyPath -PathType Leaf) {
            Remove-Item -LiteralPath $legacyPath -Force
        }
    }
}

Write-Host "Installed DLSS Neural Video to $destinationPath" -ForegroundColor Green
