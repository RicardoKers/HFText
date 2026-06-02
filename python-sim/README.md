# HFText Python Simulation

Simulação inicial do HFText Basic v0.1 em Python.

## Testes

```powershell
python -B -m pytest
```

## Gerar WAV

```powershell
python tx_wav.py --callsign pu5lrk "Teste" generated\teste.wav
```

O transmissor insere o indicativo no início do payload, seguido por um espaço.

## Decodificar WAV

```powershell
python rx_wav.py generated\teste.wav
```

Se o CRC falhar, o receptor não imprime a mensagem como válida.
