#requires -RunAsAdministrator
$ErrorActionPreference = "Stop"
$BackupPath = Join-Path $PSScriptRoot "cmp90hx-p2p-registry-backup.json"

if (-not (Test-Path $BackupPath)) {
    throw "Backup not found: $BackupPath"
}

$data = Get-Content $BackupPath -Raw | ConvertFrom-Json

foreach ($entry in @($data)) {
    $regPath = "Registry::$($entry.Key)"
    if (-not (Test-Path $regPath)) {
        Write-Warning "Registry key no longer exists: $($entry.Key)"
        continue
    }

    Write-Host "Restoring $($entry.Key)"
    foreach ($prop in $entry.Values.PSObject.Properties) {
        $name = $prop.Name
        $saved = $prop.Value
        if ([bool]$saved.Exists) {
            New-ItemProperty -Path $regPath -Name $name -PropertyType DWord -Value ([uint32]$saved.Value) -Force | Out-Null
            Write-Host ("  restore {0}=0x{1:X}" -f $name, [uint32]$saved.Value)
        } else {
            Remove-ItemProperty -Path $regPath -Name $name -ErrorAction SilentlyContinue
            Write-Host "  remove $name"
        }
    }
}

Write-Host ""
Write-Host "Registry restored. Reboot Windows through the normal tested UEFI path."
