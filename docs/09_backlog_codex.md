# Backlog para implementação com Codex

## Épico 1 — Estrutura inicial

### Tarefa 1.1 — Criar estrutura do repositório

Criar a estrutura inicial:

```text
python-sim/
core/
pc-app/
android-app/
docs/

Adicionar arquivos .gitkeep onde necessário.

Critérios de aceitação:

estrutura criada;
nenhum código complexo ainda;
README preservado.
Tarefa 1.2 — Criar CMake inicial do core

Criar um projeto CMake para core.

Critérios de aceitação:

core/CMakeLists.txt;
biblioteca hftext_core;
projeto compila vazio ou com função mínima.
Épico 2 — Simulação Python
Tarefa 2.1 — Implementar codificação de caracteres em Python

Implementar codificação de caracteres de 6 bits conforme docs/03_protocolo_modem.md.

Criar funções:

encode_text_to_symbols(text: str) -> list[int]
decode_symbols_to_text(symbols: list[int]) -> str

Critérios de aceitação:

teste com "abc 123";
maiúsculas codificadas com shift;
caracteres inválidos tratados.
Tarefa 2.2 — Implementar CRC16 em Python

Implementar CRC-16/CCITT-FALSE.

Critérios de aceitação:

função crc16_ccitt_false(data: bytes) -> int;
teste com vetor conhecido;
alteração de byte muda o CRC.
Tarefa 2.3 — Implementar montagem de quadro em Python

Implementar:

build_frame(payload_text: str) -> list[int]
parse_frame(bits: list[int]) -> FrameResult

Critérios de aceitação:

montar e desmontar quadro;
validar CRC;
rejeitar CRC inválido.
Tarefa 2.4 — Implementar modulador 2-FSK em Python

Implementar:

modulate_bits_2fsk(bits, sample_rate, symbol_duration, f0, f1) -> np.ndarray

Critérios de aceitação:

gerar áudio normalizado;
duração correta;
salvar WAV.
Tarefa 2.5 — Implementar demodulador 2-FSK em Python

Implementar demodulação por Goertzel ou correlação.

Critérios de aceitação:

demodular bits de áudio limpo;
recuperar sequência conhecida.
Tarefa 2.6 — Criar scripts TX/RX WAV em Python

Criar:

python-sim/tx_wav.py
python-sim/rx_wav.py

Uso esperado:

python tx_wav.py --callsign pu5lrk "Teste" output.wav
python rx_wav.py output.wav

Critérios de aceitação:

gerar WAV;
ler WAV;
recuperar mensagem.
Épico 3 — Núcleo C++
Tarefa 3.1 — Criar tipos básicos do core

Criar:

core/include/hftext_config.h
core/include/hftext_result.h

Com:

struct ModemConfig;
struct DecodeResult;

Critérios de aceitação:

compila;
sem dependência de UI.
Tarefa 3.2 — Implementar encoder C++

Implementar codificação de texto reduzida.

Critérios de aceitação:

testes passando;
compatível com Python.
Tarefa 3.3 — Implementar CRC16 C++

Implementar CRC-16/CCITT-FALSE.

Critérios de aceitação:

teste com vetor conhecido;
compatível com Python.
Tarefa 3.4 — Implementar frame C++

Implementar montagem e desmontagem de quadro.

Critérios de aceitação:

frame válido passa;
frame alterado falha no CRC.
Tarefa 3.5 — Implementar modulador 2-FSK C++

Implementar geração de áudio.

Critérios de aceitação:

saída std::vector<float>;
faixa -1.0 a +1.0;
duração correta.
Tarefa 3.6 — Implementar demodulador 2-FSK C++

Implementar Goertzel e decisão binária.

Critérios de aceitação:

recupera bits em áudio limpo;
recupera mensagem gerada pelo modulador.
Épico 4 — CLI
Tarefa 4.1 — Criar hftext_tx_wav

Criar ferramenta CLI:

hftext_tx_wav --callsign pu5lrk "Mensagem" output.wav

Critérios de aceitação:

gera WAV;
usa core C++.
Tarefa 4.2 — Criar hftext_rx_wav

Criar ferramenta CLI:

hftext_rx_wav input.wav

Critérios de aceitação:

imprime texto recebido;
informa CRC válido ou inválido.
Épico 5 — Aplicação PC
Tarefa 5.1 — Criar app Qt mínimo

Criar janela com:

campo de texto;
botão gerar WAV;
botão decodificar WAV;
área de saída.

Critérios de aceitação:

compila;
chama core C++.
Tarefa 5.2 — Adicionar reprodução de áudio

Adicionar saída de áudio.

Critérios de aceitação:

usuário consegue transmitir áudio pela placa de som.
Tarefa 5.3 — Adicionar captura de áudio

Adicionar entrada de áudio.

Critérios de aceitação:

mostrar nível de entrada;
armazenar buffer capturado.
Tarefa 5.4 - Mostrar estimativa de transmissao

Adicionar indicadores calculados enquanto o operador digita:

- quantidade de simbolos de 6 bits do payload TX, contando `shift`;
- limite de 127 simbolos;
- duracao estimada da transmissao com preambulo, quadro e duracao de simbolo atuais.

Criterios de aceitacao:

contador atualiza durante a digitacao;
maiusculas contam o simbolo `shift`;
duracao muda quando preambulo ou duracao de simbolo mudam.
Tarefa 5.5 - Sanitizar mensagem TX na interface

Substituir caracteres invalidos por `?` diretamente no campo de mensagem TX, antes de gerar ou transmitir.

Criterios de aceitacao:

operador ve exatamente o texto transmitivel;
substituicao usa o mesmo alfabeto do core;
nao altera a regra do protocolo.
Tarefa 5.6 - Mostrar progresso durante TX

Adicionar barra de progresso durante `Transmitir WAV`.

Criterios de aceitacao:

barra inicia em 0 ao comecar TX;
barra acompanha a duracao do WAV reproduzido;
barra para e volta ao estado correto ao clicar `Parar TX`.
Tarefa 5.7 - Adicionar waterfall RX simples

Adicionar uma waterfall pequena para observacao visual do sinal recebido.

Criterios de aceitacao:

usa blocos de audio capturados para estimar energia por frequencia;
nao bloqueia a thread de captura;
primeira versao e apenas visual, sem afetar a decodificacao.
Épico 6 — Android
Tarefa 6.1 — Criar projeto Android

Criar app Kotlin + Compose.

Critérios de aceitação:

app abre;
tela principal aparece.
Tarefa 6.2 — Implementar TX Android inicial

Implementar geração e reprodução de áudio.

Critérios de aceitação:

usuário digita texto;
app reproduz áudio FSK.
Tarefa 6.3 — Implementar RX Android inicial

Implementar captura de áudio.

Critérios de aceitação:

app solicita microfone;
captura áudio;
mostra nível.
Tarefa 6.4 — Integrar núcleo C++ via JNI

Critérios de aceitação:

Kotlin chama modulateText;
Kotlin chama demodulateSamples.
Épico 7 — Robustez
Tarefa 7.1 — Melhorar preâmbulo

Implementar preâmbulo mais robusto.

Tarefa 7.2 — Implementar detecção automática de início

Não assumir que o áudio começa exatamente no início do quadro.

Refinamento aplicado: o fluxo fisico agora usa `PREAMBLE | START_SYNC | PHYS_LENGTH | ROBUST_FRAME`. O `START_SYNC` e transmitido diretamente antes do bloco robusto para marcar o inicio dos dados e reduzir a busca lenta por candidatos Viterbi. O marcador fisico atual e `0x2DD4 0x2DD4`. O `PHYS_LENGTH` vem logo depois do `START_SYNC`, repetido 3 vezes, para permitir que o RX saiba exatamente quantos bits robustos deve acumular antes de rodar Viterbi.

Refinamento de RX aplicado: a demodulacao C++ passou a varrer pequenas variacoes de duracao de simbolo e escala de frequencia, compensando desvios de clock em capturas por microfone, especialmente perceptiveis com simbolos longos como `0,3 s`.

Refinamento de operacao aplicado: o app PC passou a receber em streaming, com decodificacao em thread de segundo plano alimentada por blocos curtos de audio. O `StreamingReceiver` demodula incrementalmente simbolos novos em um banco limitado de fases e usa `PHYS_LENGTH` para evitar varrer todos os tamanhos de payload. O fluxo de WAV fechado fica reservado para debug e reproducao de capturas, evitando travamento da interface durante recepcao continua.

Telemetria aplicada: o `StreamingReceiver` agora emite eventos diagnosticos para `START_SYNC`, `PHYS_LENGTH`, acumulacao do `ROBUST_FRAME`, quadro rejeitado e quadro valido. O app PC registra timestamp em cada linha e possui `Log RX detalhado`: no modo normal mostra eventos consolidados para operacao; no modo detalhado mostra a telemetria completa por fase, permitindo diagnosticar se a falha ocorreu no sincronismo, no tamanho fisico, no acúmulo de bits ou no CRC/payload.

Refinamento aplicado apos os primeiros testes continuos bem-sucedidos: o log normal foi reduzido para evitar poluicao durante operacao, e o resumo de clipping passou a mostrar porcentagem e classificar picos isolados, clipping ocasional ou clipping frequente.

Condicionamento RX removido apos teste real: a remocao da media de cada janela de simbolo foi avaliada como opcao, mas em recepcao radio/SDR degradou a decodificacao. O demodulador C++ offline, o `StreamingReceiver` e o app PC voltaram a medir os tons diretamente, sem subtracao de DC por simbolo.

Tambem foi iniciada uma metrica diagnostica de confianca no demodulador, calculada pela separacao relativa entre as energias dos dois tons. Essa metrica deve ajudar logs e interface no futuro, mas nao substitui CRC.

Refinamento aplicado apos testes reais com simbolo de `0,3 s`: o core C++ passou a decodificar o bloco robusto com Viterbi soft-decision quando ha confianca por simbolo disponivel. O fluxo transmitido nao muda; o RX usa a confianca do demodulador para penalizar menos bits fracos e penalizar mais bits errados com alta confianca. A aceitacao continua dependendo de CRC e payload validos.

Refinamento adicional no sincronismo fisico: `demodulateSamples` e `StreamingReceiver` passaram a usar a confianca por simbolo tambem na busca de `START_SYNC` e na recuperacao de `PHYS_LENGTH`. O receptor ainda preserva o limite duro antigo para candidatos claros, mas pode avaliar candidatos com alguns erros duros extras quando os bits divergentes sao fracos. O fluxo transmitido nao muda e a aceitacao continua dependendo de CRC, payload e consistencia de tamanho.

Refinamento apos log real com duas transmissoes de `0,3 s/bit`: a transmissao fraca chegou a varios candidatos plausiveis, inclusive `LENGTH 21/21`, mas falhou em CRC; a transmissao um pouco mais forte foi aceita com `21 simbolos` e qualidade media em torno de 70%. Para reduzir falsos candidatos em ruido e nao superestimar bits de baixa energia, o demodulador passou a separar duas metricas: `confidence` como separacao relativa dos tons para o Viterbi soft-decision, e `quality` como separacao ponderada por concentracao coerente de energia para `START_SYNC`, `PHYS_LENGTH` e diagnostico.

Tarefa 7.3 — Implementar 4-FSK

Adicionar modo 4-FSK.

Implementacao experimental v0.2 iniciada: core C++ recebeu helpers MFSK/4-FSK
para modulacao e demodulacao, mantendo 2-FSK como padrao. O modo 4-FSK usa
`Tom 0` e `Tom 1` como os dois primeiros tons e deriva os outros dois tons pelo
mesmo espacamento. O frame logico, `START_SYNC`, `PHYS_LENGTH`, FEC,
interleaving e CRC permanecem iguais ao baseline v0.1.

Os CLIs `hftext_tx_wav`, `hftext_rx_wav` e `hftext_stream_wav` passaram a
aceitar `--mode 2fsk|4fsk`. O app PC recebeu seletor de modulacao na aba
`Configuracao`, reinicia RX automaticamente quando o modo muda, ajusta a
estimativa de duracao e mostra dois ou quatro marcadores na waterfall conforme
o modo escolhido.

Foi adicionado `python-sim/mfsk_sweep.py` para comparar 2-FSK e 4-FSK em AWGN
com multiplas sementes, registrando duracao relativa, BER, CRC e payload
valido. O proximo criterio antes de promover o 4-FSK e comparar o modo em
capturas reais de radio/SDR, especialmente em sintonia levemente deslocada e
SNR baixo.

Refinamento apos testes rapidos da v0.2: o preambulo 4-FSK deixou de usar bits
alternados `1010...`, pois isso virava um unico tom quando agrupado em pares.
Agora o preambulo experimental percorre os quatro tons com o ciclo
`00 01 10 11`. O RX continuo tambem passou a ignorar posicoes impossiveis de
`START_SYNC` no 4-FSK e a marcar quadros completos rejeitados para nao
reprocessar indefinidamente o mesmo candidato antes de uma proxima mensagem.

Tarefa 7.4 — Implementar repetição

Enviar bits ou símbolos repetidos.

Antes de implementar no protocolo definitivo, criar experimento Python isolado de repeticao por bit ou simbolo com voto majoritario no RX. O HFText v0.1 permanece sem repeticao; qualquer modo repetido deve ser identificado explicitamente em versao futura para evitar ambiguidade entre transmissores e receptores.

Experimento Python inicial criado com helpers puros de repeticao de bits e voto majoritario. Proximo passo: integrar opcionalmente em uma varredura separada para medir ganho real contra AWGN/fading antes de qualquer mudanca no core ou no protocolo.

Varredura experimental `python-sim/repetition_sweep.py` adicionada para comparar fatores como 1x e 3x sob AWGN, medindo BER recuperada, sucesso de CRC/payload, confianca e multiplicador de duracao.

A varredura de repeticao tambem aceita fading por blocos opcional, para observar quando repeticao simples deixa de ser suficiente e passa a exigir interleaving.

Tarefa 7.5 — Implementar interleaving

Espalhar erros no tempo.

Experimento Python inicial criado com helpers puros de interleaving/deinterleaving por blocos retangulares completos. Proximo passo: combinar interleaving + repeticao na varredura experimental para medir ganho em fading por blocos.

`python-sim/repetition_sweep.py` agora aceita interleaving experimental por blocos completos com `--interleave-rows` e `--interleave-columns`, aplicado apos a repeticao e revertido antes do voto majoritario para espalhar copias repetidas no tempo.

`python-sim/interleaving_sweep.py` foi adicionado para comparar varias geometrias de interleaving no mesmo cenario. O script calcula o tamanho do quadro repetido, seleciona apenas blocos retangulares que encaixam exatamente e grava `summary.csv`/`trials.csv` para orientar uma futura decisao de protocolo.

A varredura de interleaving tambem inclui por padrao uma linha de base com repeticao sem interleaving, permitindo comparar diretamente se uma geometria melhora CRC, payload e BER no mesmo cenario.

A varredura grava tambem `best_summary.csv`, destacando a melhor opcao por SNR com prioridade para sucesso de CRC, payload valido e menor BER.

Resultado experimental inicial: com payload `pu5lrk Teste`, repeticao 3x, SNR -12 dB, fading por blocos de 4 simbolos e 50 sementes, a linha de base sem interleaving teve 0% de CRC valido, enquanto a geometria `12x46` atingiu 18% de CRC valido e a menor BER media do conjunto testado. Esse resultado nao altera o protocolo; apenas orienta proximas varreduras.

Varredura posterior por SNR, com a mesma mensagem curta e 50 sementes por ponto, mostrou que o ganho de interleaving aparece principalmente perto da zona de transicao: em -18 dB e -15 dB nenhuma geometria fechou CRC; em -12 dB a melhor geometria foi `6x92` com 12% de CRC valido; em -9 dB foi `12x46` com 86%; em -6 dB foi `12x46` com 100%.

Comparacao inicial por tamanho de mensagem em -12 dB indica que a geometria ideal depende do tamanho do quadro: payload `pu5lrk Ok` teve melhor resultado com `14x36` e 16% de CRC valido; payload `pu5lrk Teste` com `6x92` e 12%; payload `pu5lrk Mensagem maior para teste` com `8x114` e 4%. Isso sugere que um modo futuro nao deve fixar uma geometria unica sem antes definir como ela escala com o tamanho do quadro.

Tarefa 7.6 — Implementar FEC simples

Implementar código convolucional ou outro FEC simples.

Experimento Python inicial criado com Hamming(7,4) em `python-sim/hftext/fec.py`. A implementacao codifica 4 bits em 7 bits, corrige 1 erro por codeword, preserva o tamanho original por truncamento apos decode e ainda nao altera o protocolo nem os scripts TX/RX normais.

`python-sim/fec_sweep.py` foi adicionado para comparar `raw` e `hamming74` sob AWGN e fading por blocos, medindo overhead de duracao, BER no canal codificado, BER recuperada, CRC/payload e quantidade de codewords corrigidas.

A varredura FEC tambem aceita interleaving experimental sobre o fluxo ja codificado. Resultado inicial com payload `pu5lrk Teste`, SNR -12 dB, fading por blocos de 4 simbolos e 50 sementes: `raw` teve 0% de CRC e BER recuperada 0.082935; `hamming74` sem interleaving teve 0% de CRC e BER 0.056196; `hamming74` com interleaving `14x23` teve 6% de CRC e BER 0.044130, com overhead de 1.75x. O ganho existe, mas ainda fica abaixo de repeticao 3x + interleaving nesse cenario, embora use menos tempo de transmissao.

`python-sim/fec_interleaving_sweep.py` foi adicionado para varrer automaticamente geometrias de interleaving sobre Hamming(7,4), incluindo linha de base sem interleaving e `best_summary.csv` por SNR.

Varredura automatica inicial com payload `pu5lrk Teste`, SNR -12 dB, fading por blocos de 4 simbolos e 50 sementes confirmou ganho limitado para Hamming(7,4): linha de base `hamming74` teve 0% de CRC e BER 0.056848; melhor geometria foi `14x23`, com 2% de CRC e BER 0.051413. Isso reforca que Hamming(7,4) serve como referencia simples, mas provavelmente nao e FEC suficiente para o modo robusto principal.

Foi iniciado um segundo helper FEC em Python: codigo convolucional rate 1/2, K=3, geradores `111` e `101`, com decoder Viterbi hard-decision. Nesta etapa ele esta apenas em `python-sim/hftext/fec.py` com testes unitarios; a integracao em varredura e medicao contra Hamming/repeticao ficam para o proximo passo.

`python-sim/fec_sweep.py` passou a aceitar o modo `conv_k3` junto de `raw` e `hamming74`, registrando tambem `decoder_distance` do Viterbi. Resultado inicial com payload `pu5lrk Teste`, SNR -12 dB, fading por blocos de 4 simbolos e 50 sementes: `raw` teve 0% de CRC e BER 0.082935; `hamming74` teve 0% de CRC e BER 0.056196; `conv_k3` teve 8% de CRC e BER 0.045543, com overhead aproximado de 2.02x por causa dos bits de cauda.

`python-sim/fec_interleaving_sweep.py` foi generalizado para aceitar `hamming74` ou `conv_k3`. Varredura inicial com `conv_k3`, payload `pu5lrk Teste`, SNR -12 dB, fading por blocos de 4 simbolos e 50 sementes mostrou ganho relevante com interleaving: linha de base `conv_k3` teve 12% de CRC valido e BER 0.040326; melhor geometria `6x62` atingiu 30% de CRC valido e BER 0.034457, mantendo overhead aproximado de 2.02x. Esse e o melhor candidato experimental ate aqui entre os FECs simples testados.

Varredura posterior por SNR com `conv_k3 + interleaving`, payload `pu5lrk Teste` e 50 sementes por ponto: em -18 dB e -15 dB nenhuma configuracao fechou CRC; em -12 dB a melhor geometria foi `6x62` com 26% de CRC valido; em -9 dB foi `4x93` com 96%; em -6 dB chegou a 100%. Comparado a repeticao 3x + interleaving no mesmo conjunto de SNRs, `conv_k3 + interleaving` teve melhor taxa de CRC em -12 dB e -9 dB com overhead menor, embora a BER media nem sempre seja menor. Esse resultado sugere `conv_k3 + interleaving` como candidato principal para um futuro modo robusto experimental.

Comparacao posterior por tamanho de mensagem em -12 dB, fading por blocos de 4 simbolos e 50 sementes mostrou que o ganho tambem depende do tamanho do quadro: payload `pu5lrk Ok` teve melhor resultado com `conv_k3 + int5x68`, 36% de CRC valido; payload `pu5lrk Teste` com `conv_k3 + int6x62`, 26%; payload `pu5lrk Mensagem maior para teste` com `conv_k3 + int4x153`, 12%. Isso reforca que qualquer modo robusto futuro deve derivar a geometria de interleaving do tamanho codificado, em vez de fixar uma matriz unica.

Conclusao de implementacao: o modo robusto principal e `conv_k3 + interleaving`, mantendo o frame logico v0.1 antes do FEC e aplicando FEC/interleaving apenas no fluxo de bits transmitido.

Algoritmo deterministico inicial criado em Python: `choose_interleave_shape(bit_count, preferred_rows=6, min_rows=2, max_rows=16)`. Ele escolhe uma geometria completa cujo numero de linhas divide exatamente o tamanho codificado, priorizando a proximidade de 6 linhas e usando o menor numero de linhas em empates. O `fec_interleaving_sweep.py` agora aceita `--auto-shape` para testar essa regra sem varrer todas as geometrias.

Portabilidade para C++ iniciada: `core/include/hftext_robust.h` e `core/src/robust.cpp` implementam helpers puros para `conv_k3`, Viterbi hard-decision, Viterbi soft-decision, interleaving/deinterleaving e escolha deterministica de geometria. A primeira etapa adicionou testes unitarios em `core/tests/test_robust.cpp`.

O core C++ agora tambem possui montagem e decodificacao de frame robusto em bits: `buildRobustFrameBits` monta o frame logico v0.1, aplica `conv_k3` e interleaving deterministico; `parseRobustFrameBits` desfaz interleaving, executa Viterbi hard-decision e valida o frame logico com CRC normal. Em recepcao por audio, `parseRobustFrameSoftBits` usa a confianca por simbolo no Viterbi soft-decision.

O core C++ tambem monta uma transmissao robusta em bits com preambulo e `START_SYNC` fisico (`buildRobustTransmission`). O RX procura o `START_SYNC` no fluxo demodulado e decodifica o bloco robusto seguinte, aceitando apenas candidatos cujo frame logico recuperado tenha CRC e payload validos.

O modo robusto foi promovido a modo unico do sistema. As APIs publicas `modulateText` e `demodulateSamples`, os CLIs WAV e o app PC usam sempre `conv_k3 + interleaving`; nao ha opcao operacional para desligar FEC/interleaving.

O app PC usa sempre o modo robusto. A estimativa TX reflete o fluxo robusto, `Gerar WAV` usa `modulateText` robusto e `Decodificar WAV`/RX salvo usam `demodulateSamples` robusto.

## Estado atual do backlog

As tarefas de simulacao Python, core C++ e CLI WAV possuem implementacao inicial.

A tarefa 5.1 foi iniciada com `pc-app/`: janela Qt Widgets minima, campo de indicativo, campo de mensagem, configuracao de sample rate, duracao de simbolo, tom 0, tom 1, amplitude e preambulo, botao `Gerar WAV`, botao `Decodificar WAV`, area de saida e log. O app chama `hftext_core` por meio de `ModemController` e usa o utilitario WAV do core.

O alvo `hftext_pc` e construido somente quando Qt6 Widgets esta instalado; sem Qt, o CMake configura e testa o restante do projeto normalmente.

A tarefa 5.2 foi iniciada com reproducao explicita de WAV no app PC: `AudioOutput`, selecao de dispositivo de saida, botao `Transmitir WAV` e botao `Parar TX`.

A tarefa 5.3 evoluiu para recepcao continua no app PC: `AudioInput`, selecao de dispositivo de entrada, botao `Receber`, botao `Parar RX`, indicador simples de nivel RX e decodificacao em thread de segundo plano por `StreamingReceiver`. WAV permanece como ferramenta de debug.

A tarefa 5.4 foi iniciada com a estimativa TX ao vivo no app PC: o campo `Estimativa TX` mostra simbolos de payload, limite de 127 simbolos, bits totais transmitidos e duracao aproximada. A contagem inclui o indicativo automatico e letras maiusculas como `shift` + letra.

A tarefa 5.5 foi iniciada com sanitizacao visual da mensagem TX: caracteres invalidos sao substituidos por `?` diretamente no campo de mensagem, usando o alfabeto do core como referencia.

A tarefa 5.6 foi iniciada com a barra `Progresso TX` no app PC. Ela consulta o estado de `AudioOutput`, acompanha a posicao aproximada do WAV em reproducao e retorna ao inicio quando `Parar TX` interrompe a transmissao.

A tarefa 5.7 foi iniciada com `WaterfallWidget` no app PC. A primeira versao e visual, mostra energia aproximada de 300 Hz a 3 kHz durante a captura RX e nao altera a decodificacao. A waterfall tambem passou a mostrar linhas verticais amarelas nos tons RX configurados, permitindo comparar visualmente as trilhas recebidas com `Tom 0` e `Tom 1` para ajuste de sintonia.

A interface do app PC foi reorganizada em abas: `Operacao` para uso normal e `Configuracao` para parametros do modem e dispositivos. Essa separacao reduz a poluicao visual sem alterar o fluxo de TX/RX.

O app PC tambem passou a permitir salvar o log operacional em arquivo `.txt`, preservando timestamps e eventos de recepcao para analise posterior dos testes de campo. O arquivo exportado inclui cabecalho com configuracao de modem, dispositivos selecionados e estado do log detalhado.

O app PC passou a salvar localmente as configuracoes operacionais ao fechar e restaura-las na proxima abertura: indicativo, sample rates, duracao de simbolo, tons, amplitude, preambulo, dispositivos selecionados, estado do log detalhado e geometria da janela. A mensagem TX nao e persistida.

O app PC tambem passou a ter um recurso manual de evidencia de campo: `Salvar Evidencia RX` grava um WAV com a janela circular de audio RX recente e um TXT com configuracao, texto recebido e log. A primeira janela e limitada aos ultimos 300 segundos para permitir depuracao de recepcao real sem crescimento indefinido durante RX continuo.

O app PC passou a mostrar uma linha `Estado RX` na aba de operacao. Ela resume os eventos do `StreamingReceiver` em estado recente, ultimo `PHYS_LENGTH`, qualidade do ultimo candidato completo e ultimo motivo de rejeicao, ajudando testes em campo sem exigir log detalhado ligado o tempo todo. Depois dos testes com pacotes parciais, essa linha passou a priorizar quadro valido e frame em recebimento sobre rejeicoes isoladas do mesmo lote, reduzindo oscilacao visual. A barra `Progresso RX` tambem passou a seguir um candidato forte por vez e avancar de forma monotona dentro dele, voltando a zero apenas em inicio/parada de RX ou rejeicao forte completa. O log normal e `Sessao RX` passaram a contar rejeicoes fortes, mantendo candidatos fracos apenas no `Log RX detalhado`.

O app PC tambem passou a mostrar `Sessao RX`, um resumo com duracao e contadores reiniciados a cada nova recepcao: quadros aceitos, candidatos rejeitados, `PHYS_LENGTH` recuperados e candidatos de sync forte. Esses contadores sao consolidados como o log operacional normal; a telemetria bruta por fase permanece no log detalhado. O resumo aparece ao parar RX e tambem entra no cabecalho dos logs/evidencias exportados.

Foi adotada uma pasta local `logs/` para evidencias manuais de campo. Ela e ignorada pelo Git e pode ser limpa entre rodadas, pois os arquivos `.wav` e `.txt` gerados ali sao material temporario de depuracao. O roteiro atual de testes de campo foi documentado em `docs/08_testes_validacao.md`, com preferencia por log simples e uso de log detalhado apenas para falhas ou analise por fase.

O TXT gerado por `Salvar Evidencia RX` passou a incluir uma secao `Resumo CSV`, com uma linha de cabecalho e uma linha de valores contendo configuracao, duracao da sessao, contadores RX, ultimo diagnostico, texto recebido e caminho do WAV. O CSV tambem preserva tamanho, qualidade, offset e fases/tentativas do ultimo quadro aceito da sessao, para que uma evidencia salva depois do aceite continue mantendo os dados uteis do quadro valido. A intencao e facilitar comparar varias rodadas de campo em planilha sem alterar a captura de audio nem o protocolo.

Refinamento posterior: o TXT de evidencia tambem passou a incluir a secao
`Quadros aceitos CSV`, com uma linha por quadro aceito pelo RX continuo. Essa
tabela preserva a configuracao no momento do aceite, evitando que testes mistos
em uma mesma sessao, como 0,1 s seguido de 0,3 s ou 2-FSK seguido de 4-FSK,
fiquem resumidos apenas pela configuracao final da janela.

Foi adicionado o utilitario `python-sim/field_summary.py` para consolidar os blocos `Resumo CSV` de varios TXT de evidencia em um unico CSV agregado. Ele preserva o caminho de origem em `source_txt` e permite comparar rapidamente taxas de aceite, qualidade e parametros de campo entre rodadas salvas em `logs/`. O mesmo script tambem pode gerar `field_summary_groups.csv`, agrupado por parametros de modem, com taxa de aceite, qualidade media/minima e medias dos contadores RX.

O mesmo utilitario tambem gera `field_frames.csv` a partir de `Quadros aceitos
CSV`, oferecendo uma tabela por transmissao aceita. Como evidencias salvas
durante a mesma sessao acumulam os aceites anteriores, `field_frames.csv`
deduplica automaticamente quadros repetidos por instante/configuracao/texto. O replay offline
`python-sim/field_replay.py` passou a respeitar a modulacao registrada no resumo
da evidencia, evitando tentar WAVs 4-FSK como se fossem 2-FSK.

Foi adicionado tambem o utilitario `python-sim/field_replay.py` para reproduzir WAVs de evidencias aceitas pelo CLI C++ `hftext_rx_wav`, usando os parametros registrados no `Resumo CSV`. Ele gera `field_replay.csv` com esperado, decodificado, codigo de retorno e status, permitindo reaproveitar capturas reais como regressao manual do decoder offline.

Analise dos primeiros logs radio-SDR de 2026-06-07 mostrou que WAVs rejeitados durante RX continuo eram decodificaveis pelo `hftext_rx_wav` offline quando este usava sua pequena busca de deslocamento comum dos tons. O `StreamingReceiver` foi alinhado com essa estrategia e passou a manter variantes de frequencia em torno dos tons configurados, sem alterar o protocolo ou a regra de aceitacao por CRC.

Teste real de 2026-06-14 com simbolos de `0,5 s` confirmou boa sensibilidade do RX, inclusive com sinais progressivamente mais fracos. Uma evidencia no limite ainda era decodificavel pelo caminho offline quando os tons recebidos eram tratados como cerca de `+7,5 Hz` acima do configurado, entao o `StreamingReceiver` passou a incluir esse passo intermediario de deslocamento comum. Tambem foi corrigida a contagem de quadros aceitos na interface para usar o resultado decodificado, nao apenas eventos diagnosticos limitados por lote.

Nova rodada de campo no mesmo dia validou a correcao: a condicao anterior foi recebida, uma condicao quase inaudivel ao ouvido humano tambem foi recebida, e a falha com ruido branco forte nao foi recuperada nem pelo decoder WAV offline. O comportamento atual parece coerente com limite de SNR, mantendo a proxima prioridade em coleta comparativa e nao em mudanca imediata de protocolo.

Comparativo posterior com simbolos de `0,8 s`, `0,5 s` e `0,3 s` mostrou que `0,8 s` pode ajudar como referencia de robustez, mas e operacionalmente longo demais para uso normal em HF. `0,5 s` permanece como baseline mais equilibrado, e `0,3 s` aparenta ser promissor quando o canal esta razoavel. Falhas repetidas de `0,5 s` no conjunto mais ruidoso tambem falharam no decoder WAV offline, sugerindo limite real de SNR/canal em vez de bug especifico de streaming.

Novos testes de campo com `0,3 s` em diferentes niveis de sinal e ruido indicaram recepcao muito boa; uma tentativa com `0,1 s` e bastante ruido tambem passou. Isso reforca a direcao de reduzir a ocupacao do canal e tratar `0,3 s` como candidato operacional principal, mantendo `0,5 s` como referencia robusta.

A interface do PC app foi reorganizada em formato de chat: historico de mensagens recebidas no topo, waterfall no meio, barra discreta de progresso TX e campo de mensagem com botao iconografico de enviar/parar na parte inferior. A transmissao normal agora gera e toca o audio diretamente, sem exigir salvar WAV antes; geracao/decodificacao WAV, controles RX, logs, evidencia e indicativo ficam na aba `Configuracao` como operacao avancada/debug.

Refinamento operacional aplicado: o app PC agora inicia o RX automaticamente ao abrir quando ha dispositivo de entrada disponivel, pois a recepcao continua e o uso normal esperado. Para permitir escutas longas, o historico interno de captura em `AudioInput` foi limitado e a contagem total de amostras ficou separada do audio armazenado. Com RX ativo, mudancas em sample rate RX, duracao de simbolo, tons, preambulo, entrada de audio ou log detalhado reiniciam a escuta automaticamente com a nova configuracao. A waterfall tambem passou a usar trilhas verdes para sinal fraco/normal, amarelas perto de saturacao e vermelhas quando o bloco de entrada satura.

Foi adicionado o CLI C++ `hftext_stream_wav` para alimentar WAVs salvos ao `StreamingReceiver` em blocos, permitindo reproduzir capturas reais pelo mesmo caminho incremental usado pelo app PC. Ele complementa `hftext_rx_wav`, que continua sendo o decoder offline com busca mais ampla de WAV fechado.

O protocolo HFText Basic v0.1 foi consolidado como baseline operacional para validacao de campo: `2-FSK + START_SYNC + PHYS_LENGTH + conv_k3 + interleaving + Viterbi soft-decision + CRC`. Novos modos incompativeis, como repeticao operacional, ACK, 4-FSK ou 8-FSK, devem ser tratados como v0.2 ou posterior.
