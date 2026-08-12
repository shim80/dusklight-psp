$ErrorActionPreference = "Stop"

$script:CommonDirectory = [System.IO.Path]::GetFullPath($PSScriptRoot)
$script:ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $script:CommonDirectory "..\.."))
$script:ManifestPath = Join-Path $script:ProjectRoot "toolchain\manifest.lock"

function Test-IsWindowsHost {
    return [System.IO.Path]::DirectorySeparatorChar -eq '\'
}

function Get-ProjectRelativePath([Parameter(Mandatory = $true)][string]$FullPath) {
    $FullPath = [System.IO.Path]::GetFullPath($FullPath)
    $Comparison = if (Test-IsWindowsHost) {
        [System.StringComparison]::OrdinalIgnoreCase
    } else {
        [System.StringComparison]::Ordinal
    }
    if ($FullPath.Equals($script:ProjectRoot, $Comparison)) { return "." }
    $RootPrefix = $script:ProjectRoot.TrimEnd([char[]]@('\', '/')) + [System.IO.Path]::DirectorySeparatorChar
    $RootUri = [System.Uri]$RootPrefix
    $PathUri = [System.Uri]$FullPath
    $Relative = [System.Uri]::UnescapeDataString($RootUri.MakeRelativeUri($PathUri).ToString())
    return $Relative.Replace('/', [System.IO.Path]::DirectorySeparatorChar)
}

function Assert-ProjectRoot {
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        throw "Git est requis pour établir la frontière du projet."
    }
    $GitRoot = (& git -C $script:ProjectRoot rev-parse --show-toplevel 2>$null)
    if ($LASTEXITCODE -ne 0 -or -not $GitRoot) {
        throw "Aucune racine Git ne contient $script:ProjectRoot"
    }
    $GitRoot = [System.IO.Path]::GetFullPath($GitRoot.Trim())
    $Comparison = if (Test-IsWindowsHost) {
        [System.StringComparison]::OrdinalIgnoreCase
    } else {
        [System.StringComparison]::Ordinal
    }
    if (-not $GitRoot.Equals($script:ProjectRoot, $Comparison)) {
        throw "La racine calculée ($script:ProjectRoot) diffère de la racine Git ($GitRoot)."
    }
}

function Assert-ProjectPath([Parameter(Mandatory = $true)][string]$Path) {
    $FullPath = if ([System.IO.Path]::IsPathRooted($Path)) {
        [System.IO.Path]::GetFullPath($Path)
    } else {
        [System.IO.Path]::GetFullPath((Join-Path $script:ProjectRoot $Path))
    }
    $RootWithSeparator = $script:ProjectRoot.TrimEnd([char[]]@('\', '/')) + [System.IO.Path]::DirectorySeparatorChar
    $Comparison = if (Test-IsWindowsHost) {
        [System.StringComparison]::OrdinalIgnoreCase
    } else {
        [System.StringComparison]::Ordinal
    }
    if (-not $FullPath.Equals($script:ProjectRoot, $Comparison) -and
        -not $FullPath.StartsWith($RootWithSeparator, $Comparison)) {
        throw "Chemin extérieur à la racine : $Path"
    }

    $Current = $script:ProjectRoot
    $Relative = Get-ProjectRelativePath $FullPath
    foreach ($Part in ($Relative -split '[\\/]')) {
        if (-not $Part -or $Part -eq '.') { continue }
        if ($Part -eq '..') { throw "Composant '..' refusé : $Path" }
        $Current = Join-Path $Current $Part
        if (Test-Path -LiteralPath $Current) {
            $Item = Get-Item -LiteralPath $Current -Force
            if (($Item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Lien symbolique ou point de réanalyse refusé : $Current"
            }
        }
    }
    return $FullPath
}

function New-SafeDirectory([Parameter(Mandatory = $true)][string]$Path) {
    $FullPath = Assert-ProjectPath $Path
    New-Item -ItemType Directory -Force -Path $FullPath | Out-Null
    [void](Assert-ProjectPath $FullPath)
    return $FullPath
}

function Get-ManifestTool([Parameter(Mandatory = $true)][string]$Id) {
    $Text = Get-Content -LiteralPath $script:ManifestPath -Raw
    $Blocks = [regex]::Matches($Text, '(?ms)^\[\[tool\]\]\s*(.*?)(?=^\[\[tool\]\]|\z)')
    foreach ($Match in $Blocks) {
        $Block = $Match.Groups[1].Value
        $IdMatch = [regex]::Match($Block, '(?m)^id\s*=\s*"([^"]+)"\s*$')
        if ($IdMatch.Success -and $IdMatch.Groups[1].Value -eq $Id) {
            $Result = @{}
            foreach ($Line in ($Block -split "`r?`n")) {
                $Property = [regex]::Match($Line, '^([A-Za-z0-9_]+)\s*=\s*"(.*)"\s*$')
                if ($Property.Success) {
                    $Result[$Property.Groups[1].Value] = $Property.Groups[2].Value
                }
            }
            return $Result
        }
    }
    throw "Entrée de manifeste introuvable : $Id"
}

Assert-ProjectRoot
