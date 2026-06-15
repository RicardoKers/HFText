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
Se uma pasta de build ficar presa por algum processo, feche os executaveis abertos ou use outro diretorio temporario de build, por exemplo:

```powershell
cmake -S core -B core\build-temp
cmake --build core\build-temp
ctest --test-dir core\build-temp -C Debug --output-on-failure
```

## CLI WAV

Gerar WAV:

```powershell
core\build\Debug\hftext_tx_wav.exe --callsign pu5lrk "Teste" python-sim\generated\cpp_tx.wav
```

Decodificar WAV:

```powershell
core\build\Debug\hftext_rx_wav.exe python-sim\generated\cpp_tx.wav
```

Decodificar WAV com diagnostico de sincronismo:

```powershell
core\build\Debug\hftext_rx_wav.exe --verbose python-sim\generated\cpp_tx.wav
```

O CTest do core tambem executa um round-trip automatico `hftext_tx_wav` -> `hftext_rx_wav`.

Os CLIs usam sempre o modo robusto atual: frame logico HFText v0.1, codigo convolucional `conv_k3`, interleaving deterministico e fluxo fisico `PREAMBLE | START_SYNC | PHYS_LENGTH | ROBUST_FRAME`. Nao ha opcao para transmitir ou receber sem FEC/interleaving. Quando a recepcao vem do demodulador C++, a busca de `START_SYNC`, a recuperacao de `PHYS_LENGTH` e o Viterbi podem usar a confianca de cada simbolo.

Por padrao, a modulacao fisica e 2-FSK v0.1. Para testar a v0.2 experimental
4-FSK, use `--mode 4fsk` nos CLIs:

```powershell
core\build\Debug\hftext_tx_wav.exe --mode 4fsk --f0 1000 --f1 1200 --callsign pu5lrk "Teste" python-sim\generated\cpp_tx_4fsk.wav
core\build\Debug\hftext_rx_wav.exe --mode 4fsk --f0 1000 --f1 1200 --verbose python-sim\generated\cpp_tx_4fsk.wav
core\build\Debug\hftext_stream_wav.exe --mode 4fsk --f0 1000 --f1 1200 python-sim\generated\cpp_tx_4fsk.wav
```

No modo 4-FSK, `f0` e `f1` definem os dois primeiros tons e o espacamento; os
outros dois tons sao derivados automaticamente.

`hftext_rx_wav` continua sendo ferramenta offline de debug. A recepcao operacional do app PC usa `StreamingReceiver`, que processa blocos de audio durante a captura e evita depender de um WAV fechado.
