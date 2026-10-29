param(
    [string]$OutputDir = "D:\table\jiuyuan\delivery\firmware"
)

$ErrorActionPreference = "Stop"

$sourceDir = "D:\table\jiuyuan\xingshan\fbb_ws63-shouzaiduan\fbb_ws63-master\src"
$shortRoot = "D:\jr"
$shortDir = "D:\jr\n"
$hisparkTools = "D:\Program Files\HiSpark Studio 25.12.1\tools\Windows"
$cmakeDir = "D:\Program Files\Huawei\DevEco Studio\sdk\default\openharmony\native\build-tools\cmake\bin"
$firmwareRel = "output\ws63\fwpkg\ws63-liteos-app\ws63-liteos-app_all.fwpkg"
$manifestPath = Join-Path $OutputDir "SHA256SUMS.txt"

$builds = @(
    [pscustomobject]@{
        Name = "ground"
        Defines = "CONFIG_ENABLE_PRODUCTS_SAMPLE=y,CONFIG_SAMPLE_SUPPORT_SLE_RELAY=y,CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT=y,CONFIG_SUPPORT_SLE_PERIPHERAL=y"
    },
    [pscustomobject]@{
        Name = "sky"
        Defines = "CONFIG_ENABLE_PRODUCTS_SAMPLE=y,CONFIG_SAMPLE_SUPPORT_SLE_RELAY=y,CONFIG_SAMPLE_SUPPORT_SLE_RELAY_NODE=y,CONFIG_SUPPORT_SLE_PERIPHERAL=y,CONFIG_SUPPORT_SLE_CENTRAL=y"
    },
    [pscustomobject]@{
        Name = "victim1"
        Defines = "CONFIG_ENABLE_PRODUCTS_SAMPLE=y,CONFIG_SAMPLE_SUPPORT_SLE_RELAY=y,CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM=y,CONFIG_SLE_RELAY_VICTIM_INDEX=1,CONFIG_SUPPORT_SLE_PERIPHERAL=y,CONFIG_SUPPORT_SLE_CENTRAL=y"
    },
    [pscustomobject]@{
        Name = "victim2"
        Defines = "CONFIG_ENABLE_PRODUCTS_SAMPLE=y,CONFIG_SAMPLE_SUPPORT_SLE_RELAY=y,CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM=y,CONFIG_SLE_RELAY_VICTIM_INDEX=2,CONFIG_SUPPORT_SLE_PERIPHERAL=y,CONFIG_SUPPORT_SLE_CENTRAL=y"
    },
    [pscustomobject]@{
        Name = "victim3"
        Defines = "CONFIG_ENABLE_PRODUCTS_SAMPLE=y,CONFIG_SAMPLE_SUPPORT_SLE_RELAY=y,CONFIG_SAMPLE_SUPPORT_SLE_RELAY_VICTIM=y,CONFIG_SLE_RELAY_VICTIM_INDEX=3,CONFIG_SUPPORT_SLE_CENTRAL=y"
    }
)

if (!(Test-Path -LiteralPath "$sourceDir\build.py")) {
    throw "WS63 source project not found: $sourceDir"
}
if (!(Test-Path -LiteralPath "$cmakeDir\cmake.exe")) {
    throw "DevEco CMake not found: $cmakeDir"
}
if (!(Test-Path -LiteralPath $hisparkTools)) {
    throw "HiSpark build tools not found: $hisparkTools"
}
$resolvedShortDir = [IO.Path]::GetFullPath($shortDir).TrimEnd('\')
if ($resolvedShortDir -ne "D:\jr\n") {
    throw "Refusing to build outside the expected short path: $shortDir"
}

if (Test-Path -LiteralPath $shortDir) {
    Remove-Item -LiteralPath $resolvedShortDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $shortRoot, $shortDir, $OutputDir | Out-Null
foreach ($build in $builds) {
    $staleFirmware = Join-Path $OutputDir "$($build.Name)_all.fwpkg"
    if (Test-Path -LiteralPath $staleFirmware) {
        Remove-Item -LiteralPath $staleFirmware -Force
    }
}
if (Test-Path -LiteralPath $manifestPath) {
    Remove-Item -LiteralPath $manifestPath -Force
}

Write-Host "Syncing WS63 source to $shortDir ..."
& robocopy $sourceDir $shortDir /E /XJ /XD "$sourceDir\output" "$sourceDir\.git" /XF build.log /R:2 /W:1 /NFL /NDL /NP
if ($LASTEXITCODE -ge 8) {
    throw "Source sync failed with robocopy code $LASTEXITCODE"
}

$oldPath = $env:PATH
$oldRedirect = $env:JIUYUAN_SHORTPATH_BUILD
try {
    $env:PATH = "$cmakeDir;$hisparkTools\ninja;$hisparkTools\cc_riscv32_musl_fp_win\bin;$hisparkTools\cc_riscv32_musl_win\bin;$oldPath"
    $env:JIUYUAN_SHORTPATH_BUILD = "1"
    $manifestLines = [System.Collections.Generic.List[string]]::new()

    Push-Location $shortDir
    try {
        foreach ($build in $builds) {
            Write-Host "`nBuilding $($build.Name) firmware ..." -ForegroundColor Cyan
            & python build.py -c ws63-liteos-app "-def=$($build.Defines)"
            if ($LASTEXITCODE -ne 0) {
                throw "Build failed for $($build.Name) with exit code $LASTEXITCODE"
            }

            $sourceFirmware = Join-Path $shortDir $firmwareRel
            if (!(Test-Path -LiteralPath $sourceFirmware)) {
                throw "Build completed but firmware is missing: $sourceFirmware"
            }
            $destination = Join-Path $OutputDir "$($build.Name)_all.fwpkg"
            Copy-Item -LiteralPath $sourceFirmware -Destination $destination -Force
            $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $destination).Hash
            $manifestLines.Add("$hash  $($build.Name)_all.fwpkg")
            Write-Host "$($build.Name): $destination"
            Write-Host "SHA256: $hash"
        }
        [IO.File]::WriteAllLines($manifestPath, $manifestLines, [Text.Encoding]::ASCII)
    } finally {
        Pop-Location
    }
} finally {
    $env:PATH = $oldPath
    $env:JIUYUAN_SHORTPATH_BUILD = $oldRedirect
}

Write-Host "`nAll five rescue firmware images were built successfully." -ForegroundColor Green
Write-Host "SHA256 manifest: $manifestPath"
