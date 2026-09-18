[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$env:PYTHONUTF8 = '1'

$project = $PSScriptRoot
$platformioCore = if ($env:PLATFORMIO_CORE_DIR) {
    $env:PLATFORMIO_CORE_DIR
}
else {
    Join-Path $env:USERPROFILE '.platformio'
}
$pio = Join-Path $platformioCore 'penv\Scripts\platformio.exe'
$python = Join-Path $platformioCore 'penv\Scripts\python.exe'
$esptool = Join-Path $platformioCore 'packages\tool-esptoolpy\esptool.py'
$build = Join-Path $project '.pio\build\x4pro'
$dist = Join-Path $project 'dist'
$image = Join-Path $dist 'crossplay-transit-test-x4pro-full.bin'

foreach ($tool in @($pio, $python, $esptool)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        throw "Required build tool not found: $tool"
    }
}

Push-Location $project
try {
    & $pio run -e x4pro
    if ($LASTEXITCODE -ne 0) { throw "X4 Pro build failed with exit code $LASTEXITCODE" }

    $parts = @('bootloader.bin', 'partitions.bin', 'firmware.bin')
    foreach ($part in $parts) {
        $path = Join-Path $build $part
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Successful build did not produce $path"
        }
    }

    New-Item -ItemType Directory -Force -Path $dist | Out-Null
    & $python $esptool --chip esp32s3 merge-bin --format raw -o $image --flash-size 16MB `
        0x0 (Join-Path $build 'bootloader.bin') `
        0x8000 (Join-Path $build 'partitions.bin') `
        0x10000 (Join-Path $build 'firmware.bin')
    if ($LASTEXITCODE -ne 0) { throw "Firmware merge failed with exit code $LASTEXITCODE" }

    $bytes = [IO.File]::ReadAllBytes($image)
    if ($bytes.Length -le 0x10000 -or $bytes[0] -ne 0xE9 -or
        $bytes[0x8000] -ne 0xAA -or $bytes[0x8001] -ne 0x50 -or
        $bytes[0x10000] -ne 0xE9) {
        throw 'Merged image failed its bootloader, partition-table, or application marker check.'
    }

    $hash = (Get-FileHash -LiteralPath $image -Algorithm SHA256).Hash
    Write-Host ''
    Write-Host 'Firmware ready:' -ForegroundColor Green
    Write-Host $image
    Write-Host "SHA-256: $hash"
}
finally {
    Pop-Location
}
