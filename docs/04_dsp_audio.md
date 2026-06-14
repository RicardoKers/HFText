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

Com `symbol_duration = 0.5 s`, cada bit ocupa meio segundo. Portanto, o preambulo padrao de 64 bits dura cerca de 32 segundos antes do `START_SYNC` fisico. Esse valor e conservador para testes com radio, mas pode ser reduzido em testes locais quando o caminho de audio e conhecido e estavel.

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

Testes reais via radio/SDR mostraram que a remocao de DC por janela degradou a recepcao em vez de ajudar. Por isso, o demodulador atual mede os tons diretamente na janela de simbolo, sem subtracao de media por simbolo.

O demodulador calcula duas metricas por simbolo:

```text
separacao = abs(energia(f1) - energia(f0)) / (energia(f1) + energia(f0))
concentracao_coerente = (energia(f0) + energia(f1)) / (energia_total_da_janela * amostras_da_janela)
qualidade = separacao * peso(concentracao_coerente)
```

Quando as duas energias de tom sao zero, as metricas sao zero.

`separacao` mede qual tom venceu e com que distancia relativa; ela alimenta o Viterbi soft-decision do bloco robusto. `qualidade` reduz a confianca quando a energia nao esta concentrada de forma coerente nos tons esperados, o que ajuda a nao tratar ruido puro como bit forte. O resultado final de recepcao pode informar a media de `qualidade` nos bits do quadro detectado. Essas metricas nao substituem a validacao por CRC.

Áudio restante menor que uma janela completa de símbolo deve ser ignorado.

Esta versão assume que o áudio está alinhado à janela de símbolo do demodulador.

Após a demodulação, o receptor procura o `START_SYNC` fisico de 32 bits no fluxo de bits e descarta preambulo, silencio demodulado, ruido demodulado ou outros bits anteriores ao quadro robusto.

No RX C++, essa busca pode usar uma metrica suave: candidatos com erros duros adicionais ainda podem ser avaliados quando os bits divergentes possuem baixa qualidade. O `PHYS_LENGTH`, transmitido tres vezes, tambem pode ser recuperado por voto ponderado por qualidade. Isso aumenta tolerancia a trechos fracos do marcador fisico sem mudar o protocolo transmitido.

Se um `START_SYNC` candidato aparecer em ruido antes do quadro real, o receptor deve continuar procurando outros candidatos ate encontrar um quadro completo com CRC e payload validos. Caso nenhum candidato seja valido, o primeiro erro encontrado pode ser usado como diagnostico.

O receptor Python também pode tentar múltiplos deslocamentos iniciais de amostra dentro de uma janela de símbolo. Para cada offset candidato:

- demodular o fluxo de bits;
- procurar `START_SYNC`;
- validar CRC e payload;
- aceitar o primeiro resultado válido.

O RX C++ pode tambem tentar pequenas variacoes de tempo de simbolo, escala de frequencia e deslocamento comum dos tons para compensar erro de clock, sintonia, BFO ou SDR. Essa busca deve ser limitada a desvios pequenos e a aceitacao da mensagem continua dependendo de CRC e payload validos.

Para operacao normal em radio, o RX deve ser orientado a fluxo continuo. O receptor deve processar blocos curtos de audio, acumular uma janela limitada e tentar decodificacao apenas quando houver amostras suficientes e novo audio relevante. Arquivos WAV completos continuam uteis para debug, mas nao devem ser o caminho principal de recepcao em operacao continua.

O passo padrão da busca offline é `samples_per_symbol / 20`, com mínimo de 1 amostra. A CLI Python permite ajustar esse passo com `--offset-step`.

O núcleo C++ também usa busca de offset inicial no caminho offline quando `syncSearch` está habilitado. O resultado de decodificação informa o offset aceito e quantos offsets foram testados. A CLI C++ `hftext_rx_wav` pode exibir esses dados com `--verbose`.

A CLI C++ `hftext_stream_wav` alimenta um WAV salvo ao `StreamingReceiver` em blocos curtos. Ela deve ser usada para depurar o mesmo caminho de recepcao continua do app PC a partir de capturas reais, sem acionar a busca offline de WAV fechado.

Sincronismo temporal fino contínuo e rastreamento de clock ainda ficam para etapa posterior.

## Recepcao em fluxo

Para escuta continua, o receptor nao deve depender de um WAV infinito. O app deve capturar blocos pequenos de audio e envia-los a um receptor persistente no core.

O primeiro passo no C++ e `StreamingReceiver`, que:

- recebe blocos de amostras por `pushSamples`;
- mantém um banco limitado de fases de simbolo para nao depender do instante exato em que a captura foi iniciada;
- mantem variantes pequenas de deslocamento comum de frequencia dos dois tons, incluindo passos intermediarios como `7,5 Hz`, para tolerar erro leve de sintonia/BFO/SDR no RX continuo;
- demodula incrementalmente apenas janelas de simbolo novas;
- acumula bits por fase em uma janela limitada;
- procura `START_SYNC`, recupera `PHYS_LENGTH` com apoio opcional da qualidade por simbolo e espera apenas o bloco robusto de tamanho conhecido;
- decodifica o `ROBUST_FRAME` com Viterbi soft-decision, usando a separacao de cada simbolo como peso de erro;
- emite eventos diagnosticos quando encontra `START_SYNC`, recupera `PHYS_LENGTH`, acumula `ROBUST_FRAME`, rejeita um quadro ou valida CRC/payload;
- emite `DecodeResult` quando CRC e payload sao validos;
- descarta amostras e bits consumidos apos um quadro recuperado.

Essa estrategia evita varrer todos os comprimentos possiveis de payload e evita reprocessar continuamente um WAV crescente. A validacao continua dependendo de CRC e payload validos; candidatos falsos de sincronismo sao descartados quando nao produzem frame logico valido.

Os eventos diagnosticos do RX continuo devem ser usados pela interface apenas para log e observabilidade. A interface pode resumir esses eventos no modo normal e expor todos no modo detalhado. Eles nao alteram a regra de aceitacao: somente quadro com CRC e payload validos deve aparecer como mensagem recebida.

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

## Interleaving e FEC robustos

O interleaving foi avaliado primeiro em Python como experimento independente. A forma adotada usa blocos retangulares completos: escreve bits por linhas e transmite por colunas, espalhando rajadas de erro no tempo. O deinterleaving faz o inverso antes do Viterbi e da validacao por CRC.

O interleaving com `conv_k3` agora faz parte do modo robusto unico do HFText v0.1. No RX C++, o decoder usa Viterbi soft-decision quando recebe confianca por simbolo do demodulador, mantendo compatibilidade com a decodificacao hard-decision para testes e fluxos puramente binarios. Repeticao continua sendo apenas experimento futuro.
