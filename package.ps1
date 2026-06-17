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
  [string]$Version = "2.5",
  [switch]$SkipBuild,
  # Optional toolset override. Local builds leave it empty and use the project's toolset; CI passes a
  # toolset that exists on the runner (e.g. v143 on windows-latest) since the dev machine uses a newer one.
  [string]$PlatformToolset = ""
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
  $msbuildArgs = @("EmperorLauncher.sln", "/t:Build", "/p:Configuration=Release", "/p:Platform=x86", "/m", "/nologo")
  if ($PlatformToolset) { $msbuildArgs += "/p:PlatformToolset=$PlatformToolset" }
  & $msbuild @msbuildArgs
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

# --- checksums of the contents ----------------------------------------------
# Hash every staged file and write SHA256SUMS.txt INTO the folder so it ships
# inside the zip - lets players verify the two executables after extracting.
$sumsPath = Join-Path $stageDir "SHA256SUMS.txt"
Get-ChildItem $stageDir -File | Sort-Object Name | ForEach-Object {
  '{0}  {1}' -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower(), $_.Name
} | Set-Content -Path $sumsPath -Encoding ascii

# --- zip --------------------------------------------------------------------
Compress-Archive -Path (Join-Path $stageDir "*") -DestinationPath $zipPath -Force

# --- checksum of the zip itself (the number to publish on the site) ---------
$zipHash = (Get-FileHash $zipPath -Algorithm SHA256).Hash.ToLower()
'{0}  {1}' -f $zipHash, "$stageName.zip" | Set-Content -Path "$zipPath.sha256" -Encoding ascii
$exeHash = (Get-FileHash (Join-Path $stageDir "EmperorReborn.exe") -Algorithm SHA256).Hash.ToLower()
$dllHash = (Get-FileHash (Join-Path $stageDir "EmperorHooks.dll")  -Algorithm SHA256).Hash.ToLower()

Write-Host ""
Write-Host "Packaged: $zipPath" -ForegroundColor Green
Get-ChildItem $stageDir | Select-Object Name, @{N="KB";E={[math]::Round($_.Length/1KB,1)}} | Format-Table -AutoSize
Write-Host "SHA256 - publish these on the site and in the release notes:" -ForegroundColor Cyan
Write-Host ("  zip                {0}" -f $zipHash)
Write-Host ("  EmperorReborn.exe  {0}" -f $exeHash)
Write-Host ("  EmperorHooks.dll   {0}" -f $dllHash)
Write-Host "Reminder: EM109EN.EXE is NOT bundled - end users supply their own." -ForegroundColor Yellow
