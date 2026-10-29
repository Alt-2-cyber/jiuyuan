param(
    [int]$Seconds = 30,
    [string]$OutputFile = "D:\table\jiuyuan\delivery\rescue_network_verify.log"
)

$ErrorActionPreference = "Stop"
$captureScript = "D:\table\jiuyuan\xingshan\relay_test_fw\capture-serial.ps1"
$targets = @(
    [pscustomobject]@{
        Role = "ground"
        Port = "COM14"
        Markers = @("[GROUND_RX]", "VD-001", "VD-002", "VD-003")
    },
    [pscustomobject]@{
        Role = "sky"
        Port = "COM11"
        Markers = @("[SKY_FORWARD]")
    },
    [pscustomobject]@{
        Role = "victim1"
        Port = "COM12"
        Markers = @("[VICTIM_TX] role=VICTIM-1 id=VD-001")
    },
    [pscustomobject]@{
        Role = "victim2"
        Port = "COM9"
        Markers = @("[VICTIM_TX] role=VICTIM-2 id=VD-002")
    },
    [pscustomobject]@{
        Role = "victim3"
        Port = "COM15"
        Markers = @("[VICTIM_TX] role=VICTIM-3 id=VD-003")
    }
)
$ports = @($targets.Port)

if (!(Test-Path -LiteralPath $captureScript)) {
    throw "Serial capture script not found: $captureScript"
}

$availablePorts = [System.IO.Ports.SerialPort]::GetPortNames()
foreach ($target in $targets) {
    if ($availablePorts -notcontains $target.Port) {
        throw "$($target.Role) port is not available: $($target.Port)"
    }
}

$lines = @(& $captureScript -Ports $ports -Baud 115200 -Seconds $Seconds)
$lines | Tee-Object -FilePath $OutputFile

$failures = [System.Collections.Generic.List[string]]::new()
foreach ($target in $targets) {
    $prefix = "[$($target.Port)] "
    $fragments = @(
        foreach ($line in $lines) {
            $text = [string]$line
            if ($text.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
                $text.Substring($prefix.Length)
            }
        }
    )

    if ($fragments.Count -eq 0) {
        $failures.Add("$($target.Port) ($($target.Role)): no serial output")
        continue
    }

    # ReadExisting can split one UART line across reads, so rejoin fragments per port before matching.
    $portText = $fragments -join ""
    foreach ($marker in $target.Markers) {
        if (!$portText.Contains($marker)) {
            $failures.Add("$($target.Port) ($($target.Role)): marker not found: $marker")
        }
    }
}

if ($failures.Count -gt 0) {
    throw "Rescue network verification failed:`n - $($failures -join "`n - ")`nSee $OutputFile"
}

Write-Host "End-to-end rescue network verification passed: $OutputFile" -ForegroundColor Green
