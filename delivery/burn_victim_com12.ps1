$ErrorActionPreference = "Stop"

$burnTool = "D:\Program Files\HiSpark Studio 25.12.1\tools\BurnToolCmd\BurnToolCmd.exe"
$firmware = "D:\table\jiuyuan\xingshan\fbb_ws63-shouzaiduan\fbb_ws63-master\src\output\ws63\fwpkg\ws63-liteos-app\ws63-liteos-app_all.fwpkg"

if (!(Test-Path -LiteralPath $burnTool)) {
    throw "BurnTool not found: $burnTool"
}
if (!(Test-Path -LiteralPath $firmware)) {
    throw "Firmware not found: $firmware"
}

Write-Host "Burn victim firmware to COM12..."
Write-Host "Make sure the victim board is connected as COM12 before running this script."
& $burnTool --burn -n ws63 -m serial COM12 -b 921600 -s 0 -p 0 -f $firmware -re 1 -c 0
