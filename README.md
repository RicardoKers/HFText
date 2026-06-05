# HFText

HFText é um projeto experimental de comunicação digital de texto via áudio, destinado à transmissão por rádio HF usando um celular, computador ou outro dispositivo de áudio conectado ao rádio.

O objetivo é permitir que uma mensagem curta digitada pelo usuário seja convertida em áudio modulado, transmitida pelo rádio e posteriormente demodulada de volta para texto.

O projeto deve priorizar robustez em sinais fracos e ruidosos, mesmo que a taxa útil de transmissão seja muito baixa.

## Objetivos principais

- Criar um modem digital simples e robusto baseado em áudio.
- Permitir transmissão de texto curto via rádio HF.
- Começar com uma versão para PC para simulação, teste e validação.
- Reaproveitar o núcleo DSP em uma futura versão Android.
- Manter o núcleo do modem independente da interface gráfica.
- Usar FEC e interleaving no modo robusto atual, mantendo espaço para ACK e modos futuros.

## Estado atual

O sistema atual usa sempre o modo robusto HFText v0.1:

- frame logico `SYNC | LENGTH | PAYLOAD | CRC16`;
- FEC `conv_k3` rate 1/2 com interleaving deterministico;
- fluxo fisico `PREAMBLE | START_SYNC | PHYS_LENGTH | ROBUST_FRAME`;
- modulacao 2-FSK;
- recepcao continua no app PC por `StreamingReceiver`;
- busca de `START_SYNC`, recuperacao de `PHYS_LENGTH` e Viterbi soft-decision no RX C++ usando confianca por simbolo quando disponivel.

Este modo v0.1 e o baseline operacional para validacao de campo. Mudancas incompativeis de protocolo ou modulacao, como repeticao operacional, 4-FSK, 8-FSK, ACK ou novos campos de quadro, devem ser tratadas como v0.2 ou posterior.

O app PC em Qt ja permite gerar/transmitir WAV, receber audio continuamente pela placa de som, visualizar nivel/qualidade/waterfall, acompanhar estado/sessao RX e registrar logs/evidencias de campo.

## Estratégia de desenvolvimento

O projeto será desenvolvido em fases:

1. Simulação em Python.
2. Núcleo do modem em C++.
3. Aplicação PC.
4. Aplicação Android.
5. Melhorias de robustez e protocolo.

## Tecnologias previstas

- Python para simulação e validação inicial.
- C++17 ou C++20 para o núcleo DSP definitivo.
- Qt + C++ para aplicação PC.
- Kotlin + Jetpack Compose para aplicação Android.
- JNI para integração Android com o núcleo C++.
- CMake para build do núcleo e aplicação PC.
- Gradle/Kotlin para build Android.

## Princípio fundamental

O núcleo do modem não deve depender de interface gráfica, Android, Qt ou sistema operacional.

A interface gráfica deve apenas fornecer entrada de texto, configurações, áudio e exibição dos resultados.
