# HFText 0.6.0 Release Notes

HFText 0.6.0 is an experimental field-test release for Windows and Android.

## Highlights

- Windows can receive audio directly from an output device through WASAPI
  loopback. This is useful when an SDR already plays audio on the same PC.
- Loopback RX preserves quiet periods and delivers fixed 100 ms blocks, keeping
  the waterfall, saved evidence, and streaming decoder on a real-time cadence.
- Windows lists physical recording inputs separately from output-loopback
  sources and remembers the selected endpoint.
- Android now applies Fast/Slow profile changes to the active receiver without
  requiring capture to be stopped and restarted manually.
- RX evidence timing is captured after the destination folder is selected, so
  saved-audio duration and reported session time refer to nearly the same point.

## Compatibility

- Protocol: HFText Basic v0.1.
- Text codec: Text Codec v0.2.
- Default Fast profile: experimental 8-FSK, 0.100 s/audio symbol.
- Default Slow profile: experimental 8-FSK, 0.300 s/audio symbol.
- HFText 0.6.0 keeps the same on-air protocol and text encoding as HFText 0.5.0.

Use the same speed profile at both ends. Using the same application release is
recommended during field tests so diagnostics and evidence have matching
behavior.

## Windows Loopback

Select `Loopback: <output device>` as `Audio input`, then select the endpoint
used by the SDR or other audio application. Loopback captures the complete mix,
including notifications and audio from unrelated applications.

Enable `Pause RX during TX` when HFText transmits through the same endpoint and
self-reception is not intended.

## Artifacts

- `HFText-win64-0.6.0.zip`
- `HFText-android-0.6.0.apk`

The Android APK is an experimental field-test build. Android may require the
operator to allow installation from the application used to open the APK.

## Known Limitations

- 8-FSK remains experimental.
- Loopback is available only in the Windows application.
- Loopback captures an output endpoint's full mix, not an individual process.
- HFText does not provide encryption, ACKs, automatic retries, or guaranteed
  emergency delivery.

## Suggested Smoke Test

1. Confirm the app and evidence report show version 0.6.0.
2. Exchange one short Fast message and one short Slow message.
3. On Windows, leave loopback RX quiet for at least 10 seconds and verify that
   the waterfall continues scrolling at its normal speed.
4. Send a message through the selected loopback endpoint and confirm it appears
   shortly after the frame ends.
5. Save RX evidence and verify that the WAV duration approximately matches the
   RX session duration.
