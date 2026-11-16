param(
    [switch]$Push
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$cmakeLists = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw
if ($cmakeLists -notmatch 'project\([^)]*VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
    throw "cannot read the project version from CMakeLists.txt"
}
$version = $Matches[1]
$tag = "v$version"

$existing = git -C $root tag --list $tag
if ($existing) {
    throw "tag $tag already exists"
}

git -C $root tag -a $tag -m "Release $version"
if ($LASTEXITCODE -ne 0) {
    throw "failed to create tag $tag"
}
Write-Output "created $tag"

if ($Push) {
    git -C $root push origin $tag
    if ($LASTEXITCODE -ne 0) {
        throw "failed to push $tag"
    }
    Write-Output "pushed $tag"
}
