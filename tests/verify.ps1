# verify.ps1 -- build and verify the cloc exercise test cases.
# Usage: powershell -ExecutionPolicy Bypass -File tests\verify.ps1
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot

# ---------- 1. locate (or build) cloc.exe ----------
$candidates = @(
    (Join-Path $root 'build\cloc.exe'),
    (Join-Path $root 'build\Release\cloc.exe'),
    (Join-Path $root 'build\Debug\cloc.exe'),
    (Join-Path $root 'cmake-build-release\cloc.exe'),
    (Join-Path $root 'cmake-build-debug\cloc.exe')
)
$exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $exe) {
    Write-Host 'cloc.exe not found, trying to build with CMake ...'
    $buildDir = Join-Path $root 'build'
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
    # 依次尝试：默认生成器 -> Ninja -> MinGW Makefiles。
    # 某些机器装了 VS 却没有 C++ 工具集（缺 nmake），默认生成器会失败，
    # 此时回退到 MinGW/Ninja 才能构建成功。
    $configured = $false
    foreach ($gen in @($null, 'Ninja', 'MinGW Makefiles')) {
        Remove-Item -Recurse -Force (Join-Path $buildDir 'CMakeCache.txt'),
                                    (Join-Path $buildDir 'CMakeFiles') -ErrorAction SilentlyContinue
        if ($gen) { cmake -S $root -B $buildDir -G $gen } else { cmake -S $root -B $buildDir }
        if ($LASTEXITCODE -eq 0) { $configured = $true; break }
    }
    if (-not $configured) { Write-Error 'CMake configure failed'; exit 1 }
    cmake --build $buildDir --config Release
    if ($LASTEXITCODE -ne 0) { Write-Error 'CMake build failed'; exit 1 }
    $exe = @((Join-Path $buildDir 'Release\cloc.exe'), (Join-Path $buildDir 'cloc.exe')) |
        Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $exe) { Write-Error 'build failed: cloc.exe not found'; exit 1 }
Write-Host "using executable: $exe"
Write-Host ''

# ---------- 2. case 1: the 12-line sample from the problem statement ----------
$out1 = (& $exe (Join-Path $root 'tests\case1.c')) -join "`n"
Write-Host $out1
Write-Host ''
$row1 = ($out1 -split "`n" | Where-Object { $_ -match '^\s+\d+(\s+\d+){4}\s*$' } | Select-Object -First 1)
if (-not $row1) { Write-Error 'no data row found in output'; exit 1 }
$num1 = [regex]::Matches($row1, '\d+') | ForEach-Object { [int]$_.Value }
$got1 = ($num1 | Select-Object -Last 5) -join ' '
$want1 = '1 12 5 5 2'   # files lines code comments blanks
if ($got1 -ne $want1) { Write-Error "case 1 FAILED: expected [$want1] got [$got1]"; exit 1 }
Write-Host "case 1 passed: $want1" -ForegroundColor Green

# ---------- 3. case 2: edge cases (comment markers inside strings, escapes, line continuation) ----------
$out2 = (& $exe (Join-Path $root 'tests\edge_cases')) -join "`n"
Write-Host $out2
Write-Host ''
$row2 = ($out2 -split "`n" | Where-Object { $_ -match '^\s+\d+(\s+\d+){4}\s*$' } | Select-Object -First 1)
if (-not $row2) { Write-Error 'no data row found in output'; exit 1 }
$num2 = [regex]::Matches($row2, '\d+') | ForEach-Object { [int]$_.Value }
$got2 = ($num2 | Select-Object -Last 5) -join ' '
$want2 = '1 12 5 6 1'   # files lines code comments blanks
if ($got2 -ne $want2) { Write-Error "case 2 FAILED: expected [$want2] got [$got2]"; exit 1 }
Write-Host "case 2 passed: $want2" -ForegroundColor Green

# ---------- 4. case 3: Ruby sample from the bonus task (7 lines) ----------
$out3 = (& $exe (Join-Path $root 'tests\case1.rb')) -join "`n"
Write-Host $out3
Write-Host ''
$row3 = ($out3 -split "`n" | Where-Object { $_ -match '^\s+\d+(\s+\d+){4}\s*$' } | Select-Object -First 1)
if (-not $row3) { Write-Error 'no data row found in output'; exit 1 }
$num3 = [regex]::Matches($row3, '\d+') | ForEach-Object { [int]$_.Value }
$got3 = ($num3 | Select-Object -Last 5) -join ' '
$want3 = '1 7 1 4 2'   # files lines code comments blanks
if ($got3 -ne $want3) { Write-Error "case 3 FAILED: expected [$want3] got [$got3]"; exit 1 }
Write-Host "case 3 passed: $want3" -ForegroundColor Green

# ---------- 5. case 4: C++11 raw strings with encoding prefixes (u8R/uR/UR/LR) ----------
$out4 = (& $exe (Join-Path $root 'tests\rawstrings.cpp')) -join "`n"
Write-Host $out4
Write-Host ''
$row4 = ($out4 -split "`n" | Where-Object { $_ -match '^\s+\d+(\s+\d+){4}\s*$' } | Select-Object -First 1)
if (-not $row4) { Write-Error 'no data row found in output'; exit 1 }
$num4 = [regex]::Matches($row4, '\d+') | ForEach-Object { [int]$_.Value }
$got4 = ($num4 | Select-Object -Last 5) -join ' '
$want4 = '1 11 10 1 0'   # files lines code comments blanks
if ($got4 -ne $want4) { Write-Error "case 4 FAILED: expected [$want4] got [$got4]"; exit 1 }
Write-Host "case 4 passed: $want4" -ForegroundColor Green

Write-Host ''
Write-Host 'ALL CHECKS PASSED.' -ForegroundColor Green
exit 0
