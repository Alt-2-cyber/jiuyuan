$ErrorActionPreference = "Stop"

$rescuerSrc = "D:\table\jiuyuan\xingshan\fbb_ws63-jiuyuanduan\fbb_ws63-master\src"
$victimSrc = "D:\table\jiuyuan\xingshan\fbb_ws63-shouzaiduan\fbb_ws63-master\src"

Write-Host "[1/2] Build rescuer firmware..."
Push-Location $rescuerSrc
python build.py ws63-liteos-app
Pop-Location

Write-Host "[2/2] Build victim firmware..."
Push-Location $victimSrc
python build.py ws63-liteos-app
Pop-Location

Write-Host "Build done."
Write-Host "Rescuer: $rescuerSrc\output\ws63\fwpkg\ws63-liteos-app\ws63-liteos-app_all.fwpkg"
Write-Host "Victim : $victimSrc\output\ws63\fwpkg\ws63-liteos-app\ws63-liteos-app_all.fwpkg"
