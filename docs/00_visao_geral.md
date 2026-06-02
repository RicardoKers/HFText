# Visão geral do projeto HFText

## Descrição

HFText é um sistema de comunicação digital de texto via áudio. A mensagem digitada pelo usuário é convertida em uma sequência de bits, codificada, modulada em áudio e enviada para a entrada de áudio de um rádio HF.

No receptor, o áudio recebido do rádio é capturado, demodulado, decodificado e convertido novamente em texto.

O projeto deve funcionar inicialmente em PC e posteriormente em Android.

## Cenário de uso

1. Usuário digita uma mensagem curta.
2. O sistema adiciona identificação, tamanho, CRC e eventualmente FEC.
3. O sistema gera uma forma de onda de áudio.
4. O áudio é enviado ao rádio por cabo.
5. Outro rádio recebe o sinal.
6. O áudio recebido é fornecido ao computador ou celular.
7. O software detecta o quadro, demodula e mostra o texto.

## Objetivo técnico

Criar um modem digital extremamente simples, robusto e de baixa taxa, adequado a sinais de HF fracos e ruidosos.

## Características desejadas

- Baixa taxa de transmissão.
- Alta robustez.
- Largura de banda estreita.
- Demodulação não coerente.
- Tolerância a desvio de frequência.
- Tolerância a ruído e fading.
- Interface simples.
- Código portável entre PC e Android.

## Primeira versão funcional

A primeira versão deve implementar:

- codificação simples de texto;
- modulação 2-FSK ou 4-FSK;
- geração de arquivo WAV;
- recepção a partir de arquivo WAV;
- CRC16;
- testes com ruído simulado.

## Versões futuras

- 8-FSK ou 16-FSK;
- preâmbulo robusto;
- detecção automática de início de quadro;
- interleaving;
- FEC;
- aplicação PC com áudio em tempo real;
- aplicação Android;
- waterfall;
- ACK.
