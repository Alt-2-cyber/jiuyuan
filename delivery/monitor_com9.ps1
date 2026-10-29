param(
    [string]$Port = "COM9",
    [int]$Seconds = 60,
    [int]$BaudRate = 115200
)

$ErrorActionPreference = "Stop"

$serialPort = New-Object System.IO.Ports.SerialPort $Port, $BaudRate, "None", 8, "One"
$serialPort.ReadTimeout = 1000

try {
    $serialPort.Open()
    Write-Host "Monitoring $Port at $BaudRate for $Seconds seconds..."
    $end = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $end) {
        try {
            $line = $serialPort.ReadLine()
            if ($line) {
                $line
            }
        } catch [System.TimeoutException] {
        }
    }
} finally {
    if ($serialPort.IsOpen) {
        $serialPort.Close()
    }
}
