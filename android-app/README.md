# HFText Android App

This directory is reserved for the future Android application.

The Android app has not started yet. The current project order remains:

1. Python simulation.
2. Portable C++ core.
3. CLI tools.
4. PC app.
5. Field validation.
6. Android app.

When Android work starts, it should use:

- Kotlin and Jetpack Compose for the UI;
- `AudioTrack` for TX audio;
- `AudioRecord` for RX audio;
- JNI as a narrow bridge to the portable C ABI in `core/include/hftext_c_api.h`;
- the existing C++ core for modem settings, TX helpers, tone lists, audio statistics, and streaming RX.

Do not duplicate modem protocol, text encoding, FEC, modulation, or receiver logic in Kotlin.

## Environment Check

Before creating the Android project, run this from the repository root:

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
