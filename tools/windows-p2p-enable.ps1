#requires -RunAsAdministrator
param([switch]$WhatIfOnly)

$ErrorActionPreference = "Stop"
$ClassRoot = "HKLM:\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}"
$BackupPath = Join-Path $PSScriptRoot "cmp90hx-p2p-registry-backup.json"

$Values = [ordered]@{
    ForceP2P                    = 0x111
    CLForceP2P                  = 0x111
    RMForceP2PType              = 1
    RMPcieP2PType               = 0
    RMForceStaticBar1           = 1
    PeerMappingOverride         = 1
    RMDisableFeatureDisablement = 1
}

function Get-Cmp90ClassKeys {
    Get-ChildItem $ClassRoot -ErrorAction Stop | Where-Object {
        try {
            $p = Get-ItemProperty $_.PSPath -ErrorAction Stop
            $id = [string]$p.MatchingDeviceId
            $desc = [string]$p.DriverDesc
            ($id -match 'VEN_10DE&DEV_220D') -or ($desc -match 'CMP\s*90HX')
        } catch { $false }
    }
}

$keys = @(Get-Cmp90ClassKeys)
if ($keys.Count -eq 0) {
    throw "No CMP 90HX (10DE:220D) NVIDIA display-class registry keys found."
}

Write-Host "CMP 90HX registry keys:"
$keys | ForEach-Object { Write-Host "  $($_.Name)" }

$backup = @()
foreach ($key in $keys) {
    $item = Get-ItemProperty $key.PSPath
    $saved = [ordered]@{ Key = $key.Name; Values = [ordered]@{} }
    foreach ($name in $Values.Keys) {
        $exists = $null -ne $item.PSObject.Properties[$name]
        $saved.Values[$name] = [ordered]@{
            Exists = $exists
            Value  = if ($exists) { [uint32]$item.$name } else { $null }
        }
    }
    $backup += $saved
}

if (-not $WhatIfOnly) {
    $backup | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 $BackupPath
    Write-Host "Backup written to: $BackupPath"
}

foreach ($key in $keys) {
    Write-Host ""
    Write-Host "Target: $($key.Name)"
    foreach ($pair in $Values.GetEnumerator()) {
        Write-Host ("  {0} = 0x{1:X}" -f $pair.Key, [uint32]$pair.Value)
        if (-not $WhatIfOnly) {
            New-ItemProperty -Path $key.PSPath -Name $pair.Key -PropertyType DWord -Value ([uint32]$pair.Value) -Force | Out-Null
        }
    }
}

if ($WhatIfOnly) {
    Write-Host ""
    Write-Host "WhatIfOnly: registry was not changed."
    exit 0
}

Write-Host ""
Write-Host "Experimental CMP90HX P2P registry values applied."
Write-Host "Perform a full Windows restart through the UEFI unlock path:"
Write-Host "  1 -> 2 -> 3 -> 4 -> C -> G -> C -> W"
Write-Host "Then compile/run tools\cuda_p2p_test.cu."
