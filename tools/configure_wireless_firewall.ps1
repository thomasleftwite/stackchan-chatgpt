[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$LocalAddress,
    [Parameter(Mandatory=$true)][string]$NodeExecutable,
    [switch]$Apply
)
$ErrorActionPreference = 'Stop'
# Read-only by default. The caller must explicitly authorize -Apply.
$address = Get-NetIPAddress -AddressFamily IPv4 -IPAddress $LocalAddress
if (@($address).Count -ne 1 -or $address.PrefixLength -lt 16 -or $address.PrefixLength -gt 30) {
    throw 'Select one active hotspot IPv4 interface with a bounded subnet.'
}
$adapter = Get-NetAdapter -IncludeHidden | Where-Object { $_.ifIndex -eq $address.InterfaceIndex }
if (-not $adapter -or $adapter.InterfaceDescription -notlike '*Wi-Fi Direct*') {
    throw 'The selected address is not on the Windows mobile hotspot virtual adapter. Do not allow the upstream shared Wi-Fi.'
}
$nodePath = [System.IO.Path]::GetFullPath($NodeExecutable)
$remoteSubnet = "$LocalAddress/$($address.PrefixLength)"
$plan = [ordered]@{Rule='StackchanHost-Wireless-v1';Program=$nodePath;Interface=$address.InterfaceAlias;
    LocalAddress=$LocalAddress;RemoteAddress=$remoteSubnet;Protocol='TCP';LocalPort=48443;Applied=$false}
if (-not $Apply) { $plan | ConvertTo-Json; return }
if (-not (Test-Path -LiteralPath $nodePath -PathType Leaf)) { throw 'Start the new host once to install its verified runtime first.' }
$identity = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $identity.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Administrator rights required for this one-time firewall rule.' }
$blocked = @(Get-NetFirewallApplicationFilter | Where-Object { $_.Program -eq $nodePath } |
    ForEach-Object { Get-NetFirewallRule -AssociatedNetFirewallApplicationFilter $_ } |
    Where-Object { $_.Enabled -eq 'True' -and $_.Direction -eq 'Inbound' -and $_.Action -eq 'Block' })
$backup = Join-Path $PSScriptRoot '../.pio/device-check/wireless-firewall-before.json'
$blocked | Select-Object Name,Enabled,Direction,Action,Profile | ConvertTo-Json | Set-Content -LiteralPath $backup -Encoding UTF8
$prior = Get-NetFirewallRule -Name $plan.Rule -ErrorAction SilentlyContinue
if ($prior -and $prior.Group -ne 'StackchanHost Wireless') { throw 'Unrelated rule name collision.' }
if ($prior) { Remove-NetFirewallRule -Name $plan.Rule }
New-NetFirewallRule -Name $plan.Rule -DisplayName 'StackchanHost wireless - hotspot only' -Group 'StackchanHost Wireless' `
    -Direction Inbound -Action Allow -Enabled True -Profile Any -Program $nodePath -Protocol TCP -LocalPort 48443 `
    -LocalAddress $LocalAddress -RemoteAddress $remoteSubnet -InterfaceAlias $address.InterfaceAlias | Out-Null
foreach ($rule in $blocked) { Disable-NetFirewallRule -Name $rule.Name }
$plan.Applied=$true; $plan | ConvertTo-Json
