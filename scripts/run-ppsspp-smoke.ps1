param(
    [switch]$Plan,
    [switch]$Run,
    [ValidateRange(1, 600)][int]$TimeoutSeconds = 30
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "env.ps1")

if ($Plan -and $Run) { throw "Choisir -Plan ou -Run." }
$Mode = if ($Run) { "run" } else { "plan" }
$EbootSource = Assert-ProjectPath "build\psp\smoke\EBOOT.PBP"
$StateRoot = Assert-ProjectPath ".test-data\ppsspp"
$Runtime = Assert-ProjectPath ".test-data\ppsspp\runtime"
$GameDirectory = Assert-ProjectPath ".test-data\ppsspp\runtime\memstick\PSP\GAME\DUSKLIGHT_SMOKE"
$EbootDestination = Join-Path $GameDirectory "EBOOT.PBP"
$Marker = Join-Path $GameDirectory "SMOKE.OK"
$SourceRuntime = Assert-ProjectPath ".tools\ppsspp"

Write-Host "Mode            : $Mode"
Write-Host "PPSSPP source   : .tools\ppsspp"
Write-Host "EBOOT source    : build\psp\smoke\EBOOT.PBP"
Write-Host "Profil isolé    : .test-data\ppsspp"
Write-Host "Jeton de succès : DUSKLIGHT_PSP_SMOKE_OK"
Write-Host "Timeout         : $TimeoutSeconds secondes"
if ($Mode -eq "plan") {
    Write-Host "Plan terminé : PPSSPP n'a pas été exécuté."
    exit 0
}

if (-not (Test-Path -LiteralPath $EbootSource -PathType Leaf)) { throw "EBOOT absent; compiler d'abord test\smoke dans WSL." }
if (-not (Test-Path -LiteralPath $SourceRuntime -PathType Container)) { throw "PPSSPP absent sous .tools\ppsspp." }
[void](New-SafeDirectory ".test-data\ppsspp\runtime")
foreach ($Item in Get-ChildItem -LiteralPath $SourceRuntime -Force) {
    if (($Item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Lien ou point de réanalyse refusé dans PPSSPP : $($Item.FullName)"
    }
    Copy-Item -LiteralPath $Item.FullName -Destination $Runtime -Recurse -Force
}
if (Test-Path -LiteralPath (Join-Path $Runtime "installed.txt")) {
    throw "installed.txt détecté : le mode portable PPSSPP ne peut pas être garanti."
}

$Ppsspp = @(
    (Join-Path $Runtime "PPSSPPWindows64.exe"),
    (Join-Path $Runtime "PPSSPPWindows.exe")
) | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (-not $Ppsspp) { throw "Exécutable PPSSPP portable introuvable." }

[void](New-SafeDirectory ".test-data\ppsspp\runtime\memstick\PSP\GAME\DUSKLIGHT_SMOKE")
[void](New-SafeDirectory ".test-data\ppsspp\home")
[void](New-SafeDirectory ".test-data\ppsspp\appdata")
[void](New-SafeDirectory "logs\ppsspp")
[void](New-SafeDirectory ".tmp")
Copy-Item -LiteralPath $EbootSource -Destination $EbootDestination -Force
if (Test-Path -LiteralPath $Marker) { Remove-Item -LiteralPath $Marker -Force }

$RunId = [DateTime]::UtcNow.ToString("yyyyMMddTHHmmssZ")
$Stdout = Assert-ProjectPath "logs\ppsspp\smoke-$RunId.stdout.log"
$Stderr = Assert-ProjectPath "logs\ppsspp\smoke-$RunId.stderr.log"
$ResultReport = Assert-ProjectPath "logs\ppsspp\smoke-$RunId.result.md"
$OldEnvironment = @{
    HOME = $env:HOME; USERPROFILE = $env:USERPROFILE; APPDATA = $env:APPDATA;
    LOCALAPPDATA = $env:LOCALAPPDATA; TEMP = $env:TEMP; TMP = $env:TMP
}
$Process = $null
try {
    $env:HOME = Join-Path $StateRoot "home"
    $env:USERPROFILE = $env:HOME
    $env:APPDATA = Join-Path $StateRoot "appdata"
    $env:LOCALAPPDATA = Join-Path $StateRoot "localappdata"
    $env:TEMP = Assert-ProjectPath ".tmp"
    $env:TMP = $env:TEMP
    $Process = Start-Process -FilePath $Ppsspp -WorkingDirectory $Runtime `
        -ArgumentList @("--windowed", "--escape-exit", "--pause-menu-exit", "`"$EbootDestination`"") `
        -RedirectStandardOutput $Stdout -RedirectStandardError $Stderr -PassThru
    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $Success = $false
    while ([DateTime]::UtcNow -lt $Deadline) {
        if ((Test-Path -LiteralPath $Marker) -and
            ((Get-Content -LiteralPath $Marker -Raw).Trim() -eq "DUSKLIGHT_PSP_SMOKE_OK")) {
            $Success = $true
            break
        }
        if ($Process.HasExited) { break }
        Start-Sleep -Seconds 1
        $Process.Refresh()
    }
} finally {
    if ($Process -and -not $Process.HasExited) {
        $Process.CloseMainWindow() | Out-Null
        if (-not $Process.WaitForExit(3000)) { $Process.Kill() }
    }
    foreach ($Key in $OldEnvironment.Keys) {
        [System.Environment]::SetEnvironmentVariable($Key, $OldEnvironment[$Key], "Process")
    }
}

$Result = if ($Success) { "SUCCES" } else { "ECHEC" }
$Detail = if ($Success) {
    "Le marqueur déterministe a été écrit dans le Memory Stick portable isolé."
} else {
    "PPSSPP s'est arrêté ou a atteint le timeout sans écrire le marqueur."
}
$ReportTemp = Assert-ProjectPath ".tmp\ppsspp-smoke-report-$RunId.tmp"
@(
    "# Résultat du smoke test PPSSPP", "", "- Date UTC : ``$RunId``",
    "- Résultat : **$Result**", "- PPSSPP : ``.test-data\ppsspp\runtime``",
    "- EBOOT : ``build\psp\smoke\EBOOT.PBP``", "- Profil : ``.test-data\ppsspp``",
    "- Journal : ``logs\ppsspp\smoke-$RunId.stdout.log``", "", $Detail, "",
    "Ce résultat est fonctionnel dans PPSSPP et ne valide ni le timing, ni les caches, ni le Media Engine sur matériel réel."
) | Set-Content -LiteralPath $ReportTemp -Encoding UTF8
Move-Item -LiteralPath $ReportTemp -Destination $ResultReport -Force

Write-Host "Résultat : $Result"
Write-Host "Rapport  : logs\ppsspp\smoke-$RunId.result.md"
if (-not $Success) { exit 1 }
