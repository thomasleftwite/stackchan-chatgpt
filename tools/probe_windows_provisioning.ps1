# R01: Read-only Windows provisioning feasibility probe. No network mutation.
# Run with Windows PowerShell 5.1. Never emit credentials, profile names or IDs.
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()

function Get-ErrorCodes($Failure) {
    $codes = @()
    $current = $Failure.Exception
    while ($null -ne $current) {
        $codes += ('0x{0:X8}' -f $current.HResult)
        $current = $current.InnerException
    }
    return ,$codes
}

$result = [ordered]@{
    probeVersion = 'R01'
    windowsVersion = [Environment]::OSVersion.Version.ToString()
    elevated = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    adapters = @()
    winrtAvailable = $false
    profiles = @()
    networkChanges = $false
}
try {
    $result.adapters = @([Net.NetworkInformation.NetworkInterface]::GetAllNetworkInterfaces() | ForEach-Object {
        $props = $_.GetIPProperties()
        [ordered]@{
            kind = $_.NetworkInterfaceType.ToString()
            status = $_.OperationalStatus.ToString()
            wifiDirect = ($_.Description -like '*Wi-Fi Direct*')
            hasGateway = ($props.GatewayAddresses.Count -gt 0)
        }
    })
} catch { $result.adapterErrorCodes = Get-ErrorCodes $_ }

try {
    $null = [Windows.Networking.Connectivity.NetworkInformation,Windows.Networking.Connectivity,ContentType=WindowsRuntime]
    $null = [Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager,Windows.Networking.NetworkOperators,ContentType=WindowsRuntime]
    $result.winrtAvailable = $true
    $profile = [Windows.Networking.Connectivity.NetworkInformation]::GetInternetConnectionProfile()
    if ($null -eq $profile) {
        $result.internetProfileAvailable = $false
    } else {
        $result.internetProfileAvailable = $true
        $item = [ordered]@{
            wifi = $profile.IsWlanConnectionProfile
            connectivity = $profile.GetNetworkConnectivityLevel().ToString()
        }
        try {
            $manager = [Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager]::CreateFromConnectionProfile($profile)
            $item.tetheringState = $manager.TetheringOperationalState.ToString()
            if ($item.tetheringState -eq 'On') {
                $config = $manager.GetCurrentAccessPointConfiguration()
                $item.ssidReadable = (-not [string]::IsNullOrEmpty($config.Ssid))
                $item.passphraseReadable = (-not [string]::IsNullOrEmpty($config.Passphrase))
                $item.band = $config.Band.ToString()
                if ($null -ne $config.AuthenticationKind) {
                    $item.authenticationKind = $config.AuthenticationKind.ToString()
                }
                $item.clientCount = $manager.ClientCount
                $config = $null
            }
            $manager = $null
        } catch { $item.apiErrorCodes = Get-ErrorCodes $_ }
        $result.profiles += ,$item
    }
} catch { $result.winrtErrorCodes = Get-ErrorCodes $_ }

# Success of the process means only that a sanitized report was produced.
# Inspect each error/state; never treat exit code 0 as API/connection success.
$result | ConvertTo-Json -Depth 6 -Compress
