#Requires -Version 5.1
[CmdletBinding()]
param([string]$SdkPrefix = $env:CONGREGATION_SDK_PREFIX, [string]$BuildType = "Release", [switch]$SkipTests)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot
if (-not $SdkPrefix) { $SdkPrefix = Join-Path $Root "..\..\SDK\build\congregation-native\prefix" }
$Build = Join-Path $Root "build"
$Overrides = @()
foreach ($Name in @("CONGREGATION_LVUI_PREFIX", "CONGREGATION_IIGUI_PREFIX", "CONGREGATION_COLOR_MANAGER_PREFIX", "CONGREGATION_CORE_PREFIX",
                   "CONGREGATION_ACCOUNT_MANAGER_PREFIX", "CONGREGATION_UPDATE_MANAGER_PREFIX", "CONGREGATION_PAINT_ENGINE_PREFIX",
                   "CONGREGATION_GRAPHIC_ADJUST_PREFIX", "CONGREGATION_SHARED_CANVAS_PREFIX", "CONGREGATION_FILE_PROVIDER_PREFIX",
                   "LVUI_HOST_COMPILER")) {
    $Value = [Environment]::GetEnvironmentVariable($Name)
    if ($Value) { $Overrides += "-D${Name}=$Value" }
}
& cmake -S $Root -B $Build "-DCMAKE_BUILD_TYPE=$BuildType" "-DCONGREGATION_SDK_PREFIX=$SdkPrefix" -DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON -DCMAKE_DISABLE_FIND_PACKAGE_LVRS=ON -DBUILD_TESTING=ON @Overrides
if ($LASTEXITCODE -ne 0) { throw "Native configuration failed" }
& cmake --build $Build --config $BuildType --parallel 6
if ($LASTEXITCODE -ne 0) { throw "Native build failed" }
if (-not $SkipTests) {
    & ctest --test-dir $Build -C $BuildType --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Native tests failed" }
}
