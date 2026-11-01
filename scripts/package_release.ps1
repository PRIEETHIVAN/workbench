param(
  [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,31}$')]
  [string]$CandidateId = '',
  [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\build-release16'),
  [switch]$StageOnly,
  [switch]$RefreshStage
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildRoot = (Resolve-Path $BuildDirectory).Path
$distRoot = Join-Path $projectRoot 'dist'
$stage = Join-Path $distRoot 'UnifiedEngineeringWorkbench'

if (-not $StageOnly -and [string]::IsNullOrWhiteSpace($CandidateId)) {
  throw 'Supply the real CandidateId to create the ZIP, or use -StageOnly to prepare the review folder.'
}
if ($CandidateId -and $CandidateId -notmatch '^[A-Za-z0-9][A-Za-z0-9_-]{0,31}$') {
  throw 'CandidateId must be 1–32 letters, digits, underscores, or hyphens and start with a letter or digit.'
}
if (-not (Test-Path (Join-Path $buildRoot 'Launch.exe'))) { throw "Missing release executable in $buildRoot" }
$zip = if ($CandidateId) { Join-Path $distRoot "${CandidateId}_Workbench.zip" } else { $null }
if ($zip -and (Test-Path $zip)) { throw "Refusing to overwrite existing archive: $zip" }
if ($RefreshStage -and (Test-Path $stage)) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Force $distRoot | Out-Null

if (-not (Test-Path $stage)) {
  New-Item -ItemType Directory -Force $stage | Out-Null
  try {
    Copy-Item (Join-Path $buildRoot 'Launch.exe') $stage
    foreach ($dir in @('frontend','data')) {
      $source = Join-Path $buildRoot $dir
      if (-not (Test-Path $source)) { throw "Build output is missing $dir" }
      Copy-Item $source (Join-Path $stage $dir) -Recurse
    }
    foreach ($file in @('CMakeLists.txt','README.md','AI_LOG.md','DEPENDENCIES.md','VERSION_CONTROL.md','history.bundle','.gitignore')) {
      $source = Join-Path $projectRoot $file
      if (Test-Path $source) { Copy-Item $source $stage }
    }
    foreach ($dir in @('src','tests','docs','results','.vscode','scripts')) {
      Copy-Item (Join-Path $projectRoot $dir) (Join-Path $stage $dir) -Recurse
    }
  } catch {
    Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
    throw
  }
}

if ($StageOnly) {
  Write-Output "Prepared review folder $stage"
  return
}

Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
Write-Output "Packaged $zip"
