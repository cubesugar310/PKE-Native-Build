param(
    [string]$BuildDir = "native/build",
    [string]$Configuration = "Release",
    [string]$OutputDir = "native/dist/PKE_Native_Prototype_0.1"
)
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "../..")
$build = Join-Path $root $BuildDir
$out = Join-Path $root $OutputDir
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force $out | Out-Null
$candidates = @(
    (Join-Path $build "$Configuration/pke_native.exe"),
    (Join-Path $build "pke_native.exe")
)
$exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) { throw "pke_native.exe not found under $build. Build Release first." }
Copy-Item $exe (Join-Path $out "PKE Native Prototype.exe")
Get-ChildItem -Path $build -Recurse -Filter *.dll | ForEach-Object {
    Copy-Item $_.FullName (Join-Path $out $_.Name) -Force
}
Copy-Item (Join-Path $root "native/pke-native.example.json") (Join-Path $out "pke-native.example.json")
Copy-Item (Join-Path $root "native/README.md") (Join-Path $out "README.md")
$licenses = Join-Path $out "licenses"
New-Item -ItemType Directory -Force $licenses | Out-Null
Copy-Item (Join-Path $root "native/third_party/quickjs-ng/LICENSE") (Join-Path $licenses "quickjs-ng-LICENSE.txt")
$forbidden = Get-ChildItem -Path $out -Recurse | Where-Object { $_.Name -match 'WebView2|Chromium|chrome_elf|msedgewebview' }
if ($forbidden) { throw "Forbidden browser runtime file found in package: $($forbidden.FullName -join ', ')" }
$zip = "$out.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $out '*') -DestinationPath $zip
Write-Host "Packaged: $zip"
