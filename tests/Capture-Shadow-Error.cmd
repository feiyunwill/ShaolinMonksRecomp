@echo off
setlocal
if not exist "%~dp0OpenShao.exe" (
    echo Copy this script beside a diagnostic OpenShao.exe first.
    pause
    exit /b 1
)
set "OPENSHAO_SHADOW_SHADER_COMPARE=0"
set "OPENSHAO_RASTER_DEPTH_CLEAR=0"
set "OPENSHAO_CAPTURE_SUITE=1"
set "XBE_D3D12_DEBUG=1"
set "XBE_CAPTURE_DIR=%~dp0captures"
if not exist "%XBE_CAPTURE_DIR%" mkdir "%XBE_CAPTURE_DIR%"
powershell.exe -NoProfile -Command "Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,DriverDate,PNPDeviceID | Format-List | Out-File -LiteralPath (Join-Path $env:XBE_CAPTURE_DIR 'gpu-info.txt') -Encoding utf8; Get-FileHash -LiteralPath (Join-Path (Split-Path -Parent $env:XBE_CAPTURE_DIR) 'OpenShao.exe') -Algorithm SHA256 | Format-List | Out-File -LiteralPath (Join-Path $env:XBE_CAPTURE_DIR 'build-info.txt') -Encoding utf8"
echo Enter the affected scene and stand still. Press F3 ONCE.
echo All diagnostic checks run automatically. Rendering can pause.
echo Wait 30 seconds after F3, then close the game. An archive is created below.
"%~dp0OpenShao.exe" > "%XBE_CAPTURE_DIR%\runtime.log" 2>&1
powershell.exe -NoProfile -Command "$zip = Join-Path (Split-Path -Parent $env:XBE_CAPTURE_DIR) ('Captures-Diagnostics-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.zip'); Compress-Archive -LiteralPath $env:XBE_CAPTURE_DIR -DestinationPath $zip; if ($?) { Write-Host ('Send this archive: ' + $zip) } else { exit 1 }"
pause
