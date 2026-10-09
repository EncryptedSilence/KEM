# Enable WSL2 mirrored networking so the Orange Pi can reach a server bound
# inside WSL on the PC's physical Ethernet IP. Run in Windows PowerShell.
# Restarts WSL (wsl --shutdown) — close WSL work first.
$ErrorActionPreference = 'Stop'
$cfg = Join-Path $env:USERPROFILE '.wslconfig'
$desired = "[wsl2]`r`nnetworkingMode=mirrored`r`n"

if (Test-Path $cfg) {
    $content = Get-Content $cfg -Raw
    if ($content -match 'networkingMode\s*=\s*mirrored') {
        Write-Host "mirrored mode already set in $cfg"
    } else {
        Write-Host "appending mirrored mode to existing $cfg (backup .bak)"
        Copy-Item $cfg "$cfg.bak" -Force
        if ($content -notmatch '\[wsl2\]') { Add-Content $cfg "`r`n[wsl2]" }
        Add-Content $cfg 'networkingMode=mirrored'
    }
} else {
    Set-Content -Path $cfg -Value $desired -Encoding utf8
    Write-Host "wrote $cfg"
}

Write-Host "shutting down WSL to apply (it restarts on next use)..."
wsl --shutdown
Write-Host "done. Verify with: wsl -d Ubuntu-26.04 -- ip -brief addr"
