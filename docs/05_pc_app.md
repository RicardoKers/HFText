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
- botao `Decodificar WAV`;
- area de texto recebido;
- log simples.

O app usa `ModemController` apenas como ponte entre a interface, `hftext_core` e o utilitario de leitura/escrita WAV. Ele nao implementa logica DSP.

O `pc-app/` e incluido pelo CMake raiz, mas e ignorado automaticamente quando `Qt6 Widgets` nao esta instalado no ambiente.
