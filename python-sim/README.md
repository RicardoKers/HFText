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

Para imprimir diagnosticos de recepcao, incluindo offset, tentativas e confianca media:

```powershell
python rx_wav.py --verbose generated\teste.wav
```

## Varredura de ruído

```powershell
python noise_sweep.py --callsign pu5lrk --symbol-duration 0.05 --trials 20 "Teste HFText"
```

O script salva exemplos WAV, `summary.csv` agregado e `trials.csv` por tentativa em `generated\noise_sweep\`, medindo BER, CRC e validade do payload para cada SNR.
Os CSVs tambem registram a confianca media estimada pelo demodulador.

## Varredura experimental de repeticao

```powershell
python repetition_sweep.py "Teste HFText" --callsign pu5lrk --symbol-duration 0.05 --factor 1 3 --trials 20
```

Esse experimento compara fatores de repeticao por bit, como 1x e 3x, sob AWGN. Ele salva `summary.csv` e `trials.csv` em `generated\repetition_sweep\`, incluindo BER antes/depois do voto majoritario, sucesso de CRC/payload, confianca media e multiplicador relativo de duracao.

Tambem e possivel combinar AWGN com fading por blocos:

```powershell
python repetition_sweep.py "Teste HFText" --callsign pu5lrk --factor 1 3 --snr -12 --fading-block-symbols 4 --fading-min-gain 0.3 --fading-max-gain 1.0
```

Para avaliar interleaving experimental, use blocos que dividam exatamente o numero de bits do quadro:

```powershell
python repetition_sweep.py "Teste" --callsign pu5lrk --factor 3 --snr -12 --fading-block-symbols 4 --fading-min-gain 0.3 --fading-max-gain 1.0 --interleave-rows 4 --interleave-columns 138
```

Para comparar varias geometrias de interleaving no mesmo cenario:

```powershell
python interleaving_sweep.py "Teste" --callsign pu5lrk --factor 3 --rows 4 6 --snr -12 --fading-block-symbols 4 --fading-min-gain 0.3 --fading-max-gain 1.0
```

Sem `--rows`, o script testa automaticamente linhas que dividem exatamente o numero de bits repetidos, entre `--min-rows` e `--max-rows`.
Por padrao, ele tambem inclui a linha de base com repeticao sem interleaving; use `--no-baseline` para comparar apenas geometrias.
Alem de `summary.csv` e `trials.csv`, ele grava `best_summary.csv` com a melhor opcao por SNR, priorizando CRC, payload valido e BER.

## Varredura de canal

```powershell
python channel_sweep.py --callsign pu5lrk --symbol-duration 0.05 --trials 20 "Teste HFText"
```

O script executa cenários nomeados de canal, incluindo AWGN, atenuação, offset DC, clipping, desvio de frequência, fading por blocos e uma combinação moderada. Por padrão, ele salva apenas `summary.csv` e `trials.csv`; use `--save-wavs` para salvar exemplos WAV.
Os CSVs tambem registram a confianca media estimada pelo demodulador.
