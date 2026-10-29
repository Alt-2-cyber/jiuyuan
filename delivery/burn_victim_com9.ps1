$ErrorActionPreference = "Stop"

$burnTool = "D:\Program Files\HiSpark Studio 25.12.1\tools\cfbb\BurnTool\BurnTool.exe"
$firmware = "D:\table\jiuyuan\xingshan\fbb_ws63-shouzaiduan\fbb_ws63-master\src\output\ws63\fwpkg\ws63-liteos-app\ws63-liteos-app_all.fwpkg"

if (!(Test-Path -LiteralPath $burnTool)) {
    throw "BurnTool not found: $burnTool"
}
if (!(Test-Path -LiteralPath $firmware)) {
    throw "Firmware not found: $firmware"
}

Write-Host "Burn victim firmware to COM9..."
Write-Host "Make sure the victim board is connected as COM9 before running this script."
& $burnTool -com:9 -bin:$firmware -signalbaud:921600 -reset -switchafterloader
