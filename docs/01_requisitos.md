# Requirements

## Product Requirements

HFText must allow an operator to send and receive short text messages through an HF radio audio channel.

The system must:

- transmit only after an explicit operator action;
- receive continuously for long sessions without unbounded memory growth;
- display decoded messages clearly;
- make invalid transmitted characters visible by replacing them with `?`;
- include the configured callsign automatically at the beginning of the payload when present;
- provide enough diagnostics for field testing without overwhelming normal operation;
- keep WAV-based transmit/receive tools available for debugging.

## Protocol Requirements

- Use HFText Basic v0.1 as the operational baseline.
- Preserve the logical frame `SYNC | LENGTH | PAYLOAD | CRC16`.
- Keep `LENGTH` in the 0 to 127 range and define it as the number of encoded 6-bit payload symbols.
- Keep the callsign inside `PAYLOAD`, not as a separate protocol field.
- Calculate CRC-16/CCITT-FALSE over the packed payload only.
- Exclude `SYNC`, `LENGTH`, `START_SYNC`, and `PHYS_LENGTH` from the CRC.
- Use the robust layer with convolutional coding and interleaving for normal operation.
- Treat 4-FSK and 8-FSK as experimental physical modulation modes until enough field evidence supports promotion.

## Text Requirements

- The supported alphabet is 64 symbols.
- Lowercase letters are direct symbols.
- Uppercase letters use `shift + lowercase`.
- Acute and tilde modifiers encode supported accented vowels.
- `ç` is a direct symbol; `Ç` uses `shift + ç`.
- Unsupported input characters must become `?`.
- The app must show the sanitized transmit text before transmission.

## DSP Requirements

- Use normalized floating-point samples in the core.
- Use non-coherent tone energy detection.
- Support a configurable sample rate, symbol duration, base frequency, tone spacing, amplitude, and preamble length.
- Keep all derived tones below Nyquist.
- Prefer robust detection over speed, especially for real HF/SDR captures.
- Do not perform long offline decoding passes during normal continuous RX.

## PC Application Requirements

- The interface language must be English.
- Normal operation should resemble a compact chat-style interface.
- RX should start automatically when an input device is available.
- TX should be direct through the sound card; saving WAV first must not be required.
- The same send button may cancel TX while audio is playing.
- A Fast/Slow speed selector should be available in the Operation tab.
- Advanced modem parameters should live in an editable `hftext.ini` file instead of cluttering normal operation.
- Settings and logs belong in a separate Settings tab.
- Waterfall markers, logs, and evidence export must help the operator tune and debug without overwhelming normal operation.
- Evidence export must include a WAV capture and a text report with settings, logs, summary CSV, and accepted-frame CSV.

## Non-Functional Requirements

- Keep the C++ core portable and UI-independent.
- Keep tests close to every core behavior change.
- Keep documentation synchronized with protocol and UI changes.
- Prefer clear, conservative implementation over clever shortcuts.
- Do not add cryptography.
