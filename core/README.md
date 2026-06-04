# HFText Core

Nucleo C++ portavel do modem HFText.

## Build

No Windows com Visual Studio Build Tools:

```powershell
cmake -S core -B core\build
cmake --build core\build
ctest --test-dir core\build -C Debug --output-on-failure
```

O argumento `-C Debug` e necessario com geradores multi-config, como Visual Studio.

No MSVC, o CMake do core usa informacao de debug embutida para evitar arquivos PDB de compilacao travados durante builds locais.
Se uma pasta de build antiga ficar presa por algum processo, use outro diretorio de build, por exemplo:

```powershell
cmake -S core -B core\build-msvc
cmake --build core\build-msvc
ctest --test-dir core\build-msvc -C Debug --output-on-failure
```

## CLI WAV

Gerar WAV:

```powershell
core\build-msvc\Debug\hftext_tx_wav.exe --callsign pu5lrk "Teste" python-sim\generated\cpp_tx.wav
```

Decodificar WAV:

```powershell
core\build-msvc\Debug\hftext_rx_wav.exe python-sim\generated\cpp_tx.wav
```

Decodificar WAV com diagnostico de sincronismo:

```powershell
core\build-msvc\Debug\hftext_rx_wav.exe --verbose python-sim\generated\cpp_tx.wav
```

O CTest do core tambem executa um round-trip automatico `hftext_tx_wav` -> `hftext_rx_wav`.

Os CLIs usam sempre o modo robusto atual: frame logico HFText v0.1, codigo convolucional `conv_k3`, interleaving deterministico e 2-FSK. Nao ha opcao para transmitir ou receber sem FEC/interleaving.
