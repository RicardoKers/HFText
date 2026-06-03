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

Refinamento iniciado: o parser agora testa multiplos candidatos de `SYNC` no fluxo demodulado e aceita o primeiro quadro posterior com CRC e payload validos, em vez de desistir no primeiro `SYNC` falso causado por ruido.

Tambem foi iniciada uma metrica diagnostica de confianca no demodulador, calculada pela separacao relativa entre as energias dos dois tons. Essa metrica deve ajudar logs e interface no futuro, mas nao substitui CRC.

Tarefa 7.3 — Implementar 4-FSK

Adicionar modo 4-FSK.

Tarefa 7.4 — Implementar repetição

Enviar bits ou símbolos repetidos.

Antes de implementar no protocolo definitivo, criar experimento Python isolado de repeticao por bit ou simbolo com voto majoritario no RX. O HFText Basic v0.1 permanece sem repeticao; qualquer modo repetido deve ser identificado explicitamente em versao futura para evitar ambiguidade entre transmissores e receptores.

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

Tarefa 7.6 — Implementar FEC simples

Implementar código convolucional ou outro FEC simples.

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
