param(
    [string]$FirmwareDir = "D:\table\jiuyuan\delivery\firmware",
    [string[]]$TargetPorts = @(),
    [int]$BaudRate = 115200
)

$ErrorActionPreference = "Stop"

$burnTool = "D:\Program Files\HiSpark Studio 25.12.1\tools\BurnToolCmd\BurnToolCmd.exe"
$manifestPath = Join-Path $FirmwareDir "SHA256SUMS.txt"
$targets = @(
    [pscustomobject]@{ Role = "ground";  Port = "COM14"; Firmware = "ground_all.fwpkg" },
    [pscustomobject]@{ Role = "sky";     Port = "COM11"; Firmware = "sky_all.fwpkg" },
    [pscustomobject]@{ Role = "victim1"; Port = "COM12"; Firmware = "victim1_all.fwpkg" },
    [pscustomobject]@{ Role = "victim2"; Port = "COM9";  Firmware = "victim2_all.fwpkg" },
    [pscustomobject]@{ Role = "victim3"; Port = "COM15"; Firmware = "victim3_all.fwpkg" }
)

if ($TargetPorts.Count -gt 0) {
    $knownPorts = @($targets.Port)
    $unknownPorts = @($TargetPorts | Where-Object { $knownPorts -notcontains $_ })
    if ($unknownPorts.Count -gt 0) {
        throw "Unknown rescue target port(s): $($unknownPorts -join ', ')"
    }
    $targets = @($targets | Where-Object { $TargetPorts -contains $_.Port })
}

if (!(Test-Path -LiteralPath $burnTool)) {
    throw "BurnToolCmd not found: $burnTool"
}
if (!(Test-Path -LiteralPath $manifestPath)) {
    throw "Firmware SHA256 manifest is missing: $manifestPath"
}

$expectedHashes = @{}
foreach ($line in Get-Content -LiteralPath $manifestPath) {
    if ($line -match '^([0-9A-Fa-f]{64})\s+(.+)$') {
        $expectedHashes[$Matches[2]] = $Matches[1].ToUpperInvariant()
    }
}

$availablePorts = [System.IO.Ports.SerialPort]::GetPortNames()
foreach ($target in $targets) {
    if ($availablePorts -notcontains $target.Port) {
        throw "$($target.Role) port is not available: $($target.Port)"
    }
    $firmwarePath = Join-Path $FirmwareDir $target.Firmware
    if (!(Test-Path -LiteralPath $firmwarePath)) {
        throw "$($target.Role) firmware is missing: $firmwarePath"
    }
    if (!$expectedHashes.ContainsKey($target.Firmware)) {
        throw "$($target.Role) firmware is not listed in $manifestPath"
    }
    $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $firmwarePath).Hash
    if ($actualHash -ne $expectedHashes[$target.Firmware]) {
        throw "$($target.Role) firmware SHA256 mismatch: $firmwarePath"
    }
}

foreach ($target in $targets) {
    $firmwarePath = Join-Path $FirmwareDir $target.Firmware
    Write-Host "`nBurning $($target.Role) to $($target.Port) ..." -ForegroundColor Cyan
    Write-Host "When prompted: hold RECOVRY, tap RST, wait 1 second, then release RECOVRY." -ForegroundColor Yellow
    & $burnTool --burn -n ws63 -m serial $target.Port --baudRate $BaudRate -f $firmwarePath --reboot 1 --burnCheck 0
    if ($LASTEXITCODE -ne 0) {
        throw "Burn failed for $($target.Role) on $($target.Port), exit code $LASTEXITCODE"
    }
    Start-Sleep -Seconds 2
}

Write-Host "`nAll selected boards were burned successfully." -ForegroundColor Green
Write-Host "Press RESET once on each burned board before serial verification." -ForegroundColor Yellow
