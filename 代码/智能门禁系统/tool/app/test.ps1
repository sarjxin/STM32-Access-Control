$ErrorActionPreference = 'Stop'
. 'D:\app\android\env.ps1'
$taskTestOutput = Join-Path $PSScriptRoot 'test-output'
New-Item -ItemType Directory -Path $taskTestOutput -Force | Out-Null
$taskSourceRoot = Join-Path $PSScriptRoot 'app\src\main\java\cn\dooraccess\app'
& "$env:JAVA_HOME\bin\javac.exe" -encoding UTF-8 -d $taskTestOutput (Join-Path $taskSourceRoot 'DoorProtocol.java') (Join-Path $taskSourceRoot 'CommandTracker.java') (Join-Path $PSScriptRoot 'tests\ProtocolTests.java')
if ($LASTEXITCODE -ne 0) { throw 'Protocol test compilation failed' }
& "$env:JAVA_HOME\bin\java.exe" -cp $taskTestOutput ProtocolTests
if ($LASTEXITCODE -ne 0) { throw 'Protocol tests failed' }
