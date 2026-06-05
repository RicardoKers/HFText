# Aplicação PC

## Objetivo

Criar uma aplicação para PC que permita testar o modem em ambiente real com placa de som e rádio.

## Plataforma recomendada

C++ com Qt.

## Requisitos da primeira versão

A primeira versão da aplicação PC deve conter:

- campo de texto para mensagem;
- campo de indicativo do usuário;
- botão "Gerar WAV";
- botão "Transmitir";
- botão "Receber";
- área de texto recebido;
- seleção de dispositivo de saída;
- seleção de dispositivo de entrada;
- indicador de nível de áudio RX;
- indicador de nível de áudio TX;
- log de eventos.

## Recursos desejados

- salvar WAV transmitido;
- carregar WAV recebido;
- decodificar WAV offline;
- mostrar contagem de simbolos do payload TX durante a digitacao;
- mostrar duracao estimada da transmissao antes de gerar ou transmitir;
- mostrar progresso durante a transmissao;
- substituir caracteres invalidos por `?` diretamente no campo de mensagem TX, antes da transmissao;
- mostrar espectro básico;
- mostrar waterfall futuramente;
- salvar log de recepção.

## Arquitetura da aplicação PC

```text
pc-app/
├── main.cpp
├── MainWindow.cpp
├── MainWindow.h
├── AudioInput.cpp
├── AudioInput.h
├── AudioOutput.cpp
├── AudioOutput.h
├── ModemController.cpp
├── ModemController.h
└── CMakeLists.txt
Responsabilidades
MainWindow
Interface gráfica.
Botões.
Campos.
Exibição de mensagens.
AudioInput
Captura de áudio.
Bufferização.
Conversão para float.
AudioOutput
Reprodução de áudio.
Controle de volume.
ModemController
Conecta UI, áudio e núcleo C++.
Não deve implementar DSP diretamente.
MVP PC historico

O primeiro MVP podia ignorar recepção em tempo real.

Fluxo mínimo:

usuário configura indicativo e digita texto;
aplicação chama modulateText;
aplicação salva WAV;
usuário testa WAV externamente;
aplicação carrega WAV;
aplicação chama demodulateSamples;
aplicação mostra texto decodificado.

## Estado atual da Fase 4

A primeira fatia do `pc-app/` e uma aplicacao Qt Widgets offline:

- campo de indicativo;
- campo de mensagem;
- botao `Gerar WAV`;
- botao `Transmitir WAV`;
- botao `Parar TX`;
- botao `Receber`;
- botao `Parar RX`;
- botao `Decodificar WAV`;
- botao `Limpar RX` para limpar apenas o historico visual de texto recebido;
- configuracao de sample rate TX/WAV, sample rate RX, duracao de simbolo, tom 0, tom 1, amplitude e preambulo;
- opcao `Log RX detalhado` para alternar entre log operacional resumido e telemetria completa de debug;
- estimativa TX ao vivo com simbolos de payload, bits totais e duracao aproximada;
- sanitizacao visual da mensagem TX, preservando os acentos suportados do portugues e substituindo caracteres invalidos por `?` durante a digitacao;
- barra de progresso durante a reproducao TX;
- selecao de dispositivo de saida de audio;
- selecao de dispositivo de entrada de audio;
- indicador simples de nivel RX;
- indicador simples de qualidade RX baseado na confianca media do demodulador;
- waterfall RX simples para observacao visual do audio recebido;
- area de texto recebido;
- log simples.

A interface agora separa operacao e configuracao em abas:

- `Operacao`: indicativo, mensagem, estimativa TX, niveis/progresso, qualidade RX, waterfall, botoes e texto recebido;
- `Configuracao`: sample rates, duracao de simbolo, tons, amplitude, preambulo, dispositivos de audio e log.

A area `Texto recebido` funciona como historico: novas mensagens ou resultados de decodificacao sao adicionados abaixo dos anteriores.
O botao `Limpar RX` limpa esse historico visual sem apagar WAVs, log ou configuracoes.

O app usa `ModemController` apenas como ponte entre a interface, `hftext_core` e o utilitario de leitura/escrita WAV. Ele nao implementa logica DSP.

O `pc-app/` e incluido pelo CMake raiz, mas e ignorado automaticamente quando `Qt6 Widgets` nao esta instalado no ambiente.

## Estado atual da Fase 5

A reproducao de audio TX foi iniciada no `pc-app/` com `AudioOutput`.

Nesta etapa, o botao `Transmitir WAV` reproduz explicitamente o ultimo WAV gerado no dispositivo de saida selecionado, ou permite escolher um WAV se nenhum arquivo tiver sido gerado na sessao. O botao `Parar TX` interrompe a reproducao.

Ainda nao ha selecao avancada de formato de audio ou controle automatico de ganho.

A captura RX basica tambem foi iniciada com `AudioInput`.

Nesta etapa, o botao `Receber` inicia escuta continua pelo dispositivo de entrada selecionado. Os blocos de audio capturados alimentam o `StreamingReceiver` em uma thread de segundo plano, enquanto a interface continua atualizando `Nivel RX`, qualidade e waterfall.

Quando o `StreamingReceiver` encontra um quadro com CRC e payload validos, o app adiciona a mensagem ao historico de `Texto recebido` e registra offset/fases testadas e confianca media no log.

O log do app inclui timestamp em cada linha. Por padrao, o RX continuo mostra um resumo operacional com sync forte, tamanho fisico, progresso consolidado, rejeicoes agregadas e quadro valido. A opcao `Log RX detalhado` mostra a telemetria completa por fase:

- `START_SYNC` encontrado;
- `PHYS_LENGTH` recuperado ou invalido;
- progresso de acumulacao do `ROBUST_FRAME`;
- quadro rejeitado por CRC/payload/tamanho;
- quadro valido, com confianca e latencia estimada apos o fim do frame.

Ao parar RX, o app encerra a escuta e registra duracao capturada, pico de audio e a quantidade aproximada de amostras proximas de clipping, incluindo porcentagem sobre o total. Quando ha clipping, o log classifica como picos isolados, clipping ocasional ou clipping frequente. A parada do RX nao dispara mais uma decodificacao offline de toda a captura.

Ao usar `Decodificar WAV` manualmente, o app tambem registra duracao, sample rate, pico e clipping aproximado do arquivo aberto antes de tentar recuperar o quadro.

O sample rate de TX/WAV e o sample rate de captura RX sao configuracoes separadas. A captura RX usa 48000 Hz por padrao, que e a taxa mais comum em dispositivos de audio no Windows. O receptor em fluxo decodifica usando essa taxa. Isso evita audio RX com duracao aparente comprimida ou esticada por divergencia entre a taxa real de captura e a taxa assumida pelo demodulador.

Arquivos WAV continuam suportados para debug e reproducao de capturas, mas nao sao o caminho principal da operacao em radio.

Ainda nao ha controle automatico de ganho nem rastreamento continuo fino de clock.

## Melhorias planejadas de operacao

As proximas melhorias de interface devem ajudar o operador a prever e acompanhar a transmissao:

- manter a estimativa TX ao vivo sincronizada com indicativo, mensagem, preambulo e duracao de simbolo;
- estender a sanitizacao visual futuramente para outros campos de texto transmitidos, como indicativo, se necessario;
- manter a barra de progresso TX sincronizada com pausas/interrupcoes futuras quando houver controle mais avancado de audio.

A primeira waterfall RX foi adicionada ao app. Ela e apenas visual, usa blocos capturados de audio para mostrar energia aproximada entre 300 Hz e 3 kHz, possui escala horizontal de frequencia em passos de 300 Hz, nao altera a decodificacao e roda no thread da UI para nao bloquear a reciclagem dos buffers de captura.
