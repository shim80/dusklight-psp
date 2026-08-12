$ErrorActionPreference = "Stop"

$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
Set-Location -LiteralPath $ProjectRoot

if (-not (Get-Command codex -ErrorAction SilentlyContinue)) {
    throw "La commande 'codex' est introuvable."
}

& codex `
    --sandbox workspace-write `
    --ask-for-approval on-request `
    --config sandbox_workspace_write.network_access=true

exit $LASTEXITCODE
