#Requires -Version 5.1
[CmdletBinding()]
param([string]$SdkPrefix = $env:CONGREGATION_SDK_PREFIX, [string]$BuildType = "Release", [switch]$SkipTests)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot
if (-not $SdkPrefix) { $SdkPrefix = Join-Path $Root "..\..\SDK\build\congregation-native\prefix" }
$Build = Join-Path $Root "build"
& cmake -S $Root -B $Build "-DCMAKE_BUILD_TYPE=$BuildType" "-DCONGREGATION_SDK_PREFIX=$SdkPrefix" -DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw "Native configuration failed" }
& cmake --build $Build --config $BuildType --parallel 6
if ($LASTEXITCODE -ne 0) { throw "Native build failed" }
if (-not $SkipTests) {
    & ctest --test-dir $Build -C $BuildType --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Native tests failed" }
}
