$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null

$files = @(
    @{ Source = 'bin\x64\PeripheralBatteryPlugin.dll'; Name = 'PeripheralBatteryPlugin-x64.dll' },
    @{ Source = 'bin\x86\PeripheralBatteryPlugin.dll'; Name = 'PeripheralBatteryPlugin-x86.dll' }
)

foreach ($file in $files) {
    $source = Join-Path $root $file.Source
    if (Test-Path $source) {
        Copy-Item $source (Join-Path $dist $file.Name) -Force
        Write-Output "packed $($file.Name)"
    }
    else {
        Write-Warning "missing $source"
    }
}

foreach ($name in @('README.md', 'LICENSE', 'NOTICE.md', 'CHANGELOG.md')) {
    $source = Join-Path $root $name
    if (Test-Path $source) {
        Copy-Item $source (Join-Path $dist $name) -Force
    }
}

$zip = Join-Path $dist 'PeripheralBatteryPlugin-1.2.0.zip'
if (Test-Path $zip) {
    Remove-Item $zip -Force
}
Compress-Archive -Path (Join-Path $dist '*') -DestinationPath $zip
Write-Output "archive: $zip"
