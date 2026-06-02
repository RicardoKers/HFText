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

Tarefa 7.3 — Implementar 4-FSK

Adicionar modo 4-FSK.

Tarefa 7.4 — Implementar repetição

Enviar bits ou símbolos repetidos.

Tarefa 7.5 — Implementar interleaving

Espalhar erros no tempo.

Tarefa 7.6 — Implementar FEC simples

Implementar código convolucional ou outro FEC simples.
