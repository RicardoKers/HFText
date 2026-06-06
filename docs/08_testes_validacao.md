# Testes e validação

## Objetivo

Definir como validar o funcionamento do modem e das aplicações.

## Testes unitários

### Encoder

- texto simples deve gerar símbolos corretos;
- letras maiúsculas devem usar símbolo shift e voltar como maiúsculas na decodificação;
- vogais acentuadas em português devem usar modificadores `acute` ou `tilde` e voltar como texto acentuado;
- `ç` e `Ç` devem ser recuperados corretamente;
- caracteres inválidos devem ser tratados;
- decodificação deve recuperar texto original.

### CRC

- CRC de vetor conhecido deve bater com valor esperado;
- alteração de um bit deve invalidar o CRC.

### Frame

- montar quadro;
- desmontar quadro;
- detectar tamanho;
- validar payload;
- detectar CRC incorreto.

### Modulador

- saída não deve ultrapassar -1.0 a +1.0;
- duração do áudio deve corresponder ao número de símbolos;
- frequência dominante deve corresponder ao bit transmitido.

### Demodulador

- deve detectar tom 0;
- deve detectar tom 1;
- deve decodificar sequência conhecida;
- deve recuperar quadro limpo.

## Testes de canal

Simular:

- ruído branco;
- atenuação;
- offset DC;
- clipping;
- desvio de frequência;
- fading por blocos.

## Métricas

Registrar:

- mensagem transmitida;
- mensagem recebida;
- número de bits;
- número de erros;
- BER;
- sucesso/falha de CRC;
- SNR estimado;
- confianca media estimada pelo demodulador;
- duração da transmissão.

Na simulação Python inicial, as métricas mínimas são:

- potência média do sinal;
- número de erros de bit;
- BER;
- estado do CRC;
- validade do payload.

Os testes de canal iniciais devem cobrir recuperação em condições moderadas de:

- AWGN;
- atenuação;
- offset DC;
- clipping leve;
- pequeno desvio de frequência;
- fading leve por blocos.

Para estimar desempenho por SNR, a varredura deve executar múltiplas sementes por nível de ruído e registrar:

- taxa de sucesso de CRC;
- taxa de payload válido;
- BER média;
- pior BER;
- confianca media;
- menor confianca;
- mínimo e máximo de erros de bit.

Além da varredura por SNR, a simulação deve possuir uma varredura por cenários nomeados de canal, cobrindo efeitos isolados e combinação moderada de efeitos.

## Teste mínimo de aceitação da Fase 1

Configuração:

```text
indicativo = pu5lrk
mensagem = Teste
```

Payload esperado:

```text
pu5lrk Teste
```

Condições:

- WAV gerado pelo transmissor Python;
- WAV lido pelo receptor Python;
- sem ruído;
- CRC válido.

Resultado esperado:

```text
pu5lrk Teste
```

## Teste com ruído

Configuração:

```text
indicativo = pu5lrk
mensagem = cq cq
```

Condição:

- ruído branco moderado;
- SNR inicial de referência: 6 dB em teste determinístico;
- sem clipping;
- sem fading severo.

Resultado esperado:

- payload `pu5lrk cq cq` recuperado corretamente ou CRC deve falhar;
- nunca exibir mensagem errada como se fosse válida.

## Regra importante

Se o CRC falhar, o sistema não deve apresentar o texto como mensagem válida.

Deve mostrar:

```text
Quadro detectado, mas CRC inválido.
```

## Testes com rádio real

Etapas:

- teste com cabo direto entre saída e entrada de áudio;
- teste com dois dispositivos próximos sem RF;
- teste com rádio em carga fantasma, se aplicável;
- teste em VHF/FM para validação simples;
- teste em HF/SSB;
- teste com sinal fraco.

## Roteiro atual de testes de campo

Durante a validacao do RX continuo, salvar evidencias em uma pasta local `logs/`.
Essa pasta e material temporario de campo e nao deve ser versionada.

Usar log simples por padrao. O log detalhado deve ser ligado apenas quando:

