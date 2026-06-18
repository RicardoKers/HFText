param(
    [switch]$SkipEnvironmentCheck
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$androidDir = Join-Path $repoRoot "android-app"
$gradlew = Join-Path $androidDir "gradlew.bat"

if (-not (Test-Path $gradlew)) {
    throw "Gradle wrapper not found: $gradlew"
}

if (-not $env:JAVA_HOME) {
    $androidStudioJbr = "C:\Program Files\Android\Android Studio\jbr"
    if (Test-Path (Join-Path $androidStudioJbr "bin\java.exe")) {
        $env:JAVA_HOME = $androidStudioJbr
    }
}

if (-not $env:ANDROID_HOME) {
    $defaultSdk = Join-Path $env:LOCALAPPDATA "Android\Sdk"
    if (Test-Path $defaultSdk) {
        $env:ANDROID_HOME = $defaultSdk
    }
}

if ($env:ANDROID_HOME) {
    $env:ANDROID_SDK_ROOT = $env:ANDROID_HOME
}

if ($env:JAVA_HOME) {
    $env:Path = "$(Join-Path $env:JAVA_HOME 'bin');$env:Path"
}
if ($env:ANDROID_HOME) {
    $env:Path = "$(Join-Path $env:ANDROID_HOME 'platform-tools');$env:Path"
}

if (-not $SkipEnvironmentCheck) {
    & (Join-Path $PSScriptRoot "check_android_environment.ps1") -Strict
}

Push-Location $androidDir
try {
    & $gradlew assembleDebug --no-daemon
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
} finally {
    Pop-Location
}
