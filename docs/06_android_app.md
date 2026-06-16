# Android Application

## Status

The Android application is a future phase. Development should continue on Python, C++ core, CLI tools, and the PC app until the protocol and receiver behavior are stable enough.

## Goal

The Android app should let an operator send and receive HFText messages using a phone or tablet audio interface connected to a radio.

## Planned Architecture

```text
Jetpack Compose UI
        |
Kotlin controller layer
        |
AudioTrack / AudioRecord
        |
JNI bridge
        |
Portable C++ core
```

## Requirements

- Reuse the C++ core.
- Keep the same protocol and modem settings as the PC application.
- Support direct audio TX after explicit operator action.
- Support continuous RX without unbounded memory growth.
- Provide a compact operation screen and a separate settings/debug area.
- Export logs or evidence in a format compatible with the PC-side analysis tools when practical.

## Not Planned Yet

- Android-specific protocol changes.
- Automatic transmission without operator action.
- Encryption.
- Starting Android before the PC/core behavior is mature.
