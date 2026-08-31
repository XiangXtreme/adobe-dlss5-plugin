#Requires -Version 5.1
[CmdletBinding(SupportsShouldProcess)]
param()

$ErrorActionPreference = "Stop"
$rootDirectory = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\')
$rootPrefix = $rootDirectory + '\'

foreach ($directoryName in @("build", "dist")) {
    $target = [IO.Path]::GetFullPath((Join-Path $rootDirectory $directoryName))
    if (-not $target.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean path outside the project: $target"
    }
    if ((Test-Path -LiteralPath $target) -and $PSCmdlet.ShouldProcess($target, "Remove generated directory")) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}
