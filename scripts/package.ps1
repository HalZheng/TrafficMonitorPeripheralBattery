$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $root 'dist'
if (Test-Path $dist) {
    Remove-Item $dist -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $dist | Out-Null

$cmakeLists = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw
if ($cmakeLists -notmatch 'project\([^)]*VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
    throw "cannot read the project version from CMakeLists.txt"
}
$version = $Matches[1]

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

$docs = Join-Path $root 'docs'
if (Test-Path $docs) {
    $target = Join-Path $dist 'docs'
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    Copy-Item (Join-Path $docs '*.md') $target -Force
    $images = Join-Path $docs 'images'
    if (Test-Path $images) {
        $imageTarget = Join-Path $target 'images'
        New-Item -ItemType Directory -Force -Path $imageTarget | Out-Null
        Copy-Item (Join-Path $images '*') $imageTarget -Force
    }
}

$licenses = Join-Path $root 'LICENSES'
if (Test-Path $licenses) {
    Copy-Item $licenses (Join-Path $dist 'LICENSES') -Recurse -Force
}

$zip = Join-Path $dist "PeripheralBatteryPlugin-$version.zip"
if (Test-Path $zip) {
    Remove-Item $zip -Force
}
Compress-Archive -Path (Join-Path $dist '*') -DestinationPath $zip
Write-Output "archive: $zip"
