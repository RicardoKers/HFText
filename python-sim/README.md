# HFText Python Simulation

Simulação inicial do HFText v0.1 em Python.

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

## Resumo de evidencias de campo

Depois de salvar evidencias pelo app PC em `logs\`, e possivel consolidar os blocos `Resumo CSV` em uma unica tabela:

```powershell
python field_summary.py --input-dir ..\logs --output ..\logs\field_summary.csv
```

Sem `--output`, o script grava `field_summary.csv` dentro da pasta informada por `--input-dir`. Use `--stdout` para imprimir o CSV agregado no terminal.

Quando grava em arquivo, o script tambem cria `field_summary_groups.csv`, agrupando por duracao de simbolo, tons, amplitude, preambulo e estado do log detalhado. Esse resumo inclui quantidade de evidencias, quadros aceitos, taxa de aceite, qualidade media/minima e medias dos contadores RX. Use `--group-by` para escolher outras colunas de agrupamento, `--group-output` para escolher outro caminho ou `--no-groups` para gerar apenas a tabela linha a linha.

Para reproduzir os WAVs das evidencias aceitas pelo decoder C++ de linha de comando:

```powershell
python field_replay.py --input-dir ..\logs --output ..\logs\field_replay.csv
```

O script procura `hftext_rx_wav` nos builds conhecidos, usa os parametros registrados na evidencia e compara a primeira linha decodificada com o texto recebido salvo no TXT. Use `--rx-exe` se o executavel estiver em outro caminho. O resultado `field_replay.csv` permite transformar capturas reais em casos de regressao manual para o decoder offline.

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

## FEC experimental

O modulo `hftext.fec` contem um helper inicial Hamming(7,4), apenas para experimentos de validacao. Ele codifica blocos de 4 bits em 7 bits, corrige 1 erro por codeword e informa quantos blocos foram corrigidos.

O mesmo modulo tambem contem o codigo convolucional rate 1/2, K=3, com geradores `111` e `101` e decoder Viterbi hard-decision usado nas varreduras historicas em Python.

Esse FEC faz parte do modo robusto atual no core C++. No RX C++ por audio, o decoder pode usar Viterbi soft-decision com a confianca dos simbolos; os scripts Python historicos continuam uteis para experimentos e comparacoes.

Para comparar o quadro sem FEC, Hamming(7,4) e o convolucional K=3:

```powershell
python fec_sweep.py "Teste" --callsign pu5lrk --snr -12 --trials 20 --fading-block-symbols 4 --fading-min-gain 0.3 --fading-max-gain 1.0
```

O script salva `summary.csv` e `trials.csv` em `generated\fec_sweep\`, incluindo BER do canal codificado, BER recuperada do quadro original, CRC/payload, quantidade media de codewords corrigidas e distancia media do decoder Viterbi para o modo convolucional.

Tambem e possivel aplicar interleaving experimental depois da codificacao FEC:

```powershell
python fec_sweep.py "Teste" --callsign pu5lrk --mode hamming74 --snr -12 --trials 20 --fading-block-symbols 4 --fading-min-gain 0.3 --fading-max-gain 1.0 --interleave-rows 14 --interleave-columns 23
```

Para varrer varias geometrias de interleaving com um modo FEC:

```powershell
python fec_interleaving_sweep.py "Teste" --callsign pu5lrk --mode conv_k3 --snr -12 --trials 20 --fading-block-symbols 4 --fading-min-gain 0.3 --fading-max-gain 1.0
```

O script inclui por padrao a linha de base do modo FEC escolhido sem interleaving e grava `best_summary.csv` com a melhor geometria por SNR. Os modos aceitos sao `hamming74` e `conv_k3`.

Use `--auto-shape` para testar a geometria deterministica inicial. Ela escolhe um divisor exato do tamanho codificado com numero de linhas mais proximo de `--preferred-rows`, cujo padrao atual e 6.

## Varredura de canal

```powershell
python channel_sweep.py --callsign pu5lrk --symbol-duration 0.05 --trials 20 "Teste HFText"
```

O script executa cenários nomeados de canal, incluindo AWGN, atenuação, offset DC, clipping, desvio de frequência, fading por blocos e uma combinação moderada. Por padrão, ele salva apenas `summary.csv` e `trials.csv`; use `--save-wavs` para salvar exemplos WAV.
Os CSVs tambem registram a confianca media estimada pelo demodulador.
