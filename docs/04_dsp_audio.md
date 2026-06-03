# DSP de áudio

## Objetivo

Definir os parâmetros iniciais de áudio usados na simulação Python do modem HFText.

## Modulação inicial

A versão inicial usa 2-FSK:

```text
bit 0 -> 1200 Hz
bit 1 -> 1600 Hz
```

Parâmetros padrão:

```text
sample_rate = 48000 Hz
symbol_duration = 0.5 s
f0 = 1200 Hz
f1 = 1600 Hz
amplitude = 0.8
preamble_bits = 64
```

Com `symbol_duration = 0.5 s`, cada bit ocupa meio segundo. Portanto, o preâmbulo padrão de 64 bits dura cerca de 32 segundos antes do `SYNC`. Esse valor é conservador para testes com rádio, mas pode ser reduzido em testes locais quando o caminho de áudio é conhecido e estável.

O modulador deve gerar áudio mono em `float32`, normalizado entre `-1.0` e `+1.0`.

## Fase

O modulador 2-FSK deve manter fase contínua entre símbolos para evitar cliques desnecessários no áudio gerado.

## WAV

Arquivos WAV gerados na simulação devem preservar o áudio mono normalizado.

Scripts e artefatos gerados futuramente devem usar `python-sim/generated/` como diretório padrão.

## Demodulação inicial

A demodulação 2-FSK inicial usa detecção não coerente por correlação I/Q em cada janela de símbolo.

Para cada símbolo:

- calcular a energia em `f0`;
- calcular a energia em `f1`;
- decidir `0` se `energia(f0) >= energia(f1)`;
- decidir `1` se `energia(f1) > energia(f0)`.

O demodulador tambem calcula uma confianca simples por simbolo:

```text
confianca = abs(energia(f1) - energia(f0)) / (energia(f1) + energia(f0))
```

Quando as duas energias sao zero, a confianca e zero. O resultado final de recepcao pode informar a media dessa confianca nos bits do quadro detectado. Essa metrica e apenas diagnostica: nao substitui a validacao por CRC.

Áudio restante menor que uma janela completa de símbolo deve ser ignorado.

Esta versão assume que o áudio está alinhado à janela de símbolo do demodulador.

Após a demodulação, o receptor procura o `SYNC` no fluxo de bits e descarta preâmbulo, silêncio demodulado ou outros bits anteriores ao quadro.

Se um `SYNC` candidato aparecer em ruido antes do quadro real, o receptor deve continuar procurando outros candidatos ate encontrar um quadro completo com CRC e payload validos. Caso nenhum candidato seja valido, o primeiro erro encontrado pode ser usado como diagnostico.

O receptor Python também pode tentar múltiplos deslocamentos iniciais de amostra dentro de uma janela de símbolo. Para cada offset candidato:

- demodular o fluxo de bits;
- procurar `SYNC`;
- validar CRC e payload;
- aceitar o primeiro resultado válido.

O passo padrão da busca é `samples_per_symbol / 20`, com mínimo de 1 amostra. A CLI Python permite ajustar esse passo com `--offset-step`.

O núcleo C++ também usa busca de offset inicial quando `syncSearch` está habilitado, usando o mesmo passo padrão de `samples_per_symbol / 20`. O resultado de decodificação informa o offset aceito e quantos offsets foram testados. A CLI C++ `hftext_rx_wav` pode exibir esses dados com `--verbose`.

Sincronismo temporal fino contínuo, rastreamento de clock e recuperação em áudio sem alinhamento aproximado ficam para etapa posterior.

## Recepcao em fluxo

Para escuta continua, o receptor nao deve depender de um WAV infinito. O app deve capturar blocos pequenos de audio e envia-los a um receptor persistente no core.

O primeiro passo no C++ e `StreamingReceiver`, que:

- recebe blocos de amostras por `pushSamples`;
- acumula temporariamente as amostras em buffer interno;
- tenta decodificar quadros completos com o receptor offline existente;
- emite `DecodeResult` quando CRC e payload sao validos;
- descarta amostras consumidas apos um quadro recuperado.

Esta primeira versao ainda nao implementa rastreamento continuo de clock nem demodulacao incremental por simbolo; ela cria uma ponte segura entre o modo WAV offline e a futura recepcao em tempo real.

## Canal simulado

A validação inicial deve incluir ruído branco Gaussiano aditivo (AWGN) com SNR configurável.

O SNR é definido por potência média:

```text
signal_power = mean(samples^2)
noise_power = signal_power / 10^(snr_db / 10)
```

O canal simulado deve ser determinístico nos testes quando receber uma semente fixa de gerador aleatório.

Também deve haver efeitos determinísticos simples para testes de:

- atenuação por ganho linear;
- offset DC;
- clipping simétrico;
- desvio de frequência por sinal analítico;
- fading por blocos com ganho constante por bloco.

## Repeticao futura

A repeticao simples deve ser avaliada primeiro como experimento de DSP/simulacao, sem alterar o protocolo v0.1.

O experimento recomendado e aplicar repeticao por bit ou simbolo antes da modulacao e recuperar por voto majoritario depois da demodulacao. A metrica de confianca pode ser usada como apoio ao diagnostico, mas a aceitacao da mensagem deve continuar dependendo de CRC valido.

A simulacao Python possui helpers experimentais para repetir bits e recuperar por voto majoritario. Eles ainda nao fazem parte dos scripts TX/RX normais nem do core C++.

## Interleaving futuro

O interleaving deve ser avaliado primeiro em Python como experimento independente. A primeira forma experimental usa blocos retangulares completos: escreve bits por linhas e transmite por colunas, espalhando rajadas de erro no tempo. O deinterleaving faz o inverso antes do voto majoritario ou da validacao por CRC.

Assim como a repeticao, esse recurso ainda nao faz parte do HFText Basic v0.1 nem dos scripts TX/RX normais.
