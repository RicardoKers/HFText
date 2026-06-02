# HFText Python Simulation

Simulação inicial do HFText Basic v0.1 em Python.

## Testes

```powershell
python -B -m pytest
```

Testes de canal com ruído branco:

```powershell
python -B -m pytest tests\test_channel.py
```

## Gerar WAV

```powershell
python tx_wav.py --callsign pu5lrk "Teste" generated\teste.wav
```

O transmissor insere o indicativo no início do payload, seguido por um espaço.

O WAV gerado inclui um preâmbulo alternado antes do quadro. O preâmbulo ajuda o receptor a ignorar áudio/bits antes do `SYNC`.

## Decodificar WAV

```powershell
python rx_wav.py generated\teste.wav
```

Se o CRC falhar, o receptor não imprime a mensagem como válida.

O RX procura `SYNC` no fluxo de bits e, por padrão, tenta múltiplos offsets de símbolo. Para desabilitar a busca:

```powershell
python rx_wav.py --no-sync-search generated\teste.wav
```

## Varredura de ruído

```powershell
python noise_sweep.py --callsign pu5lrk --symbol-duration 0.05 --trials 20 "Teste HFText"
```

O script salva exemplos WAV, `summary.csv` agregado e `trials.csv` por tentativa em `generated\noise_sweep\`, medindo BER, CRC e validade do payload para cada SNR.

## Varredura de canal

```powershell
python channel_sweep.py --callsign pu5lrk --symbol-duration 0.05 --trials 20 "Teste HFText"
```

O script executa cenários nomeados de canal, incluindo AWGN, atenuação, offset DC, clipping, desvio de frequência, fading por blocos e uma combinação moderada. Por padrão, ele salva apenas `summary.csv` e `trials.csv`; use `--save-wavs` para salvar exemplos WAV.
