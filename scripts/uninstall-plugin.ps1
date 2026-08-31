#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$Destination = (Join-Path ${env:CommonProgramFiles} "Adobe\Plug-ins\7.0\MediaCore")
)

$ErrorActionPreference = "Stop"

function Test-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

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

foreach ($fileName in @("DLSS_Neural_Video.aex", "dlssnr_host.dll", "nvngx_dlssnr.dll")) {
    $path = Join-Path $destinationPath $fileName
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        Remove-Item -LiteralPath $path -Force
    }
}

Write-Host "Uninstalled DLSS Neural Video from $destinationPath" -ForegroundColor Green
