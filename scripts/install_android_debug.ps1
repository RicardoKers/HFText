param(
    [string]$DeviceId,
    [switch]$SkipBuild,
    [switch]$NoLaunch,
    [switch]$ListDevices
)

$ErrorActionPreference = "Stop"

function Get-AndroidSdkRoot {
    $candidates = @(
        $env:ANDROID_HOME,
        $env:ANDROID_SDK_ROOT,
        (Join-Path $env:LOCALAPPDATA "Android\Sdk"),
        (Join-Path $env:USERPROFILE "AppData\Local\Android\Sdk")
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }

    foreach ($candidate in $candidates | Select-Object -Unique) {
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    return $null
}

function Get-AdbPath {
    $adbCommand = Get-Command adb -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -ne $adbCommand) {
        return $adbCommand.Source
    }

    $sdkRoot = Get-AndroidSdkRoot
    if ($null -eq $sdkRoot) {
        return $null
    }

    $adb = Join-Path $sdkRoot "platform-tools\adb.exe"
    if (Test-Path -LiteralPath $adb) {
        return (Resolve-Path -LiteralPath $adb).Path
    }
    return $null
}

function Get-ConnectedAndroidDevices {
    param([Parameter(Mandatory = $true)][string]$AdbPath)

    $rawDevices = & $AdbPath devices
    if ($LASTEXITCODE -ne 0) {
        throw "adb devices failed."
    }

    $devices = New-Object System.Collections.Generic.List[object]
    foreach ($line in $rawDevices) {
        if ($line -match "^\s*([^\s]+)\s+(device|unauthorized|offline)\s*$") {
            $devices.Add([pscustomobject]@{
                Id = $Matches[1]
                State = $Matches[2]
            }) | Out-Null
        }
    }
    return $devices
}

function Select-AndroidDevice {
    param(
        [Parameter(Mandatory = $true)][object[]]$Devices,
        [string]$RequestedDeviceId
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedDeviceId)) {
        $selected = $Devices | Where-Object { $_.Id -eq $RequestedDeviceId } | Select-Object -First 1
        if ($null -eq $selected) {
            throw "Requested Android device was not found: $RequestedDeviceId"
        }
        if ($selected.State -ne "device") {
            throw "Requested Android device is $($selected.State): $RequestedDeviceId"
        }
        return $selected.Id
    }

    $readyDevices = @($Devices | Where-Object { $_.State -eq "device" })
    if ($readyDevices.Count -eq 0) {
        throw "No ready Android devices found. Connect a device, start an emulator, or run with -ListDevices."
    }
    if ($readyDevices.Count -gt 1) {
        $ids = ($readyDevices | ForEach-Object { $_.Id }) -join ", "
        throw "More than one Android device is ready. Re-run with -DeviceId. Ready devices: $ids"
    }
    return $readyDevices[0].Id
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$apkPath = Join-Path $repoRoot "android-app\app\build\outputs\apk\debug\app-debug.apk"
$packageName = "org.hftext.android"
$mainActivity = "$packageName/.MainActivity"

$adb = Get-AdbPath
if ($null -eq $adb) {
    throw "adb was not found. Install Android SDK Platform-Tools or set ANDROID_HOME."
}

$devices = @(Get-ConnectedAndroidDevices -AdbPath $adb)

if ($ListDevices) {
    if ($devices.Count -eq 0) {
        Write-Host "No Android devices reported by adb."
    } else {
        $devices | Format-Table -AutoSize Id, State
    }
    exit 0
}

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build_android_debug.ps1")
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if (-not (Test-Path -LiteralPath $apkPath)) {
    throw "Debug APK not found: $apkPath"
}

$selectedDevice = Select-AndroidDevice -Devices $devices -RequestedDeviceId $DeviceId

Write-Host "Installing HFText debug APK on $selectedDevice"
& $adb -s $selectedDevice install -r $apkPath
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (-not $NoLaunch) {
    Write-Host "Launching HFText on $selectedDevice"
    & $adb -s $selectedDevice shell am start -n $mainActivity
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