- uma transmissao audivelmente boa nao for decodificada;
- a interface indicar muitos candidatos rejeitados;
- houver demora anormal depois do fim audivel do pacote;
- for necessario comparar fases internas do receptor.

Sequencia recomendada para cada rodada:

1. Alto-falante para microfone, simbolo 0.300 s, amplitude normal.
   Esperado: uma mensagem aceita, pouca latencia apos o fim do TX, sem clipping.
2. Alto-falante para microfone, mesma configuracao, amplitude menor.
   Esperado: aceitar se o sinal ainda estiver claro; se falhar, salvar evidencia com log simples.
3. Radio para SDR ou segundo computador, simbolo 0.300 s, amplitude normal.
   Esperado: decodificacao logo apos o fim do pacote e `Sessao RX` com poucos candidatos fortes consolidados.
4. Radio para SDR com sinal fraco ou ruido maior.
   Esperado: mensagem aceita ou rejeicao limpa por CRC/payload, sem travar a interface.
5. Pacote parcial: iniciar RX depois do inicio da transmissao ou interromper TX antes do fim.
   Esperado: nenhuma mensagem falsa deve aparecer; a interface deve continuar responsiva.
6. Repetir qualquer falha com `Log RX detalhado` ligado.
   Esperado: salvar evidencia completa para analise de sync, `PHYS_LENGTH`, progresso e CRC.

Em cada evidencia, registrar tambem:

- duracao de simbolo;
- amplitude aproximada usada no transmissor;
- se foi alto-falante/microfone, cabo, radio local ou radio/SDR remoto;
- se o sinal estava forte, medio ou fraco visualmente/auditivamente;
- se houve ruido impulsivo, fading ou clipping aparente.

## Logs

Cada recepção deve salvar opcionalmente:

- timestamp;
- configuração do modem;
- nível médio;
- pico;
- resultado do CRC;
- texto recebido;
- confiança estimada.

Para testes de campo, o app PC deve permitir salvar manualmente um pacote de evidencia RX contendo o audio recente em WAV e um TXT com configuracao, resumo CSV, historico de texto recebido e log. Esse pacote deve ser acionado pelo operador e servir como material reproduzivel para investigar falhas de sincronismo, `PHYS_LENGTH`, CRC ou nivel de sinal. O resumo CSV deve ter uma linha de cabecalho e uma linha de valores para facilitar copiar varios testes para uma planilha. Quando houver quadro aceito na sessao, o CSV deve preservar os diagnosticos desse ultimo aceite, incluindo qualidade, tamanho aceito, offset em amostras e quantidade de fases/tentativas, mesmo que a linha visual `Estado RX` ja tenha voltado a outro estado.

O utilitario `python-sim/field_summary.py` deve consolidar varios TXT de evidencia em um unico CSV agregado, preservando o caminho do arquivo original em `source_txt`. Isso permite comparar rodadas de campo por duracao de simbolo, contadores RX, qualidade, texto recebido e diagnosticos do ultimo quadro aceito. Quando gravar em arquivo, ele tambem deve gerar um CSV agrupado por parametros de modem para comparar taxa de aceite, qualidade media/minima e medias dos contadores RX entre configuracoes.

O utilitario `python-sim/field_replay.py` deve reproduzir os WAVs de evidencias aceitas usando o CLI C++ `hftext_rx_wav`, com duracao de simbolo e tons extraidos do `Resumo CSV`, e gravar `field_replay.csv` com texto esperado, texto decodificado, codigo de retorno e status. Esse replay nao substitui o RX continuo, mas permite verificar se capturas reais continuam decodificaveis pelo decoder offline apos alteracoes futuras.

No app PC, cada linha do log deve incluir timestamp. Durante RX continuo, o log normal deve mostrar eventos consolidados suficientes para operacao: sync forte, `PHYS_LENGTH`, progresso do `ROBUST_FRAME`, rejeicoes agregadas, texto recebido, confianca e latencia estimada quando um quadro valido for publicado. O log normal deve omitir marcos repetidos por fases diferentes. A opcao `Log RX detalhado` deve preservar a telemetria completa por fase para debug.

