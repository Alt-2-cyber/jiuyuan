$ErrorActionPreference = "Stop"

$burnTool = "D:\Program Files\HiSpark Studio 25.12.1\tools\cfbb\BurnTool\BurnTool.exe"
$firmware = "D:\table\jiuyuan\xingshan\fbb_ws63-jiuyuanduan\fbb_ws63-master\src\output\ws63\fwpkg\ws63-liteos-app\ws63-liteos-app_all.fwpkg"

if (!(Test-Path -LiteralPath $burnTool)) {
    throw "BurnTool not found: $burnTool"
}
if (!(Test-Path -LiteralPath $firmware)) {
    throw "Firmware not found: $firmware"
}

Write-Host "Burn rescuer firmware to COM9..."
& $burnTool -com:9 -bin:$firmware -signalbaud:921600 -reset -switchafterloader
