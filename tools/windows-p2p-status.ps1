$ErrorActionPreference = "SilentlyContinue"
$ClassRoot = "HKLM:\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}"
$Names = @(
  "ForceP2P",
  "CLForceP2P",
  "RMForceP2PType",
  "RMPcieP2PType",
  "RMForceStaticBar1",
  "PeerMappingOverride",
  "RMDisableFeatureDisablement"
)

$keys = @(Get-ChildItem $ClassRoot | Where-Object {
    $p = Get-ItemProperty $_.PSPath
    ([string]$p.MatchingDeviceId -match 'VEN_10DE&DEV_220D') -or
    ([string]$p.DriverDesc -match 'CMP\s*90HX')
})

Write-Host "CMP 90HX registry state"
Write-Host "======================="
foreach ($key in $keys) {
    $p = Get-ItemProperty $key.PSPath
    Write-Host ""
    Write-Host $key.Name
    Write-Host ("  DriverDesc       : {0}" -f $p.DriverDesc)
    Write-Host ("  MatchingDeviceId : {0}" -f $p.MatchingDeviceId)
    foreach ($n in $Names) {
        $prop = $p.PSObject.Properties[$n]
        if ($null -ne $prop) {
            Write-Host ("  {0,-28} 0x{1:X}" -f $n, [uint32]$prop.Value)
        } else {
            Write-Host ("  {0,-28} <not set>" -f $n)
        }
    }
}

Write-Host ""
Write-Host "NVIDIA topology"
Write-Host "==============="
& nvidia-smi
Write-Host ""
& nvidia-smi topo -m
Write-Host ""
Write-Host "P2P read:"
& nvidia-smi topo -p2p r
Write-Host ""
Write-Host "P2P write:"
& nvidia-smi topo -p2p w
