param(
    [ValidateSet('All','CPU','None')][string]$Tests = 'All',
    [string]$BuildDirectory = 'build'
)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
function Run([string]$program, [string[]]$arguments) {
    & $program @arguments
    if ($LASTEXITCODE -ne 0) { throw "$program failed: $LASTEXITCODE" }
}
$dependencies = @(
    @('thorvg', 'https://github.com/thorvg/thorvg.git', 'v1.0.7', '1eef89b3ae16e7a00ccc7d8fadb493cc27f56d26'),
    @('ffgl', 'https://github.com/resolume/ffgl.git', 'v2.2', 'fda8d4a5904eaf09dd97ac0f95c246bd7f9f9a46'),
    @('json', 'https://github.com/nlohmann/json.git', 'v3.12.0', '55f93686c01528224f448c19128836e7df245f72')
)
foreach ($dep in $dependencies) {
    $path = Join-Path 'deps' $dep[0]
    if (!(Test-Path -LiteralPath $path)) { Run git @('clone', '--depth', '1', '--branch', $dep[2], $dep[1], $path) }
    $revision = & git -C $path rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $revision -ne $dep[3]) { throw "Unexpected dependency revision in $path" }
    $changes = & git -C $path status --porcelain
    if ($dep[0] -eq 'thorvg') { Run python @('scripts/apply-thorvg-patches.py') }
    elseif ($changes) { throw "Dependency has local changes: $path" }
}
if (!(Test-Path .tools/Scripts/python.exe)) { Run python @('-m', 'venv', '.tools') }
$python = (Resolve-Path .tools/Scripts/python.exe).Path
Run $python @('-m', 'pip', 'install', '-r', 'scripts/requirements-build.txt')
Run $python @('scripts/embed-demo.py', '--check')
Run cmd @('/c', 'scripts\build-thorvg.cmd')
Run cmd @('/c', 'scripts\build-thorvg-gl.cmd')
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (!$cmake) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $vsroot = & $vswhere -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vsroot) { $cmake = Join-Path $vsroot 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe' }
}
if (!$cmake -or !(Test-Path -LiteralPath $cmake)) { throw 'Install CMake 3.20+ or the VS CMake component.' }
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
Run $cmake @('-S', '.', '-B', $BuildDirectory, '-A', 'x64', '-DBUILD_FFGL=ON', "-DPython3_EXECUTABLE=$python")
Run $cmake @('--build', $BuildDirectory, '--config', 'Release')
if ($Tests -eq 'All') {
    Run $ctest @('--test-dir', $BuildDirectory, '-C', 'Release', '--output-on-failure')
} elseif ($Tests -eq 'CPU') {
    Run $ctest @('--test-dir', $BuildDirectory, '-C', 'Release', '-R', '^(core|png|features|transforms|emitter-state|palette|clipping)$', '--output-on-failure')
}
