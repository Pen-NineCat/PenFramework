<#
.SYNOPSIS
    Builds PenMemory and its tests, then runs them.

.DESCRIPTION
    PenMemory lives at PenFramework/Code/Native/Memory and is normally built as
    PenMemory.dll by the root CMakeLists.txt (add_subdirectory), with tests
    enabled by the PEN_MEMORY_BUILD_TESTS option.  This script is the standalone
    Windows/MSVC path: it compiles the sources directly with cl.exe, using
    vcvars64.bat (located through vswhere), and does not go through CMake.
    Use -Debug to build with assertions enabled (/Od /MDd, NDEBUG undefined).
    On POSIX use CMakeLists.txt or compile the sources with
    "g++ -std=c++23 -O2 -pthread".

.EXAMPLE
    pwsh -File build.ps1
    pwsh -File build.ps1 -Debug
    pwsh -File build.ps1 -NoRun
#>
param(
    [switch]$Debug,
    [switch]$NoRun,
    [switch]$Bench
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$buildDir = Join-Path $root $(if ($Debug) { 'build-debug' } else { 'build' })
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

# --- locate the MSVC environment -------------------------------------------
$vcvars = $null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (Test-Path $vswhere) {
    $install = & $vswhere -latest -products * -property installationPath 2>$null
    if ($install) {
        $candidate = Join-Path $install 'VC\Auxiliary\Build\vcvars64.bat'
        if (Test-Path $candidate) { $vcvars = $candidate }
    }
}
if (-not $vcvars) {
    $candidates = Get-ChildItem -Path @(
        'C:\Program Files\Microsoft Visual Studio',
        'C:\Program Files (x86)\Microsoft Visual Studio',
        'E:\Visual Studio'
    ) -Filter 'vcvars64.bat' -Recurse -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty FullName
    if ($candidates) { $vcvars = $candidates[0] }
}
if (-not $vcvars) {
    throw 'Could not find vcvars64.bat. Install Visual Studio with the C++ workload, or build with CMake/g++.'
}
Write-Host "MSVC environment: $vcvars" -ForegroundColor DarkGray

# --- sources ---------------------------------------------------------------
$libSources = @(
    'Interface.cpp'
    'ThreadCache.cpp'
    'CentralCache.cpp'
    'PageCache.cpp'
    'Radix.cpp'
    'OSMemory.cpp'
)
$tests = [ordered]@{
    'test_sizemap'     = @('tests\test_sizemap.cpp')
    'test_pagecache'   = @('tests\test_pagecache.cpp')
    'test_centralcache'= @('tests\test_centralcache.cpp')
    'test_threadcache' = @('tests\test_threadcache.cpp')
    'test_interface'   = @('tests\test_interface.cpp')
    'test_override'    = @('tests\test_override.cpp', '..\Engine\Memory\MemoryOperator.cpp')
}
$benchmarks = [ordered]@{
    'benchmark'        = @('tests\benchmark.cpp')
}

if ($Debug) {
    $flags = '/nologo /std:c++latest /EHsc /W4 /WX /Od /Zi /MDd /D_DEBUG'
} else {
    $flags = '/nologo /std:c++latest /EHsc /W4 /WX /O2 /MD /DNDEBUG'
}
$includes = "/I `"$root`""

# --- generate one batch file so the toolchain only starts once -------------
$bat = Join-Path $buildDir 'build.bat'
$lines = @('@echo off', "call `"$vcvars`" >nul", "cd /d `"$buildDir`"")

foreach ($source in $libSources) {
    $lines += "cl $flags $includes /c `"$(Join-Path $root $source)`" || exit /b 1"
}
$libObjects = ($libSources | ForEach-Object { [IO.Path]::GetFileNameWithoutExtension($_) + '.obj' }) -join ' '

foreach ($name in $tests.Keys) {
    $sources = ($tests[$name] | ForEach-Object { "`"$(Join-Path $root $_)`"" }) -join ' '
    $lines += "cl $flags $includes $sources $libObjects /Fe:$name.exe || exit /b 1"
}
foreach ($name in $benchmarks.Keys) {
    $sources = ($benchmarks[$name] | ForEach-Object { "`"$(Join-Path $root $_)`"" }) -join ' '
    $lines += "cl $flags $includes $sources $libObjects /Fe:$name.exe || exit /b 1"
}
$lines += 'exit /b 0'
Set-Content -Path $bat -Value $lines -Encoding ASCII

Write-Host "Building ($(if ($Debug) { 'debug' } else { 'release' })) ..." -ForegroundColor Cyan
& cmd.exe /c $bat
if ($LASTEXITCODE -ne 0) { throw "build failed with exit code $LASTEXITCODE" }

if ($NoRun) { Write-Host 'Build finished (-NoRun).' -ForegroundColor Green; exit 0 }

# --- run -------------------------------------------------------------------
Write-Host ''
$failed = @()
foreach ($name in $tests.Keys) {
    $exe = Join-Path $buildDir "$name.exe"
    & $exe
    if ($LASTEXITCODE -ne 0) { $failed += $name }
}

Write-Host ''
if ($failed.Count -eq 0) {
    Write-Host "All $($tests.Count) test binaries passed." -ForegroundColor Green
} else {
    Write-Host "FAILED: $($failed -join ', ')" -ForegroundColor Red
    exit 1
}

if ($Bench) {
    Write-Host ''
    & (Join-Path $buildDir 'benchmark.exe')
}

exit 0
