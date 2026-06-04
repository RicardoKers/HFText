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

Refinamento aplicado: o fluxo fisico agora usa `PREAMBLE | START_SYNC | ROBUST_FRAME`. O `START_SYNC` e transmitido diretamente antes do bloco robusto para marcar o inicio dos dados e reduzir a busca lenta por candidatos Viterbi.

Tambem foi iniciada uma metrica diagnostica de confianca no demodulador, calculada pela separacao relativa entre as energias dos dois tons. Essa metrica deve ajudar logs e interface no futuro, mas nao substitui CRC.

Tarefa 7.3 — Implementar 4-FSK

Adicionar modo 4-FSK.

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

Portabilidade para C++ iniciada: `core/include/hftext_robust.h` e `core/src/robust.cpp` implementam helpers puros para `conv_k3`, Viterbi hard-decision, interleaving/deinterleaving e escolha deterministica de geometria. A primeira etapa adicionou testes unitarios em `core/tests/test_robust.cpp`.

O core C++ agora tambem possui montagem e decodificacao de frame robusto em bits: `buildRobustFrameBits` monta o frame logico v0.1, aplica `conv_k3` e interleaving deterministico; `parseRobustFrameBits` desfaz interleaving, executa Viterbi e valida o frame logico com CRC normal.

O core C++ tambem monta uma transmissao robusta em bits com preambulo e `START_SYNC` fisico (`buildRobustTransmission`). O RX procura o `START_SYNC` no fluxo demodulado e decodifica o bloco robusto seguinte, aceitando apenas candidatos cujo frame logico recuperado tenha CRC e payload validos.

O modo robusto foi promovido a modo unico do sistema. As APIs publicas `modulateText` e `demodulateSamples`, os CLIs WAV e o app PC usam sempre `conv_k3 + interleaving`; nao ha opcao operacional para desligar FEC/interleaving.

O app PC usa sempre o modo robusto. A estimativa TX reflete o fluxo robusto, `Gerar WAV` usa `modulateText` robusto e `Decodificar WAV`/RX salvo usam `demodulateSamples` robusto.

## Estado atual do backlog

As tarefas de simulacao Python, core C++ e CLI WAV possuem implementacao inicial.

A tarefa 5.1 foi iniciada com `pc-app/`: janela Qt Widgets minima, campo de indicativo, campo de mensagem, configuracao de sample rate, duracao de simbolo, tom 0, tom 1, amplitude e preambulo, botao `Gerar WAV`, botao `Decodificar WAV`, area de saida e log. O app chama `hftext_core` por meio de `ModemController` e usa o utilitario WAV do core.

O alvo `hftext_pc` e construido somente quando Qt6 Widgets esta instalado; sem Qt, o CMake configura e testa o restante do projeto normalmente.

A tarefa 5.2 foi iniciada com reproducao explicita de WAV no app PC: `AudioOutput`, selecao de dispositivo de saida, botao `Transmitir WAV` e botao `Parar TX`.

A tarefa 5.3 foi iniciada com captura explicita de WAV no app PC: `AudioInput`, selecao de dispositivo de entrada, botao `Receber`, botao `Parar RX` e indicador simples de nivel RX. A demodulacao em tempo real ainda nao foi implementada.

A tarefa 5.4 foi iniciada com a estimativa TX ao vivo no app PC: o campo `Estimativa TX` mostra simbolos de payload, limite de 127 simbolos, bits totais transmitidos e duracao aproximada. A contagem inclui o indicativo automatico e letras maiusculas como `shift` + letra.

A tarefa 5.5 foi iniciada com sanitizacao visual da mensagem TX: caracteres invalidos sao substituidos por `?` diretamente no campo de mensagem, usando o alfabeto do core como referencia.

A tarefa 5.6 foi iniciada com a barra `Progresso TX` no app PC. Ela consulta o estado de `AudioOutput`, acompanha a posicao aproximada do WAV em reproducao e retorna ao inicio quando `Parar TX` interrompe a transmissao.

A tarefa 5.7 foi iniciada com `WaterfallWidget` no app PC. A primeira versao e visual, mostra energia aproximada de 300 Hz a 3 kHz durante a captura RX e nao altera a decodificacao.

A interface do app PC foi reorganizada em abas: `Operacao` para uso normal e `Configuracao` para parametros do modem e dispositivos. Essa separacao reduz a poluicao visual sem alterar o fluxo de TX/RX.
