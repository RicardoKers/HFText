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
- Permitir evolução futura para FEC, interleaving, ACK e modos mais robustos.

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