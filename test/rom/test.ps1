# Download the firmware
Invoke-WebRequest -Uri 'http://raymii.org/s/inc/downloads/es40-srmon/cl67srmrom.exe' -OutFile 'cl67srmrom.exe'

# Start AXPbox
Start-Process '..\..\..\build\Release\axpbox' -ArgumentList 'run' -NoNewWindow  -RedirectStandardOutput stdout.txt -RedirectStandardError stderr.txt

# Wait for AXPbox to start
Start-Sleep -Seconds 5

# Connect to terminal
Start-Process -FilePath 'nc' -ArgumentList '-t', '127.0.0.1', '21000' -NoNewWindow -RedirectStandardOutput 'axp.log'

# Wait for the last line of log to become P00>>>
$timeout = 300
while ($true) {
    if ($timeout -eq 0) {
        Write-Host "waiting for SRM prompt timed out" -ForegroundColor Red
        exit 1
    }

#    echo "=== start axp.log ==="
#    Get-Content -Path 'axp.log' -Raw
#    echo "=== end axp.log ==="

    $content = Get-Content -Path 'axp.log' -Raw
    $contentWithoutNullBytes = $content -replace '\0', ''

    if ($contentWithoutNullBytes -match "P00>>>") {
        exit 0
    }

    Start-Sleep -Seconds 1
    $timeout--
}

Stop-Process -Name 'nc'
