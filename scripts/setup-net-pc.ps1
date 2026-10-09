# Static point-to-point IPv4 on the PC's Ethernet adapter + firewall rule for
# the experiment port. Run in an ELEVATED Windows PowerShell (needs admin).
param(
  [string]$AdapterName = 'Ethernet',
  [string]$IpAddress   = '10.42.0.1',
  [int]$Prefix         = 24,
  [int]$Port           = 44330
)
$ErrorActionPreference = 'Stop'

$if = Get-NetAdapter -Name $AdapterName -ErrorAction Stop
Write-Host "adapter '$AdapterName' ifIndex=$($if.InterfaceIndex) status=$($if.Status)"

# Remove any existing IPv4 on this adapter (APIPA / stale), then set static.
Get-NetIPAddress -InterfaceIndex $if.InterfaceIndex -AddressFamily IPv4 -ErrorAction SilentlyContinue |
    Where-Object { $_.IPAddress -ne $IpAddress } |
    ForEach-Object { Remove-NetIPAddress -InterfaceIndex $if.InterfaceIndex -IPAddress $_.IPAddress -Confirm:$false -ErrorAction SilentlyContinue }

if (-not (Get-NetIPAddress -InterfaceIndex $if.InterfaceIndex -IPAddress $IpAddress -ErrorAction SilentlyContinue)) {
    New-NetIPAddress -InterfaceIndex $if.InterfaceIndex -IPAddress $IpAddress -PrefixLength $Prefix | Out-Null
    Write-Host "set $IpAddress/$Prefix on $AdapterName"
} else {
    Write-Host "$IpAddress already present on $AdapterName"
}

# Firewall: allow inbound TCP on the experiment port.
$ruleName = "PQC-experiment-$Port"
if (-not (Get-NetFirewallRule -DisplayName $ruleName -ErrorAction SilentlyContinue)) {
    New-NetFirewallRule -DisplayName $ruleName -Direction Inbound -Action Allow `
        -Protocol TCP -LocalPort $Port | Out-Null
    Write-Host "added firewall rule $ruleName"
} else {
    Write-Host "firewall rule $ruleName already present"
}

Get-NetIPAddress -InterfaceIndex $if.InterfaceIndex -AddressFamily IPv4 |
    Select-Object IPAddress, PrefixLength | Format-Table -AutoSize | Out-String | Write-Host
