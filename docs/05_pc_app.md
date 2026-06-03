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
MVP PC

O primeiro MVP pode ignorar recepção em tempo real.

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
- configuracao de sample rate, duracao de simbolo, tom 0, tom 1, amplitude e preambulo;
- selecao de dispositivo de saida de audio;
- selecao de dispositivo de entrada de audio;
- indicador simples de nivel RX;
- area de texto recebido;
- log simples.

O app usa `ModemController` apenas como ponte entre a interface, `hftext_core` e o utilitario de leitura/escrita WAV. Ele nao implementa logica DSP.

O `pc-app/` e incluido pelo CMake raiz, mas e ignorado automaticamente quando `Qt6 Widgets` nao esta instalado no ambiente.

## Estado atual da Fase 5

A reproducao de audio TX foi iniciada no `pc-app/` com `AudioOutput`.

Nesta etapa, o botao `Transmitir WAV` reproduz explicitamente o ultimo WAV gerado no dispositivo de saida selecionado, ou permite escolher um WAV se nenhum arquivo tiver sido gerado na sessao. O botao `Parar TX` interrompe a reproducao.

Ainda nao ha selecao avancada de formato de audio, demodulacao em tempo real ou controle automatico de ganho.

A captura RX basica tambem foi iniciada com `AudioInput`.

Nesta etapa, o botao `Receber` inicia a gravacao pelo dispositivo de entrada selecionado e o botao `Parar RX` salva o audio recebido em WAV. O indicador `Nivel RX` mostra o pico aproximado dos buffers recebidos.

Ao parar RX, o app registra no log a duracao capturada, o pico de audio e uma contagem aproximada de amostras proximas de clipping. Em seguida, tenta decodificar automaticamente o WAV salvo e mostra o resultado na area de texto recebido.

Ao usar `Decodificar WAV` manualmente, o app tambem registra duracao, sample rate, pico e clipping aproximado do arquivo aberto antes de tentar recuperar o quadro.

Durante a captura RX, o app tambem envia blocos de audio para `StreamingReceiver`. Quando um quadro valido e recuperado antes de parar a gravacao, o texto aparece na area recebida e o evento e registrado no log como `RX streaming`.

Ainda nao ha controle automatico de ganho nem rastreamento continuo fino de clock.
