param([switch]$Strict)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "env.ps1")

$MissingHost = $false
$MissingPsp = $false

function Test-Command([string]$Label, [string]$Name) {
    $Command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($Command) {
        Write-Host ("[OK] {0,-22} {1}" -f $Label, $Command.Source)
    } else {
        Write-Host ("[MANQUANT] {0,-17} {1}" -f $Label, $Name)
        $script:MissingHost = $true
    }
}

function Test-ProjectFile([string]$Label, [string]$Path) {
    $FullPath = Assert-ProjectPath $Path
    if (Test-Path -LiteralPath $FullPath -PathType Leaf) {
        Write-Host ("[OK] {0,-22} {1}" -f $Label, (Get-ProjectRelativePath $FullPath))
    } else {
        Write-Host ("[MANQUANT] {0,-17} {1}" -f $Label, (Get-ProjectRelativePath $FullPath))
        $script:MissingPsp = $true
    }
}

Write-Host "Racine Git       : $script:ProjectRoot"
Write-Host "PSPDEV           : $env:PSPDEV"
Write-Host "État PPSSPP      : $env:PPSSPP_STATE_ROOT"
Write-Host ""

Test-Command "Git" "git"
Test-Command "CMake" "cmake"
Write-Host ("[OK] {0,-22} {1}" -f "PowerShell", $PSVersionTable.PSVersion.ToString())
Test-Command "Archive tar" "tar"

if (Test-Path -LiteralPath $script:ManifestPath -PathType Leaf) {
    Write-Host ("[OK] {0,-22} {1}" -f "Manifeste", "toolchain\manifest.lock")
} else {
    Write-Host ("[MANQUANT] {0,-17} {1}" -f "Manifeste", "toolchain\manifest.lock")
    $MissingHost = $true
}

Test-ProjectFile "PSP GCC" (Join-Path $env:PSPDEV "bin\psp-gcc")
Test-ProjectFile "PSP Config" (Join-Path $env:PSPDEV "bin\psp-config")
Test-ProjectFile "PSP objdump" (Join-Path $env:PSPDEV "bin\psp-objdump")
Test-ProjectFile "Créateur PBP" (Join-Path $env:PSPDEV "bin\pack-pbp")

$Candidates = @(
    (Join-Path $script:ProjectRoot ".tools\ppsspp\PPSSPPWindows64.exe"),
    (Join-Path $script:ProjectRoot ".tools\ppsspp\PPSSPPWindows.exe")
)
$Ppsspp = $Candidates | Where-Object { Test-Path -LiteralPath (Assert-ProjectPath $_) -PathType Leaf } | Select-Object -First 1
if ($Ppsspp) {
    Write-Host ("[OK] {0,-22} {1}" -f "PPSSPP", (Get-ProjectRelativePath $Ppsspp))
} else {
    Write-Host ("[MANQUANT] {0,-17} {1}" -f "PPSSPP", ".tools\ppsspp")
    $MissingPsp = $true
}

Write-Host ""
Write-Host "Hôte prêt : $(if (-not $MissingHost) { 'oui' } else { 'non' })"
Write-Host "PSP/PPSSPP installé : $(if (-not $MissingPsp) { 'oui' } else { 'non' })"
Write-Host "Ce contrôle n'a exécuté aucun binaire situé sous .tools\."

if ($MissingHost) { exit 2 }
if ($Strict -and $MissingPsp) { exit 1 }
exit 0
