$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "lib\ProjectPaths.ps1")

$env:DUSKLIGHT_PSP_ROOT = $script:ProjectRoot
$env:PSPDEV = Join-Path $script:ProjectRoot ".tools\pspdev"
$env:PSP = Join-Path $env:PSPDEV "psp"
$env:PSPSDK = Join-Path $env:PSP "sdk"
$env:PPSSPP_STATE_ROOT = Join-Path $script:ProjectRoot ".test-data\ppsspp"
$env:PPSSPP_HOME = Join-Path $env:PPSSPP_STATE_ROOT "home"
$env:XDG_CONFIG_HOME = Join-Path $env:PPSSPP_HOME ".config"
$env:XDG_CACHE_HOME = Join-Path $env:PPSSPP_STATE_ROOT "xdg-cache"
$env:TEMP = Join-Path $script:ProjectRoot ".tmp"
$env:TMP = $env:TEMP

$LocalBin = Join-Path $script:ProjectRoot ".tools\bin"
$PspDevBin = Join-Path $env:PSPDEV "bin"
$PathParts = @($PspDevBin, $LocalBin) + ($env:Path -split [System.IO.Path]::PathSeparator)
$env:Path = ($PathParts | Select-Object -Unique) -join [System.IO.Path]::PathSeparator

$PkgConfig = Join-Path $env:PSP "lib\pkgconfig"
if (Test-Path -LiteralPath $PkgConfig) {
    $env:PKG_CONFIG_PATH = if ($env:PKG_CONFIG_PATH) {
        "$PkgConfig$([System.IO.Path]::PathSeparator)$env:PKG_CONFIG_PATH"
    } else {
        $PkgConfig
    }
}

Write-Host "Environnement local défini pour ce processus : $script:ProjectRoot"