## Validacao no app PC

No app PC, o fluxo atual de validacao com audio real e:

- gerar um WAV TX com a configuracao desejada;
- transmitir explicitamente o WAV pelo dispositivo de saida selecionado;
- iniciar RX pelo dispositivo de entrada selecionado;
- aguardar a mensagem aparecer no historico de texto recebido durante a captura;
- conferir se o texto recebido tambem aparece no log;
- registrar offset/fase aceita, quantidade de fases testadas e confianca media estimada;
- conferir no log a sequencia resumida `sync forte` -> `PHYS_LENGTH` -> acumulacao do `ROBUST_FRAME` -> quadro valido;
- parar RX para encerrar a escuta;
- registrar duracao, pico de audio e amostras proximas de clipping;
- aceitar o texto recebido apenas se o CRC estiver valido.

Ao abrir um WAV manualmente no app PC, a validacao tambem deve registrar duracao, sample rate, pico de audio e amostras proximas de clipping antes da decodificacao.

Durante uma captura RX ativa, o app deve priorizar a reciclagem dos buffers de audio e enviar copias curtas para o `StreamingReceiver` em thread propria. A decodificacao deve acontecer durante a recepcao e publicar mensagens pouco depois do fim do quadro. WAV fechado deve ser usado apenas para debug e reproducao de casos de teste.

O app deve permanecer responsivo mesmo quando o canal gerar muitos candidatos falsos. Para isso, a fila interna de audio RX e limitada, o worker processa blocos curtos, o waterfall pode descartar atualizacoes visuais atrasadas e o log detalhado pode resumir/omitir excesso de eventos por lote. Em teste manual, deixar RX ativo em ruido ou em uma transmissao fraca nao deve congelar a janela; se o processamento nao acompanhar o tempo real, o comportamento aceitavel e perder parte da recepcao atual e continuar operavel.

Ao testar captura por placa de som, conferir no log do app:

- `RX streaming iniciado` deve informar a taxa de captura RX;
- `RX duracao` deve informar o sample rate usado na captura;
- a duracao exibida deve bater com o tempo real de gravacao.

Se o WAV recebido parecer comprimido ou esticado, a primeira verificacao e comparar o sample rate RX configurado com o sample rate mostrado no log e no arquivo WAV salvo. Em Windows, usar 48000 Hz para RX e o ponto de partida recomendado.

## Testes manuais de interface PC

A interface PC deve manter validacoes manuais simples:

- ao digitar mensagem TX com letras maiusculas, o contador deve incluir os simbolos `shift`;
- ao digitar vogais acentuadas, o contador deve incluir os modificadores `acute` ou `tilde`;
- ao digitar `ç`, o contador deve tratar como um simbolo, e `Ç` como `shift` + `ç`;
- ao digitar caracteres invalidos, o campo TX deve mostrar `?` antes de gerar WAV;
- ao alterar duracao de simbolo ou preambulo, a duracao estimada deve atualizar;
- durante `Transmitir WAV`, a barra de progresso deve avancar ate o fim do arquivo ou parar corretamente ao clicar `Parar TX`;
- durante RX, a barra `Progresso RX` deve avancar de forma monotona dentro de um candidato forte, sem oscilar com candidatos fracos ou fases internas, e voltar a zero apos rejeicao forte completa;
- durante RX, a linha `Estado RX` deve mostrar o estado consolidado mais util do lote recente, ultimo `PHYS_LENGTH`, qualidade do ultimo candidato completo e ultimo motivo de rejeicao forte;
- durante RX, a linha `Sessao RX` deve acumular duracao e contadores consolidados da recepcao atual, contando rejeicoes fortes no modo normal, reiniciar ao clicar `Receber` e aparecer no log ao parar RX;
- a waterfall RX deve atualizar visualmente durante captura sem encurtar o WAV salvo nem atrapalhar a decodificacao ao parar RX;
- a estimativa TX deve refletir sempre o fluxo robusto com FEC/interleaving;
- o botao `Salvar Log` deve gerar um arquivo `.txt` contendo cabecalho de configuracao e o log atual com timestamps;
- o botao `Limpar Log` deve limpar somente o log, sem apagar `Texto recebido` nem configuracoes;
- o botao `Salvar Evidencia RX` deve criar um `.wav` com audio RX recente e um `.txt` associado com resumo CSV, sem parar automaticamente a recepcao, preservando os dados do ultimo quadro aceito quando houver;
- `python-sim/field_summary.py` deve ler os TXT de evidencia e gerar um CSV agregado com uma linha por evidencia valida, alem de um CSV agrupado por parametros quando solicitado ou usado no modo padrao de arquivo;
- `python-sim/field_replay.py` deve rodar os WAVs das evidencias aceitas pelo `hftext_rx_wav` e gerar um CSV de replay com passa/falha;
- ao fechar e abrir novamente, o app deve restaurar indicativo, parametros do modem, dispositivos selecionados, estado do log detalhado e geometria da janela;
- ao fechar e abrir novamente, a caixa de mensagem TX deve permanecer vazia;
- no Windows, abrir `hftext_pc.exe` deve mostrar apenas a janela grafica do HFText, sem console adicional;
- a janela e o executavel devem exibir o icone proprio do HFText.

