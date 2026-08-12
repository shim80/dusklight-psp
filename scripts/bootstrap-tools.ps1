param(
    [switch]$Plan,
    [switch]$DownloadOnly,
    [switch]$Install,
    [switch]$Verify,
    [switch]$Offline
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "lib\ProjectPaths.ps1")

$Modes = @($Plan, $DownloadOnly, $Install, $Verify) | Where-Object { $_ }
if ($Modes.Count -gt 1) { throw "Choisir un seul mode : -Plan, -DownloadOnly, -Install ou -Verify." }
$Mode = if ($DownloadOnly) { "download-only" } elseif ($Install) { "install" } elseif ($Verify) { "verify" } else { "plan" }

$LogDirectory = New-SafeDirectory "logs\bootstrap"
$Timestamp = [DateTime]::UtcNow.ToString("yyyyMMddTHHmmssZ")
$LogPath = Assert-ProjectPath (Join-Path $LogDirectory "$Timestamp-$Mode-powershell.log")

function Write-Log([string]$Message) {
    Write-Host $Message
    Add-Content -LiteralPath $LogPath -Value $Message -Encoding UTF8
}

function Get-Sha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-ArchivePath([hashtable]$Tool) {
    return Assert-ProjectPath (Join-Path ".cache\downloads" $Tool.archive)
}

function Test-Archive([hashtable]$Tool, [switch]$ThrowOnFailure) {
    if ($Tool.sha256 -eq "pending") {
        if ($ThrowOnFailure) { throw "$($Tool.id) : SHA-256 pending." }
        return $false
    }
    $ArchivePath = Get-ArchivePath $Tool
    if (-not (Test-Path -LiteralPath $ArchivePath -PathType Leaf)) {
        if ($ThrowOnFailure) { throw "$($Tool.id) : archive absente." }
        return $false
    }
    if ((Get-Sha256 $ArchivePath) -ne $Tool.sha256) {
        if ($ThrowOnFailure) { throw "$($Tool.id) : SHA-256 invalide." }
        return $false
    }
    return $true
}

function Test-InstallMarker([hashtable]$Tool, [string]$InstallPath) {
    $Marker = Join-Path $InstallPath ".installed-manifest"
    if (-not (Test-Path -LiteralPath $Marker -PathType Leaf)) { return $false }
    $Lines = Get-Content -LiteralPath $Marker
    return (($Lines -contains "id=$($Tool.id)") -and
            ($Lines -contains "version=$($Tool.version)") -and
            ($Lines -contains "sha256=$($Tool.sha256)"))
}

function Download-Tool([hashtable]$Tool) {
    [void](New-SafeDirectory ".cache\downloads")
    if (Test-Archive $Tool) {
        Write-Log "[OK] $($Tool.id) déjà présent et vérifié."
        return
    }
    if ($Tool.sha256 -eq "pending") { throw "$($Tool.id) : téléchargement bloqué, SHA-256 pending." }

    $ArchivePath = Get-ArchivePath $Tool
    $PartialPath = Assert-ProjectPath "$ArchivePath.part"
    if (Test-Path -LiteralPath $PartialPath -PathType Leaf) {
        if ((Get-Sha256 $PartialPath) -eq $Tool.sha256) {
            Move-Item -LiteralPath $PartialPath -Destination $ArchivePath
            Write-Log "[OK] $($Tool.id) récupéré depuis un téléchargement interrompu déjà complet."
            return
        }
        Remove-Item -LiteralPath $PartialPath -Force
    }
    if ($Offline) { throw "$($Tool.id) : archive absente en mode -Offline." }
    Write-Log "[RÉSEAU] $($Tool.download_url)"
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    try {
        Invoke-WebRequest -Uri $Tool.download_url -OutFile $PartialPath -UseBasicParsing
        if ((Get-Sha256 $PartialPath) -ne $Tool.sha256) {
            throw "$($Tool.id) : SHA-256 téléchargé invalide."
        }
        Move-Item -LiteralPath $PartialPath -Destination $ArchivePath
    } catch {
        if (Test-Path -LiteralPath $PartialPath) { Remove-Item -LiteralPath $PartialPath -Force }
        throw
    }
    Write-Log "[OK] $($Tool.id) téléchargé et vérifié."
}

