[CmdletBinding()]
param(
    [switch]$Strict
)

$ErrorActionPreference = "Stop"

function Get-RepositoryRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

function Get-CommandSource {
    param([Parameter(Mandatory = $true)][string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $command) {
        return $null
    }
    return $command.Source
}

function Add-CheckResult {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyCollection()][System.Collections.Generic.List[object]]$Results,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Status,
        [Parameter(Mandatory = $true)][string]$Detail,
        [bool]$Required = $true
    )

    $Results.Add([pscustomobject]@{
        Name = $Name
        Required = if ($Required) { "yes" } else { "no" }
        Status = $Status
        Detail = $Detail
    }) | Out-Null
}

function Resolve-ExistingPath {
    param([string[]]$Candidates)

    foreach ($candidate in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    return $null
}

function Get-AndroidSourceProperty {
    param(
        [Parameter(Mandatory = $true)][string]$Directory,
        [Parameter(Mandatory = $true)][string]$PropertyName
    )

    $sourceProperties = Join-Path $Directory "source.properties"
    if (-not (Test-Path -LiteralPath $sourceProperties)) {
        return $null
    }

    foreach ($line in Get-Content -LiteralPath $sourceProperties) {
        if ($line -match "^$([regex]::Escape($PropertyName))\s*=\s*(.+)$") {
            return $Matches[1].Trim()
        }
    }
    return $null
}

function Format-PathWithRevision {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $Path
    }

    $revision = Get-AndroidSourceProperty -Directory $Path -PropertyName "Pkg.Revision"
    if ([string]::IsNullOrWhiteSpace($revision)) {
        return $Path
    }
    return "$Path (revision $revision)"
}

function Get-AndroidSdkCandidates {
    $candidates = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($env:ANDROID_HOME)) {
        $candidates.Add($env:ANDROID_HOME)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:ANDROID_SDK_ROOT)) {
        $candidates.Add($env:ANDROID_SDK_ROOT)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        $candidates.Add((Join-Path $env:LOCALAPPDATA "Android\Sdk"))
    }
    if (-not [string]::IsNullOrWhiteSpace($env:USERPROFILE)) {
        $candidates.Add((Join-Path $env:USERPROFILE "AppData\Local\Android\Sdk"))
    }
    return $candidates | Select-Object -Unique
}

function Find-SdkTool {
    param(
        [string]$SdkRoot,
        [string[]]$RelativeCandidates
    )

    if ([string]::IsNullOrWhiteSpace($SdkRoot)) {
        return $null
    }

    $candidates = foreach ($relative in $RelativeCandidates) {
        Join-Path $SdkRoot $relative
    }
    return Resolve-ExistingPath -Candidates $candidates
}

function Find-SdkVersionedDirectory {
    param(
        [string]$SdkRoot,
        [string]$RelativeRoot,
        [string]$ProbeFile
    )

    if ([string]::IsNullOrWhiteSpace($SdkRoot)) {
        return $null
    }

    $root = Join-Path $SdkRoot $RelativeRoot
    if (-not (Test-Path -LiteralPath $root)) {
        return $null
    }

    $directories = Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending

    foreach ($directory in $directories) {
        $probe = Join-Path $directory.FullName $ProbeFile
        if (Test-Path -LiteralPath $probe) {
            return $directory.FullName
        }
    }
    return $null
}

function Find-AndroidStudio {
    $candidates = @(
        "C:\Program Files\Android\Android Studio\bin\studio64.exe",
        "C:\Program Files\Android\Android Studio\bin\studio.exe"
    )
    return Resolve-ExistingPath -Candidates $candidates
}

