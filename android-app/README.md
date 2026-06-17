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
