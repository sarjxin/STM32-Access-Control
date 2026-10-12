param([switch]$SkipLint)
$ErrorActionPreference = 'Stop'
$taskEnvironment = 'D:\app\android\env.ps1'
if (-not (Test-Path -LiteralPath $taskEnvironment)) { throw 'Android environment not found at D:\app\android' }
. $taskEnvironment
$taskAppRoot = $PSScriptRoot
$taskLogDirectory = Join-Path $taskAppRoot 'output\logs'
New-Item -ItemType Directory -Path $taskLogDirectory -Force | Out-Null
& (Join-Path $taskAppRoot 'test.ps1')
$taskTasks = @('assembleDebug')
if (-not $SkipLint) { $taskTasks += 'lintDebug' }
# Merge stderr inside CMD so Windows PowerShell 5.1 doesn't label normal JVM messages as errors.
$taskGradleCommand = '""D:\app\android\gradle\bin\gradle.bat" -p "{0}" --no-daemon --console=plain {1} 2>&1"' -f $taskAppRoot, ($taskTasks -join ' ')
& $env:ComSpec /d /s /c $taskGradleCommand | Tee-Object -FilePath (Join-Path $taskLogDirectory 'build.log')
$taskBuildExit = $LASTEXITCODE
if ($taskBuildExit -ne 0) { throw 'Android build or lint failed. See output\logs\build.log' }
$taskApk = Join-Path $taskAppRoot 'app\build\outputs\apk\debug\app-debug.apk'
$taskSignatureCommand = '""D:\app\android\sdk\build-tools\35.0.0\apksigner.bat" verify --verbose "{0}" 2>&1"' -f $taskApk
& $env:ComSpec /d /s /c $taskSignatureCommand | Tee-Object -FilePath (Join-Path $taskLogDirectory 'signature.log')
$taskSignatureExit = $LASTEXITCODE
if ($taskSignatureExit -ne 0) { throw 'APK signature verification failed' }
$taskOutput = Join-Path $taskAppRoot 'output\DoorAccess-v1.2.apk'
Copy-Item -LiteralPath $taskApk -Destination $taskOutput -Force
$taskHash = (Get-FileHash -LiteralPath $taskOutput -Algorithm SHA256).Hash
Set-Content -LiteralPath (Join-Path $taskAppRoot 'output\DoorAccess-v1.2.apk.sha256') -Value $taskHash -Encoding ascii
Write-Host "APK ready: $taskOutput"
Write-Host "SHA256: $taskHash"
