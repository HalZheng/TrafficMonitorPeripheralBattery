param(
    [switch]$SkipX86
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot

$cmake = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path $cmake)) {
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        throw "cmake.exe not found. Install Visual Studio C++ tools or CMake."
    }
    $cmake = $command.Source
}

function Build-Arch([string]$Preset) {
    Write-Output "=== Configure $Preset ==="
    & $cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) {
        throw "configure failed for $Preset"
    }

    Write-Output "=== Build $Preset ==="
    & $cmake --build --preset $Preset
    if ($LASTEXITCODE -ne 0) {
        throw "build failed for $Preset"
    }
}

Push-Location $root
try {
    Build-Arch 'x64'

    $ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
    if (Test-Path $ctest) {
        & $ctest --test-dir (Join-Path $root 'build\x64') -C Release --output-on-failure
        if ($LASTEXITCODE -ne 0) {
            throw "x64 tests failed"
        }
    }

    if (-not $SkipX86) {
        Build-Arch 'x86'
    }
}
finally {
    Pop-Location
}

Write-Output "x64: $root\bin\x64\PeripheralBatteryPlugin.dll"
if (-not $SkipX86) {
    Write-Output "x86: $root\bin\x86\PeripheralBatteryPlugin.dll"
}
