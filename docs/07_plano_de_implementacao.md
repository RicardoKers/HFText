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

Melhoria aplicada nesta fase: o fluxo fisico passou a usar `PREAMBLE | START_SYNC | ROBUST_FRAME`. O `START_SYNC` e transmitido diretamente antes do bloco robusto, permitindo que o RX encontre o inicio dos dados antes de aplicar Viterbi, em vez de testar muitos candidatos de bloco robusto.

Outra melhoria iniciada: o demodulador passou a calcular uma confianca media baseada na separacao relativa entre as energias dos tons 0 e 1. A confianca e diagnostica e nao altera a regra de aceitacao por CRC.

A investigacao de robustez em Python apontou `conv_k3 + interleaving` como modo principal: codigo convolucional rate 1/2, K=3, geradores `111` e `101`, Viterbi hard-decision e interleaving retangular sobre o fluxo codificado. O HFText v0.1 usa esse modo sempre; o frame simples `SYNC | LENGTH | PAYLOAD | CRC16` permanece como frame logico interno antes do FEC.

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

A Fase 5 foi iniciada pelos fluxos basicos de audio: o `pc-app/` possui `AudioOutput`, selecao de dispositivo de saida, botao `Transmitir WAV`, botao `Parar TX`, `AudioInput`, selecao de dispositivo de entrada, botao `Receber`, botao `Parar RX`, indicador simples de nivel RX, metricas basicas da captura e decodificacao automatica do WAV salvo ao parar RX. A demodulacao em tempo real fica para etapas posteriores.

A preparacao para RX continuo foi iniciada no core com `StreamingReceiver`, testado com audio gerado pelo core e entregue em blocos. A integracao direta desse receptor no app foi adiada porque executar a decodificacao offline dentro do caminho de captura pode atrasar a reciclagem dos buffers de audio e encurtar o WAV gravado. Por enquanto, o app prioriza capturar o WAV completo e decodificar automaticamente ao parar RX.

As proximas melhorias recomendadas para a Fase 5 sao incrementais e focadas em operacao:

- contador de simbolos do payload TX durante a digitacao, contando `shift` como simbolo;
- estimativa de duracao total da transmissao com os parametros atuais;
- sanitizacao visual da mensagem TX, substituindo caracteres invalidos por `?` antes da transmissao;
- barra de progresso durante a reproducao TX;
- waterfall RX simples, apenas visual, depois que a captura estiver estavel.

O contador de simbolos e a estimativa de duracao foram implementados no app PC como primeira melhoria operacional. Eles consideram o indicativo inserido automaticamente no payload, o limite de 127 simbolos, o preambulo configurado e a duracao de simbolo atual.

A sanitizacao visual da mensagem TX tambem foi iniciada: caracteres fora do alfabeto HFText sao substituidos por `?` diretamente no campo de mensagem durante a digitacao.

O alfabeto v0.1 foi expandido para portugues usando os tres simbolos restantes: `acute`, `tilde` e `ç`. Vogais acentuadas passam a consumir dois simbolos, vogais acentuadas maiusculas consomem tres simbolos, `ç` consome um simbolo e `Ç` consome dois.

A barra de progresso TX foi adicionada ao app PC. Ela acompanha a posicao aproximada do WAV em reproducao e volta ao inicio quando o operador interrompe a transmissao.

A primeira waterfall RX tambem foi adicionada ao app PC como visualizacao leve. Ela mostra energia aproximada no tempo e na frequencia durante a captura, sem participar da decodificacao nem executar no caminho critico de reciclagem dos buffers de audio.

A interface do app PC foi reorganizada em abas para reduzir poluicao visual: `Operacao` concentra o fluxo normal de TX/RX, enquanto `Configuracao` concentra parametros do modem, dispositivos de audio e log. A area de texto recebido acumula novas mensagens em formato de historico.

Como o texto recebido agora e um historico, a interface tambem possui `Limpar RX` para limpar somente a area de mensagens recebidas, preservando log, WAVs e configuracoes.
