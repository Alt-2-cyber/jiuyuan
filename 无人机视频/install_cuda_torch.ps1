$ErrorActionPreference = 'Stop'

$venv = 'D:\table\jiuyuan\.venv-yolo'
$log = 'D:\table\jiuyuan\cuda_torch_install.log'

Start-Transcript -LiteralPath $log -Append | Out-Null
try {
    & "$venv\Scripts\python.exe" -m pip install `
        'torch==2.11.0+cu128' `
        'torchvision==0.26.0+cu128' `
        '--index-url' 'https://download.pytorch.org/whl/cu128' `
        '--progress-bar' 'off'
    exit $LASTEXITCODE
} finally {
    Stop-Transcript | Out-Null
}
