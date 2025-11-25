param(
  [string]$Port = "",
  [switch]$EraseFlash = $false
)

Write-Host "== UIperso validation (PowerShell) =="

if (-not (Test-Path "CMakeLists.txt")) { throw "Run from repo root (CMakeLists.txt not found)" }

idf.py set-target esp32s3

idf.py fullclean
idf.py build
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

if ($Port -ne "") {
  if ($EraseFlash) {
    idf.py -p $Port erase_flash
    if ($LASTEXITCODE -ne 0) { throw "erase_flash failed" }
  }
  idf.py -p $Port flash
  if ($LASTEXITCODE -ne 0) { throw "flash failed" }
  idf.py -p $Port monitor
} else {
  Write-Host "No port provided. Flash/monitor skipped."
  Write-Host "Use: .\agent\validate.ps1 -Port COM3"
}

Write-Host "Validation OK."