function Assert-SafeZip([string]$ArchivePath, [string]$Destination) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $Destination = [System.IO.Path]::GetFullPath($Destination)
    $DestinationPrefix = $Destination.TrimEnd([char[]]@('\', '/')) + [System.IO.Path]::DirectorySeparatorChar
    $Zip = [System.IO.Compression.ZipFile]::OpenRead($ArchivePath)
    try {
        foreach ($Entry in $Zip.Entries) {
            $Name = $Entry.FullName.Replace('/', [System.IO.Path]::DirectorySeparatorChar)
            if ([System.IO.Path]::IsPathRooted($Name)) { throw "Chemin ZIP absolu refusé : $Name" }
            $Target = [System.IO.Path]::GetFullPath((Join-Path $Destination $Name))
            if (-not $Target.StartsWith($DestinationPrefix, [System.StringComparison]::OrdinalIgnoreCase) -and
                -not $Target.Equals($Destination, [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "Chemin ZIP extérieur refusé : $Name"
            }
            $UnixMode = ($Entry.ExternalAttributes -shr 16) -band 0xF000
            if ($UnixMode -eq 0xA000) { throw "Lien symbolique ZIP refusé : $Name" }
        }
    } finally {
        $Zip.Dispose()
    }
}

function Install-Tool([hashtable]$Tool) {
    [void](Test-Archive $Tool -ThrowOnFailure)
    if ($Tool.archive_format -ne "zip") { throw "Format non pris en charge nativement par PowerShell : $($Tool.archive_format)" }
    $InstallPath = Assert-ProjectPath $Tool.install_dir
    if (Test-InstallMarker $Tool $InstallPath) {
        Write-Log "[OK] $($Tool.id) déjà installé."
        return
    }
    if (Test-Path -LiteralPath $InstallPath) { throw "Destination existante non reconnue : $InstallPath" }

    [void](New-SafeDirectory ".tmp")
    $Stage = Assert-ProjectPath ".tmp\install-$($Tool.id)-$PID"
    $Extract = Join-Path $Stage "extract"
    New-Item -ItemType Directory -Path $Extract | Out-Null
    try {
        $ArchivePath = Get-ArchivePath $Tool
        Assert-SafeZip $ArchivePath $Extract
        [System.IO.Compression.ZipFile]::ExtractToDirectory($ArchivePath, $Extract)
        $MarkerTemp = Join-Path $Extract ".installed-manifest.tmp"
        @("id=$($Tool.id)", "version=$($Tool.version)", "sha256=$($Tool.sha256)") |
            Set-Content -LiteralPath $MarkerTemp -Encoding ASCII
        Move-Item -LiteralPath $MarkerTemp -Destination (Join-Path $Extract ".installed-manifest")
        [void](New-SafeDirectory ([System.IO.Path]::GetDirectoryName($Tool.install_dir)))
        Move-Item -LiteralPath $Extract -Destination $InstallPath
    } finally {
        if (Test-Path -LiteralPath $Stage) { Remove-Item -LiteralPath $Stage -Recurse -Force }
    }
    Write-Log "[OK] $($Tool.id) installé sous $($Tool.install_dir), sans exécuter ses binaires."
}

function Invoke-WslBootstrap([string]$SelectedMode) {
    $Wsl = Get-Command wsl.exe -ErrorAction SilentlyContinue
    if (-not $Wsl) { throw "WSL est requis par PSPDEV sur Windows; wsl.exe est introuvable." }
    $WslRoot = (& wsl.exe wslpath -a -- $script:ProjectRoot).Trim()
    if ($LASTEXITCODE -ne 0 -or -not $WslRoot) { throw "Impossible de convertir la racine pour WSL." }
    $Arguments = @("bash", "$WslRoot/scripts/bootstrap-tools.sh", "--$SelectedMode")
    if ($Offline) { $Arguments += "--offline" }
    Write-Log "Délégation PSPDEV à Ubuntu WSL : scripts/bootstrap-tools.sh --$SelectedMode"
    & wsl.exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Le bootstrap PSPDEV dans WSL a échoué ($LASTEXITCODE)." }
}

if (-not (Test-IsWindowsHost)) {
    throw "Ce script PowerShell cible Windows. Utiliser scripts/bootstrap-tools.sh sur macOS/Linux."
}

$Ppsspp = Get-ManifestTool "ppsspp-windows-x86_64"
$Ppsspp.id = "ppsspp-windows-x86_64"
Write-Log "Mode       : $Mode"
Write-Log "Hors ligne: $($Offline.IsPresent)"
Write-Log "Racine     : $script:ProjectRoot"
Write-Log ""

switch ($Mode) {
    "plan" {
        Write-Log "[pspdev-ubuntu-x86_64] compilation et outils PSP dans Ubuntu WSL, installation locale sous .tools/pspdev."
        Write-Log "[ppsspp-windows-x86_64] $($Ppsspp.version), $($Ppsspp.archive), installation sous $($Ppsspp.install_dir)."
        Write-Log "Plan terminé : aucun accès réseau, aucune extraction, aucun binaire exécuté."
    }
    "download-only" {
        Invoke-WslBootstrap "download-only"
        Download-Tool $Ppsspp
    }
    "install" {
        Invoke-WslBootstrap "install"
        Install-Tool $Ppsspp
    }
    "verify" {
        Invoke-WslBootstrap "verify"
        if (Test-Archive $Ppsspp) { Write-Log "[OK] cache $($Ppsspp.id)" } else { Write-Log "[ABSENT] cache $($Ppsspp.id)" }
        $InstallPath = Assert-ProjectPath $Ppsspp.install_dir
        if (Test-InstallMarker $Ppsspp $InstallPath) {
            Write-Log "[OK] installation $($Ppsspp.id)"
        } elseif (Test-Path -LiteralPath $InstallPath) {
            throw "$($Ppsspp.id) : destination présente sans marqueur d'installation valide."
        } else {
            Write-Log "[ABSENT] installation $($Ppsspp.id)"
        }
        Write-Log "Vérification terminée sans exécuter les outils installés."
    }
}
