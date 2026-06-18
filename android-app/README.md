# HFText Android App

This directory contains the first Android shell for HFText.

Current status:

- Gradle wrapper project;
- Kotlin source enabled through Android Gradle Plugin built-in Kotlin support;
- Jetpack Compose UI shell;
- no JNI bridge yet;
- no audio TX/RX yet;
- no modem logic duplicated in Kotlin.

Android development should continue to use:

- Kotlin and Jetpack Compose for the UI;
- `AudioTrack` for TX audio;
- `AudioRecord` for RX audio;
- JNI as a narrow bridge to the portable C ABI in `core/include/hftext_c_api.h`;
- the existing C++ core for modem settings, TX helpers, tone lists, audio statistics, and streaming RX.

Do not duplicate modem protocol, text encoding, FEC, modulation, or receiver logic in Kotlin.

The C ABI usage contract for future JNI work is documented in:

```text
docs/12_c_api_reference.md
```

## Build

From the repository root:

```powershell
.\scripts\build_android_debug.ps1
```

Or directly from this directory, when `JAVA_HOME` and `ANDROID_HOME` are already set:

```powershell
.\gradlew.bat assembleDebug
```

The generated debug APK is written under:

```text
android-app/app/build/outputs/apk/debug/
```

Open `android-app/` in Android Studio to run it on an emulator or device.

## Environment Check

From the repository root:

```powershell
.\scripts\check_android_environment.ps1
```

The script only reports installed tools, detected versions, and practical warnings. It does not install or modify anything.

For Windows installation steps, see:

```text
docs/11_android_windows_setup.md
```

Use strict mode when the Android build becomes part of validation:

```powershell
.\scripts\check_android_environment.ps1 -Strict
```
