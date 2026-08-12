param([switch]$Plan, [switch]$Execute)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "lib\ProjectPaths.ps1")

if ($Plan -and $Execute) { throw "Choisir -Plan ou -Execute." }
$Mode = if ($Execute) { "execute" } else { "plan" }
$Targets = @(
    ".tmp",
    "build\host",
    "build\psp",
    "logs\audit",
    "logs\bootstrap",
    "logs\ppsspp",
    "artifacts",
    ".test-data\ppsspp"
)

$Items = [System.Collections.Generic.List[System.IO.FileSystemInfo]]::new()
foreach ($Relative in $Targets) {
    $Base = Assert-ProjectPath $Relative
    if (-not (Test-Path -LiteralPath $Base -PathType Container)) { continue }
    $BaseItem = Get-Item -LiteralPath $Base -Force
    if (($BaseItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Cible de nettoyage symbolique refusée : $Relative"
    }
    foreach ($Item in Get-ChildItem -LiteralPath $Base -Force) {
        if ($Item.Name -eq ".gitkeep") { continue }
        [void](Assert-ProjectPath $Item.FullName)
        if (($Item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Lien symbolique ou point de réanalyse détecté; nettoyage annulé : $($Item.FullName)"
        }
        $Items.Add($Item)
    }
}

if ($Items.Count -eq 0) {
    Write-Host "Aucun fichier généré à nettoyer."
    exit 0
}

Write-Host "Mode : $Mode"
foreach ($Item in $Items) {
    Write-Host "  $(Get-ProjectRelativePath $Item.FullName)"
}

if ($Mode -eq "plan") {
    Write-Host "Plan uniquement; relancer avec -Execute après examen."
    exit 0
}

foreach ($Item in $Items) {
    Remove-Item -LiteralPath $Item.FullName -Recurse -Force
}
Write-Host "Nettoyage terminé sur la liste fermée ci-dessus."
