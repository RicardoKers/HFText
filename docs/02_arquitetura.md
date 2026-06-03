# Arquitetura do sistema

## Visão geral

O sistema deve ser dividido em quatro partes principais:

1. Simulação Python.
2. Núcleo DSP em C++.
3. Aplicação PC.
4. Aplicação Android.

## Diagrama lógico

```text
Mensagem
   ↓
Codificador de texto
   ↓
Montador de quadro
   ↓
CRC / FEC / Interleaving
   ↓
Modulador FSK/MFSK
   ↓
Áudio PCM
   ↓
Rádio HF
   ↓
Áudio recebido
   ↓
Demodulador FSK/MFSK
   ↓
Deinterleaving / FEC / CRC
   ↓
Decodificador de texto
   ↓
Mensagem recebida

Núcleo C++

O núcleo C++ deve conter:

core/
├── include/
│   ├── hftext_config.h
│   ├── hftext_encoder.h
│   ├── hftext_frame.h
│   ├── hftext_crc16.h
│   ├── hftext_modulator.h
│   ├── hftext_demodulator.h
│   └── hftext_result.h
│
├── src/
│   ├── encoder.cpp
│   ├── frame.cpp
│   ├── crc16.cpp
│   ├── modulator_fsk.cpp
│   ├── demodulator_fsk.cpp
│   └── goertzel.cpp
│
└── tests/
Interfaces principais sugeridas
struct ModemConfig {
    int sampleRate = 48000;
    float symbolDurationSec = 0.5f;
    float frequency0Hz = 1200.0f;
    float frequency1Hz = 1600.0f;
    float amplitude = 0.8f;
    int preambleBits = 64;
    bool syncSearch = true;
};

struct DecodeResult {
    bool frameDetected = false;
    bool crcOk = false;
    bool payloadValid = false;
    std::string text;
    std::string error;
    int length = 0;
    int syncIndex = -1;
    int startOffset = 0;
    int offsetsTried = 1;
    float confidence = 0.0f;
};

std::vector<float> modulateText(
    const std::string& text,
    const ModemConfig& config
);

DecodeResult demodulateSamples(
    const std::vector<float>& samples,
    const ModemConfig& config
);

class StreamingReceiver {
public:
    explicit StreamingReceiver(const ModemConfig& config);
    void reset();
    std::vector<DecodeResult> pushSamples(const std::vector<float>& samples);
};

Essas interfaces de alto nível devem ser a entrada preferencial para ferramentas CLI, aplicação PC e futura integração Android.
Quando `syncSearch` está habilitado, `demodulateSamples` pode tentar múltiplos offsets iniciais de amostra dentro de um símbolo antes de escolher o primeiro quadro com CRC e payload válidos.
`StreamingReceiver` é a base para recepção contínua: ele acumula blocos curtos de amostras, tenta recuperar quadros completos e emite resultados válidos sem depender de um WAV fechado.
As APIs internas de encoder, frame, modulador e demodulador continuam disponíveis para testes e validações de baixo nível.
Simulação Python

A simulação Python deve ser usada para experimentar rapidamente algoritmos.

Ela pode duplicar algoritmos temporariamente, mas a versão definitiva deve migrar para C++.

Aplicação PC

A aplicação PC deve usar o núcleo C++ diretamente.

Responsabilidades da aplicação PC:

interface gráfica;
seleção de dispositivos de áudio;
captura e reprodução;
visualização;
logs.

Não deve conter lógica DSP principal.

Aplicação Android

A aplicação Android deve usar:

Kotlin para interface;
AudioTrack para transmissão;
AudioRecord para recepção;
JNI para chamar o núcleo C++.
Dependências permitidas
Core C++

Permitidas:

STL;
CMake;
biblioteca de testes leve.

Evitar:

Qt;
Android SDK;
bibliotecas gráficas;
dependências de áudio.
PC

Permitidas:

Qt;
PortAudio, RtAudio ou API equivalente;
biblioteca para gráficos simples.
Python

Permitidas:

NumPy;
SciPy;
Matplotlib;
soundfile;
sounddevice.
Android

Permitidas:

Kotlin;
Jetpack Compose;
AudioTrack;
AudioRecord;
NDK;
JNI.
