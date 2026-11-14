param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

foreach ($arch in @('x86', 'x64')) {
    $platform = if ($arch -eq 'x86') { 'Win32' } else { 'x64' }
    $build = Join-Path $root ("build\{0}" -f $arch)
    $configure = @(
        '-S', $root, '-B', $build,
        '-G', 'Visual Studio 17 2022',
        '-A', $platform,
        "-DCMAKE_BUILD_TYPE=$Configuration"
    )
    & cmake @configure
    if ($LASTEXITCODE -ne 0) {
        throw "ACuda NVAPI $arch configure failed with exit code $LASTEXITCODE"
    }
    cmake --build $build --config $Configuration --parallel
    if ($LASTEXITCODE -ne 0) {
        throw "ACuda NVAPI $arch build failed with exit code $LASTEXITCODE"
    }
}

Write-Host "ACuda NVAPI DLLs:"
Get-ChildItem (Join-Path $root 'bin') -Recurse -Filter 'nvapi*.dll' | ForEach-Object { Write-Host "  $($_.FullName)" }