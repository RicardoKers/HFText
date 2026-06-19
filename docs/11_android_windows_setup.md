# Android Windows Setup

This guide is for the Android phase. The Android app is still incremental, and the PC/core workflow remains the reference implementation for modem behavior.

Use this guide to install or verify Android development tools.

## First Check

From the repository root, run:

```powershell
.\scripts\check_android_environment.ps1
```

The script only reports installed tools. It does not install or change anything.

Expected before setup:

- Android Studio may be missing.
- Android SDK may be missing.
- SDK Platform-Tools may be missing.
- SDK Command-line Tools may be missing.
- NDK may be missing.
- Android SDK CMake may be missing.
- Java may be present but too old for Android builds.

## Install Android Studio

Install Android Studio for Windows from the official Android developer site.

Recommended installation path:

```text
C:\Program Files\Android\Android Studio
```

Android Studio normally includes a suitable JDK runtime. Prefer that runtime over an old standalone Java 8 installation.

## Install SDK Components

Open Android Studio and then open SDK Manager.

Install these components:

- Android SDK Platform-Tools;
- Android SDK Command-line Tools;
- NDK (Side by side);
- CMake from the Android SDK tools list.

The Android SDK is usually installed under:

```text
%LOCALAPPDATA%\Android\Sdk
```

The environment check also accepts SDK paths from:

- `ANDROID_HOME`;
- `ANDROID_SDK_ROOT`;
- `%LOCALAPPDATA%\Android\Sdk`;
- `%USERPROFILE%\AppData\Local\Android\Sdk`.

## Recheck

After installing components, open a new PowerShell terminal and run:

```powershell
.\scripts\check_android_environment.ps1
```

When missing Android tools should fail validation, use:

```powershell
.\scripts\check_android_environment.ps1 -Strict
```

Strict mode exits with a non-zero status if required Android tools are missing or problematic.

Build the Android debug shell from the repository root:

```powershell
.\scripts\build_android_debug.ps1
```

The build script uses the Android Studio bundled JBR and the default SDK path when
`JAVA_HOME` or `ANDROID_HOME` are not set.

## What Not To Do Yet

Do not duplicate HFText modem behavior in Kotlin.

Android should call the portable C ABI in:

```text
core/include/hftext_c_api.h
```

The Android app should reuse the C++ core for:

- protocol metadata;
- Fast/Slow modem profiles;
- text preparation and payload length checks;
- tone lists;
- audio statistics;
- TX audio generation;
- streaming RX.

## Troubleshooting

If the script still reports missing tools after installation:

- close and reopen the terminal;
- confirm Android Studio can open SDK Manager;
- confirm the SDK path exists;
- set `ANDROID_HOME` to the SDK path if the SDK is installed in a non-default location;
- rerun the check script.

If Java is reported as old, use the runtime bundled with Android Studio instead of an older standalone Java installation.
