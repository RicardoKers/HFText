# Visão geral do projeto HFText

## Descrição

HFText é um sistema de comunicação digital de texto via áudio. A mensagem digitada pelo usuário é convertida em uma sequência de bits, codificada, modulada em áudio e enviada para a entrada de áudio de um rádio HF.

No receptor, o áudio recebido do rádio é capturado, demodulado, decodificado e convertido novamente em texto.

O projeto deve funcionar inicialmente em PC e posteriormente em Android.

## Cenário de uso

1. Usuário digita uma mensagem curta.
2. O sistema adiciona identificação, tamanho, CRC, FEC e interleaving.
3. O sistema gera uma forma de onda de áudio.
4. O áudio é enviado ao rádio por cabo.
5. Outro rádio recebe o sinal.
6. O áudio recebido é fornecido ao computador ou celular.
7. O software detecta o quadro, demodula e mostra o texto.

## Objetivo técnico

Criar um modem digital extremamente simples, robusto e de baixa taxa, adequado a sinais de HF fracos e ruidosos.

O modo operacional atual e o modo robusto unico do HFText Basic v0.1: frame logico com `SYNC | LENGTH | PAYLOAD | CRC16`, codificacao convolucional `conv_k3`, interleaving deterministico e transmissao 2-FSK.

Esse modo v0.1 e o baseline para validacao de campo. Mudancas incompativeis, como novos modos de modulacao, repeticao operacional, ACK ou campos adicionais de quadro, devem ser planejadas como v0.2 ou posterior.

## Características desejadas

- Baixa taxa de transmissão.
- Alta robustez.
- Largura de banda estreita.
- Demodulação não coerente.
- Tolerância a desvio de frequência.
- Tolerância a ruído e fading.
- Interface simples.
- Código portável entre PC e Android.

## Primeira versão histórica

A primeira versao funcional do projeto foi planejada como uma etapa incremental com:

- codificação simples de texto;
- modulação 2-FSK ou 4-FSK;
- geração de arquivo WAV;
- recepção a partir de arquivo WAV;
- CRC16;
- testes com ruído simulado.

## Estado atual

A implementacao atual ja inclui:

- modo robusto unico com FEC `conv_k3` e interleaving;
- transmissao e recepcao 2-FSK;
- recepcao continua no app PC usando `StreamingReceiver`;
- Viterbi soft-decision no RX C++ quando ha confianca por simbolo;
- app PC Qt com TX/RX por placa de som, decodificacao WAV para debug, historico de mensagens, log com timestamp, estado/sessao RX, evidencia de campo, nivel/qualidade RX e waterfall simples entre 300 Hz e 3 kHz.

## Proximas evolucoes

- v0.2 experimental para repeticao, ACK, 4-FSK, 8-FSK ou 16-FSK;
- aplicação Android;
- rastreamento fino de clock/frequencia;
- controle automatico de ganho ou orientacao operacional equivalente;
- empacotamento/deploy de release para testes de campo;
- ACK.
