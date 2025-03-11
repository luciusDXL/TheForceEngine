
# Setup TheForceEngine directory. Place the executable here to run your test

# If you do not, then it will guess where the executable is.
$root_path = ""

# Fidn the specific build. Assume path is two layers above the script and in x64\DEBUG or x64\Release
$build="RELEASE"

# Enable strict mode for better error handling
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Function to get the full path of the script directory
function Get-ScriptDirectory {
    return Split-Path -Parent $PSCommandPath
}

# If unset, try to copy from x64 release/debug folder during build and copy it to a standard place.
if (-not $root_path -or -not (Test-Path "$root_path/TheForceEngine.exe")) {
  $root_path = Join-Path (Get-ScriptDirectory) "..\..\x64\$build\"
  Write-Host $root_path
  if (Test-Path "$root_path\TheForceEngine.exe") {
     $destPath = Join-Path (Get-ScriptDirectory) "..\"
     Copy-Item -Path "$root_path\TheForceEngine.exe" -Destination $destPath
     Write-Host "Found TheForceEngine.exe in path $root_path - copied to $destPath"
     $root_path = $destPath
  } else {
     Write-Host "ERROR: Cannot find TheForceEngine.exe. Please set it in the script"
     exit 1               
  }
}

# Directory where TFE logs are kept
$user_doc_path = ""

if (-not $user_doc_path -or -not (Test-Path $user_doc_path -PathType Container)) {
    $user_doc_path = "$env:USERPROFILE\Documents\TheForceEngine"
    if (Test-Path $user_doc_path -PathType Container) {
        Write-Host "Found the user directory in $user_doc_path"
    } else {
        Write-Host "WARNING: Cannot find the user TFE directory $user_doc_path. Hopefully, it will be created by the executable."
    }
}

# Path to the demo file
$demo_path = ""

if (-not $demo_path -or -not (Test-Path $demo_path)) {
    $demo_path = Join-Path (Get-ScriptDirectory) "Replays\base_test.demo"
    if (Test-Path $demo_path) {
        Write-Host "Found the demo at $demo_path"
    } else {
        Write-Host "ERROR: Cannot find the demo file $demo_path. It should be in the TheForceEngine\Tests\Replays folder"
        exit 1
    }
}

# Path to the demo log file
$demo_log_path = ""

if (-not $demo_log_path -or -not (Test-Path $demo_log_path)) {
    $demo_log_path = Join-Path (Get-ScriptDirectory) "Replays\base_replay_log.txt"
    if (Test-Path $demo_log_path) {
        Write-Host "Found the demo log at $demo_log_path"
    } else {
        Write-Host "ERROR: Cannot find the demo log file $demo_log_path. It should be in the TheForceEngine\Tests\Replays folder"
        exit 1
    }
}

# Run the demo and generate a new log file
Push-Location $root_path
Write-Host "Running The Force Engine..."
Write-Host "Executing Command $root_path\TheForceEngine.exe -gDark -r$demo_path --demo_logging --exit_after_replay ..."

$proc = Start-Process -FilePath "$root_path\TheForceEngine.exe" -ArgumentList "-gDark -r`"$demo_path`" --demo_logging --exit_after_replay" -NoNewWindow -Wait -PassThru
$exitCode =  $($proc.ExitCode)
Write-Host "Done running The Force Engine. Result is $exitCode"

if ($exitCode -ne 0) {
    Write-Host "ERROR: TFE failed to run. Please look at the logs hopefully generated in $user_doc_path"
    exit 1
}

# Diff Log Results of the test replay.log and the base one.
$replayLog = Join-Path $user_doc_path "replay.log"

if (-not (Test-Path $replayLog)) {
    Write-Host "ERROR: Missing $replayLog - No log generated?"
    exit 1
}

if (-not (Test-Path $demo_log_path)) {
    Write-Host "ERROR: Missing $demo_log_path - did you not checkout the latest code?"
    exit 1
}

# Compare last lines of both log files
$lastLine1 = Get-Content -Path $replayLog | Select-Object -Last 1
$lastLine2 = Get-Content -Path $demo_log_path | Select-Object -Last 1

if ($lastLine1 -eq $lastLine2) {
    Write-Host "TEST SUCCEEDED RESULTS MATCH!"
    exit 0
} else {
    Write-Host "ERROR: RESULTS DO NOT MATCH!"
    Write-Host "Expected: $lastLine2"
    Write-Host "Got: $lastLine1"
    exit 1
}
