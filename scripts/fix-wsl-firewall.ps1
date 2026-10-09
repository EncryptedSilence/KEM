# Allow inbound TCP to WSL under mirrored networking (Hyper-V firewall gates it;
# a normal New-NetFirewallRule does not cover the WSL vNIC). Run ELEVATED.
param([int]$Port = 44330)
$ErrorActionPreference = 'Continue'
$vmId = '{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}'   # WSL VM creator id
try {
  if (Get-NetFirewallHyperVRule -Name "PQC-$Port" -ErrorAction SilentlyContinue) {
    Remove-NetFirewallHyperVRule -Name "PQC-$Port" -ErrorAction SilentlyContinue
  }
  New-NetFirewallHyperVRule -Name "PQC-$Port" -DisplayName "PQC experiment $Port" `
    -Direction Inbound -VMCreatorId $vmId -Protocol TCP -LocalPorts $Port -Action Allow -ErrorAction Stop | Out-Null
  "OK: added Hyper-V inbound rule for TCP $Port"
} catch {
  "targeted Hyper-V rule failed: $($_.Exception.Message)"
  try {
    Set-NetFirewallHyperVVMSetting -Name $vmId -DefaultInboundAction Allow -ErrorAction Stop
    "OK: set WSL DefaultInboundAction=Allow (broad fallback; revert with -DefaultInboundAction Block)"
  } catch {
    "FALLBACK FAILED: $($_.Exception.Message)"
  }
}