function Find-JavaRuntime {
    $java = Get-CommandSource -Name "java"
    if ($null -ne $java) {
        return $java
    }
    if (-not [string]::IsNullOrWhiteSpace($env:JAVA_HOME)) {
        $candidate = Join-Path $env:JAVA_HOME "bin\java.exe"
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    $studioJava = "C:\Program Files\Android\Android Studio\jbr\bin\java.exe"
    if (Test-Path -LiteralPath $studioJava) {
        return (Resolve-Path -LiteralPath $studioJava).Path
    }
    return $null
}

function Get-JavaVersionText {
    param([string]$JavaPath)

    if ([string]::IsNullOrWhiteSpace($JavaPath)) {
        return $null
    }

    try {
        $quotedJavaPath = '"' + $JavaPath + '"'
        $versionOutput = & cmd.exe /d /c "$quotedJavaPath -version 2>&1"
        if ($versionOutput.Count -gt 0) {
            return [string]$versionOutput[0]
        }
    } catch {
        return $null
    }
    return $null
}

function Get-JavaMajorVersion {
    param([string]$VersionText)

    if ([string]::IsNullOrWhiteSpace($VersionText)) {
        return $null
    }

    if ($VersionText -match '"1\.(\d+)') {
        return [int]$Matches[1]
    }
    if ($VersionText -match '"(\d+)') {
        return [int]$Matches[1]
    }
    return $null
}

function Write-AndroidSetupHints {
    Write-Host ""
    Write-Host "Suggested next steps:"
    Write-Host "1. Install Android Studio."
    Write-Host "2. Open Android Studio > SDK Manager."
    Write-Host "3. Install Android SDK Platform-Tools."
    Write-Host "4. Install Android SDK Command-line Tools."
    Write-Host "5. Install NDK (Side by side)."
    Write-Host "6. Install CMake from the Android SDK tools list."
    Write-Host "7. Re-run .\scripts\check_android_environment.ps1."
}

$repoRoot = Get-RepositoryRoot
$results = New-Object System.Collections.Generic.List[object]

$androidStudio = Find-AndroidStudio
if ($null -eq $androidStudio) {
    Add-CheckResult $results "Android Studio" "missing" "Not found in the default Windows install path." $false
} else {
    Add-CheckResult $results "Android Studio" "ok" $androidStudio $false
}

$sdkRoot = Resolve-ExistingPath -Candidates (Get-AndroidSdkCandidates)
if ($null -eq $sdkRoot) {
    Add-CheckResult $results "Android SDK" "missing" "Set ANDROID_HOME or install the SDK under AppData\Local\Android\Sdk." $true
} else {
    Add-CheckResult $results "Android SDK" "ok" $sdkRoot $true
}

$adb = Find-SdkTool -SdkRoot $sdkRoot -RelativeCandidates @("platform-tools\adb.exe")
if ($null -eq $adb) {
    Add-CheckResult $results "Android platform-tools" "missing" "Install Android SDK Platform-Tools." $true
} else {
    Add-CheckResult $results "Android platform-tools" "ok" $adb $true
}

$sdkManager = Find-SdkTool -SdkRoot $sdkRoot -RelativeCandidates @(
    "cmdline-tools\latest\bin\sdkmanager.bat",
    "cmdline-tools\latest\bin\sdkmanager",
    "tools\bin\sdkmanager.bat",
    "tools\bin\sdkmanager"
)
if ($null -eq $sdkManager) {
    Add-CheckResult $results "Android command-line tools" "missing" "Install Android SDK Command-line Tools." $true
} else {
    Add-CheckResult $results "Android command-line tools" "ok" $sdkManager $true
}

$ndk = Find-SdkVersionedDirectory -SdkRoot $sdkRoot -RelativeRoot "ndk" -ProbeFile "source.properties"
if ($null -eq $ndk) {
    Add-CheckResult $results "Android NDK" "missing" "Install an Android SDK side-by-side NDK package." $true
} else {
    Add-CheckResult $results "Android NDK" "ok" (Format-PathWithRevision -Path $ndk) $true
}

$androidCMake = Find-SdkVersionedDirectory -SdkRoot $sdkRoot -RelativeRoot "cmake" -ProbeFile "bin\cmake.exe"
if ($null -eq $androidCMake) {
    Add-CheckResult $results "Android SDK CMake" "missing" "Install the Android SDK CMake package." $true
} else {
    Add-CheckResult $results "Android SDK CMake" "ok" (Format-PathWithRevision -Path $androidCMake) $true
}

$javaRuntime = Find-JavaRuntime
if ($null -eq $javaRuntime) {
    Add-CheckResult $results "Java runtime" "missing" "Install Android Studio or set JAVA_HOME." $true
} else {
    $javaVersionText = Get-JavaVersionText -JavaPath $javaRuntime
    $javaMajorVersion = Get-JavaMajorVersion -VersionText $javaVersionText
    $javaDetail = if ([string]::IsNullOrWhiteSpace($javaVersionText)) {
        $javaRuntime
    } else {
        "$javaRuntime ($javaVersionText)"
    }
    if ($null -ne $javaMajorVersion -and $javaMajorVersion -lt 17) {
        Add-CheckResult $results "Java runtime" "warning" "$javaDetail. Android builds normally require JDK 17 or newer; Android Studio includes a suitable runtime." $true
    } else {
        Add-CheckResult $results "Java runtime" "ok" $javaDetail $true
    }
}

Write-Host "HFText Android environment check"
Write-Host "Repository: $repoRoot"
Write-Host ""
$results | Format-Table -AutoSize -Wrap Name, Required, Status, Detail

$requiredProblems = @($results | Where-Object { $_.Required -eq "yes" -and $_.Status -ne "ok" })
if ($requiredProblems.Count -gt 0) {
    Write-Host ""
    Write-Warning "Android development checks needing attention: $($requiredProblems.Name -join ', ')"
    Write-Host "This is expected before Android setup. The PC app and C++ core can still be built normally."
    Write-AndroidSetupHints
    if ($Strict) {
        exit 1
    }
}

exit 0