A waterfall e validada manualmente: durante `Receber`, tons proximos da faixa do modem devem aparecer como trilhas horizontais, e a duracao registrada ao parar RX deve continuar coerente com o tempo real de recepcao.

O indicador de clipping e aproximado e usa amostras com magnitude muito proxima do fundo de escala. O app deve registrar quantidade e porcentagem de amostras clipadas; picos isolados podem ser ruido impulsivo do canal, enquanto clipping frequente sugere reduzir ganho ou volume quando possivel.

## Testes do modo robusto

A simulacao Python deve continuar validando a camada robusta:

- round-trip limpo com `conv_k3` sem interleaving;
- round-trip limpo com `conv_k3 + interleaving`;
- Viterbi recuperando quadros com erros esparsos antes da verificacao de CRC;
- Viterbi soft-decision recuperando bits fracos quando a decisao dura escolhe um caminho incorreto;
- ruido puro deve produzir baixa qualidade, mesmo quando a decisao dura escolher 0 ou 1;
- deinterleaving restaurando exatamente o fluxo codificado antes do Viterbi;
- geometria de interleaving derivada de forma deterministica a partir do tamanho codificado;
- rejeicao de geometrias que nao encaixem exatamente no fluxo codificado;
- varredura por SNR comparando frame logico sem FEC, repeticao 3x, Hamming(7,4), `conv_k3` e `conv_k3 + interleaving`;
- comparacao por tamanho de payload curto, medio e longo;
- registro de taxa de CRC, payload valido, BER recuperada, confianca media, pior BER e distancia media do Viterbi.

No core C++, os testes automatizados devem cobrir:

- helpers puros de `conv_k3`, Viterbi, interleaving e deinterleaving;
- Viterbi soft-decision e parse robusto usando confianca por bit;
- montagem e parse de frame robusto em bits;
- deteccao de `START_SYNC` fisico em fluxo de bits com preambulo e bits extras;
- deteccao de `START_SYNC` em audio com erros duros adicionais quando os bits divergentes possuem baixa qualidade;
- recuperacao de `PHYS_LENGTH` repetido por voto duro e por voto ponderado por qualidade;
- rejeicao de tamanho fisico invalido;
- round-trip limpo via API publica `modulateText`/`demodulateSamples`;
- recepcao continua por `StreamingReceiver`, incluindo atraso inicial arbitrario e mais de um quadro no mesmo fluxo;
- round-trip WAV pelos CLIs `hftext_tx_wav` e `hftext_rx_wav`;
- round-trip manual no app PC gerando e decodificando o mesmo WAV.

O modo robusto deve continuar aceitando texto recebido apenas quando o CRC do frame logico estiver valido. O decoder FEC, incluindo Viterbi soft-decision, nao substitui o CRC.
