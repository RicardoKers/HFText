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
