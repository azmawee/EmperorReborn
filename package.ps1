<#
.SYNOPSIS
  Build Emperor Reborn (Release/x86) and package a redistributable zip.

.DESCRIPTION
  Produces dist\EmperorReborn-v<Version>.zip containing only files we are
  allowed to distribute:
      EmperorReborn.exe
      EmperorHooks.dll
      INSTALL.txt
      README.txt   (copy of readme.md)
      license.txt
      THIRD-PARTY-NOTICES.txt

  It does NOT include EM109EN.EXE (the EA/Westwood v1.09 patch) or any game
  data - those are copyrighted by EA/Westwood and the user must supply their
  own. As a safety net the script aborts if EmperorReborn.exe is unexpectedly
  large, which would mean the EA patch got embedded into the executable.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\package.ps1 -Version 1.0
#>
param(
  [string]$Version = "1.0",
  [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
Set-Location $root

# --- locate MSBuild ---------------------------------------------------------
function Find-MSBuild {
  $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  if (Test-Path $vswhere) {
    $p = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild `
                    -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
    if ($p) { return $p }
  }
  $cmd = Get-Command msbuild -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  throw "MSBuild not found. Install Visual Studio 2022+ with the C++ workload."
}

# --- build ------------------------------------------------------------------
if (-not $SkipBuild) {
  $msbuild = Find-MSBuild
  Write-Host "Building with: $msbuild" -ForegroundColor Cyan
  & $msbuild "EmperorLauncher.sln" /t:Build /p:Configuration=Release /p:Platform=x86 /m /nologo
  if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)." }
}

# --- collect build output ---------------------------------------------------
$exe = Join-Path $root "Release\EmperorReborn.exe"
$dll = Join-Path $root "Release\EmperorHooks.dll"
foreach ($f in @($exe, $dll)) {
  if (-not (Test-Path $f)) { throw "Missing build output: $f" }
}

# --- copyright safety net ---------------------------------------------------
# A clean launcher is a few hundred KB. If the EA v1.09 patch (~5.8 MB) got
# embedded, the exe balloons past ~1 MB - refuse to package that.
$exeSizeMB = (Get-Item $exe).Length / 1MB
if ($exeSizeMB -gt 1.0) {
  throw ("EmperorReborn.exe is {0:N1} MB - that is too big. The EA patch (EM109EN.EXE) " -f $exeSizeMB) +
        "is probably embedded. Remove the PATCH109EN line from EmperorLauncher\Resources.rc and rebuild. " +
        "Refusing to package copyrighted EA content."
}
Write-Host ("EmperorReborn.exe is {0:N0} KB - clean, no embedded EA patch." -f ((Get-Item $exe).Length / 1KB)) -ForegroundColor Green

# --- assemble staging folder ------------------------------------------------
$stageName = "EmperorReborn-v$Version"
$distDir   = Join-Path $root "dist"
$stageDir  = Join-Path $distDir $stageName
$zipPath   = Join-Path $distDir "$stageName.zip"

if (Test-Path $stageDir) { Remove-Item $stageDir -Recurse -Force }
if (Test-Path $zipPath)  { Remove-Item $zipPath -Force }
New-Item -ItemType Directory -Path $stageDir -Force | Out-Null

Copy-Item $exe $stageDir
Copy-Item $dll $stageDir
Copy-Item (Join-Path $root "INSTALL.txt")              $stageDir
Copy-Item (Join-Path $root "license.txt")              $stageDir
Copy-Item (Join-Path $root "THIRD-PARTY-NOTICES.txt")  $stageDir
Copy-Item (Join-Path $root "readme.md") (Join-Path $stageDir "README.txt")

# --- zip --------------------------------------------------------------------
Compress-Archive -Path (Join-Path $stageDir "*") -DestinationPath $zipPath -Force

Write-Host ""
Write-Host "Packaged: $zipPath" -ForegroundColor Green
Get-ChildItem $stageDir | Select-Object Name, @{N="KB";E={[math]::Round($_.Length/1KB,1)}} | Format-Table -AutoSize
Write-Host "Reminder: EM109EN.EXE is NOT bundled - end users supply their own." -ForegroundColor Yellow
