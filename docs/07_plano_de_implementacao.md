# Plano de implementação

## Fase 0 — Preparação do repositório

Objetivo:
Criar estrutura inicial do projeto.

Tarefas:
- criar pastas;
- criar documentação;
- configurar Git;
- criar README;
- criar AGENTS.md;
- criar CMake inicial;
- criar ambiente Python.

Resultado esperado:
Projeto organizado e pronto para implementação.

## Fase 1 — Simulação Python 2-FSK

Objetivo:
Validar transmissão e recepção em arquivo WAV.

Tarefas:
- implementar codificação de texto reduzida;
- implementar CRC16;
- implementar geração de bits;
- implementar modulação 2-FSK;
- salvar WAV;
- carregar WAV;
- demodular por Goertzel;
- recuperar texto;
- adicionar ruído simulado.

Resultado esperado:
Um script Python deve conseguir codificar texto, gerar WAV, ler o WAV e recuperar o texto.

## Fase 2 — Núcleo C++

Objetivo:
Criar implementação definitiva inicial.

Tarefas:
- criar biblioteca `hftext_core`;
- implementar encoder;
- implementar frame;
- implementar CRC16;
- implementar modulador 2-FSK;
- implementar demodulador 2-FSK;
- criar testes unitários.

Resultado esperado:
Testes C++ passando para codificação, CRC, modulação e demodulação.

## Fase 3 — Ferramentas CLI

Objetivo:
Criar ferramentas simples de linha de comando.

Tarefas:
- `hftext_tx_wav`: texto para WAV;
- `hftext_rx_wav`: WAV para texto.

Resultado esperado:
Usuário consegue gerar e decodificar WAV pelo terminal.

## Fase 4 — Aplicação PC mínima

Objetivo:
Criar interface gráfica simples.

Tarefas:
- criar projeto Qt;
- campo de texto;
- botão gerar WAV;
- botão decodificar WAV;
- exibir resultado.

Resultado esperado:
Aplicação PC offline funcional.

## Fase 5 — Áudio em tempo real no PC

Objetivo:
Transmitir e receber pela placa de som.

Tarefas:
- selecionar dispositivo de saída;
- reproduzir áudio;
- selecionar dispositivo de entrada;
- capturar áudio;
- mostrar nível;
- demodular áudio capturado.

Resultado esperado:
Teste com cabo de áudio ou rádio real.

## Fase 6 — Melhorias do modem

Objetivo:
Aumentar robustez.

Tarefas:
- melhorar preâmbulo;
- detectar início de quadro;
- estimar offset temporal;
- implementar 4-FSK;
- implementar 8-FSK;
- implementar repetição;
- implementar interleaving;
- implementar FEC simples.

Resultado esperado:
Melhor desempenho em ruído e fading.

## Fase 7 — Aplicação Android TX

Objetivo:
Transmitir texto por áudio no Android.

Tarefas:
- criar projeto Android;
- tela principal;
- campo de mensagem;
- gerar áudio;
- reproduzir áudio.

Resultado esperado:
Celular transmite áudio modulado.

## Fase 8 — Aplicação Android RX

Objetivo:
Receber e decodificar áudio no Android.

Tarefas:
- permissão de microfone;
- captura com AudioRecord;
- bufferização;
- chamada ao núcleo;
- exibição do texto recebido.

Resultado esperado:
Celular recebe áudio e mostra mensagem.

## Fase 9 — Testes em rádio HF

Objetivo:
Validar em condições reais.

Tarefas:
- teste com áudio local;
- teste com dois computadores;
- teste com rádio em baixa potência;
- teste em HF;
- medir taxa de sucesso;
- ajustar níveis de áudio.

Resultado esperado:
Comunicação de texto curta via rádio.

## Estado atual

Fases 1, 2 e 3 possuem implementacao inicial validada por testes automatizados.

A Fase 4 foi iniciada com uma aplicacao PC offline em `pc-app/`, usando Qt Widgets, `hftext_core`, geracao de WAV e decodificacao de WAV. O alvo do app e ignorado pelo CMake quando Qt6 Widgets nao esta instalado.
