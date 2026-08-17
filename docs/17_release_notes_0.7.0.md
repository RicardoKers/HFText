# HFText 0.7.0 Release Notes

HFText 0.7.0 is an experimental field-test release for Windows and Android.

## Highlights

- Continuous RX uses a substantially faster tone-analysis path while preserving
  the existing demodulation decisions and weak-signal behavior.
- Long Fast and Slow sessions on a second-generation Core i5 remained ahead of
  real time with no sample drops; observed process CPU fell from about 43% to
  23-26% in the documented field tests.
- On PC, Enter sends the current message, Shift+Enter inserts a newline, and
  Up/Down recall messages transmitted during the current session.
- PC and Android can highlight RX messages from a callsign selected from senders
  already present in the local history. Other RX traffic remains visible and TX
  messages keep their normal appearance.
- Android saved-evidence level statistics now summarize the complete saved
  audio window instead of only the most recent live block.

## Compatibility

- Protocol: HFText Basic v0.1.
- Text codec: Text Codec v0.2.
- Default Fast profile: experimental 8-FSK, 0.100 s/audio symbol.
- Default Slow profile: experimental 8-FSK, 0.300 s/audio symbol.
- HFText 0.7.0 keeps the same on-air protocol and text encoding as HFText 0.6.0.

Use the same speed profile at both ends. Automatic receive-speed detection is
not implemented in this release.

## Sender Highlight

On Windows, right-click the message history and open `Highlight sender`. On
Android, use the compact `Sender` button beside Clear. Both lists are populated
from callsigns found at the beginning of RX messages. Choose `All senders` to
remove the highlight.

The feature is a local display aid. It does not hide traffic and does not change
the transmitted payload or protocol.

## Artifacts

- `HFText-win64-0.7.0.zip`
- `HFText-android-0.7.0.apk`
- `HFText-0.7.0-SHA256.txt`

The Android APK is an experimental field-test build. Android may require the
operator to allow installation from the application used to open the APK.

## Known Limitations

- 8-FSK remains experimental.
- Fast/Slow must be selected manually at both ends.
- Windows loopback captures an output endpoint's full mix, not an individual
  process, and is unavailable on Android.
- HFText does not provide encryption, ACKs, automatic retries, or guaranteed
  emergency delivery.

## Suggested Smoke Test

1. Confirm the PC app, Android app, CLI tools, and evidence reports show version
   0.7.0.
2. Exchange one short Fast message and one short Slow message.
3. On PC, verify Enter sends, Shift+Enter inserts a newline, and Up recalls the
   previous transmitted message.
4. Receive messages from two callsigns and verify sender highlighting on PC and
   Android, then select `All senders`.
5. Save RX evidence and confirm the report and WAV files are created.
