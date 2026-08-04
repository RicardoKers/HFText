<p align="center">
  <img src="docs/assets/hftext-icon.png" width="128" alt="HFText icon">
</p>

<h1 align="center">HFText</h1>

<p align="center"><strong>Short text messages over HF radio, carried as audio.</strong></p>

HFText turns a Windows PC or Android phone into a digital text modem for HF
radio. Type a short message, press Send, and HFText converts it into audio tones.
Another HFText device listens to the receiver audio and displays the decoded
message with a timestamp.

It can be used through a simple speaker-to-microphone path, an audio cable, or a
radio and remote SDR. No internet connection or user account is required.

> HFText is experimental software intended for testing and learning. It does not
> provide encryption and should not be relied on for emergency communication.

## Download

Ready-to-use test builds are available on the
[HFText Releases page](https://github.com/RicardoKers/HFText/releases/latest).

- **Windows:** download the Windows ZIP, extract it, and run `hftext_pc.exe`.
- **Android:** download the APK, install it, and grant microphone permission
  when requested.

Use the same HFText release at both ends of a link. Different releases may use
incompatible text or modem formats.

## Try It Without a Radio

The easiest first test uses a speaker and microphone:

1. Open HFText on two devices.
2. In Settings, replace the `nocall` placeholder with your callsign or test name.
3. Start RX on the receiving device. Windows normally starts RX automatically;
   Android provides `Start RX capture` in Settings.
4. Select the same speed, `Fast` or `Slow`, on both devices.
5. Place the receiving microphone near the transmitting speaker.
6. Type a short message such as `hello` and press Send.
7. The decoded message should appear in the receiving history.

When a device can hear its own speaker, enable `Pause RX during TX` in Settings
to prevent it from decoding its own transmission.

## Use It With a Radio

HFText works through the radio's normal audio path:

1. Feed the transmitting device audio into the radio data or microphone input.
2. Feed receiver or SDR audio into the receiving device audio input.
3. Select the same `Fast` or `Slow` profile at both ends.
4. Tune until the received tones align with the yellow markers in the waterfall.
5. Keep the signal visible without driving the input into the red clipping range.
6. Type the message and press Send.

Every transmission requires an explicit press of the Send button. Always follow
your local amateur-radio rules, band plan, and licensing requirements.

## What You See

- **Message history:** timestamped TX and RX messages in visually distinct
  bubbles.
- **Waterfall:** a live view of received audio between 300 Hz and 3 kHz, with
  markers for the expected modem tones.
- **Fast / Slow:** Fast occupies the channel for less time; Slow is more
  forgiving when conditions are difficult.
- **TX estimate:** the number of encoded symbols and expected transmission time.
- **Settings:** callsign, audio controls, RX control, local echo prevention, and
  test-evidence tools.

## Help Improve HFText

Real audio and radio tests are especially valuable. After a useful success or
failure, use `Save RX evidence` on the receiving device. The report and audio
capture help identify tuning, level, timing, and channel problems.

See the [Field Test Guide](docs/14_field_test_guide.md) for a short test procedure
and feedback template. Problems and observations can be reported through
[GitHub Issues](https://github.com/RicardoKers/HFText/issues).

Do not include private or sensitive communication in shared evidence files.

## Documentation

- [User Guide](docs/10_user_guide.md)
- [Field Test Guide](docs/14_field_test_guide.md)
- [Project Overview](docs/00_visao_geral.md)
- [PC Application](docs/05_pc_app.md)
- [Android Application](docs/06_android_app.md)
- [Modem Protocol](docs/03_protocolo_modem.md)
- [Text Codec](docs/13_text_codec_v02.md)
- [Architecture](docs/02_arquitetura.md)
- [Audio and DSP](docs/04_dsp_audio.md)
- [Validation](docs/08_testes_validacao.md)

Developers should also read [AGENTS.md](AGENTS.md) before changing the modem core
or protocol.
