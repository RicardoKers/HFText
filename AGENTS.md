# Instruções para agentes Codex

Este projeto implementa um modem digital simples e robusto para comunicação de texto via áudio, com uso previsto em rádio HF.

O desenvolvimento deve ser incremental, testável e bem documentado.

## Regras principais

1. Não implementar grandes blocos de uma vez.
2. Antes de codificar, ler os documentos em `docs/`.
3. Manter o núcleo DSP independente de interface gráfica, Android, Qt ou APIs específicas de áudio.
4. Toda funcionalidade nova do núcleo deve ter testes.
5. Não remover arquivos sem justificativa.
6. Não alterar protocolo sem atualizar `docs/03_protocolo_modem.md`.
7. Priorizar clareza, robustez e testabilidade.
8. Não implementar criptografia.
9. Não transmitir automaticamente sem ação explícita do usuário.
10. Não começar pelo Android; a ordem correta é simulação Python, núcleo C++, CLI, PC app e depois Android.

## Arquitetura desejada

O projeto deve ser dividido em:

- `python-sim/`: simulação, geração de WAV, testes matemáticos e validação inicial.
- `core/`: núcleo definitivo em C++ portável.
- `pc-app/`: aplicação PC.
- `android-app/`: aplicação Android.
- `docs/`: documentação de requisitos, arquitetura, protocolo e testes.

## Protocolo atual

Usar HFText Basic v0.1:

```text
SYNC | LENGTH | PAYLOAD | CRC16

Regras:

SYNC = 0x2DD4, 2 bytes.
LENGTH tem 1 byte, usa apenas os 7 bits inferiores.
Bit 7 de LENGTH deve ser zero.
LENGTH representa a quantidade de símbolos de 6 bits no PAYLOAD.
Valores válidos de LENGTH: 0 a 127.
PAYLOAD tem no máximo 127 símbolos de 6 bits.
O indicativo não é campo separado; quando configurado, o transmissor o insere automaticamente no início do PAYLOAD, seguido por um espaço.
O alfabeto usa letras minúsculas; letras maiúsculas são codificadas como shift + letra minúscula.
Caracteres não suportados devem ser substituídos por ?.
CRC16 é CRC-16/CCITT-FALSE calculado sobre PAYLOAD compactado em bytes.
Símbolos de 6 bits são compactados em bytes MSB-first, com zero padding no último byte.
SYNC e CRC16 são serializados em big-endian; o quadro completo vira bits MSB-first antes da modulação.
TX usa preâmbulo alternado de 64 bits antes do quadro; RX procura SYNC no fluxo de bits e descarta bits anteriores.
RX Python pode tentar múltiplos offsets iniciais de amostra dentro do símbolo para melhorar alinhamento temporal.
Não incluir SYNC nem LENGTH no CRC.

## Implementação inicial

A primeira implementação deve usar:

Python;
2-FSK;
WAV offline;
CRC16;
sem FEC;
sem interleaving;
sem recepção em tempo real.

## Estilo Python
Usar funções pequenas.
Usar NumPy para buffers de áudio.
Usar pytest para testes.
Salvar WAVs gerados em python-sim/generated/.
Não misturar scripts CLI com biblioteca.

## Estilo C++
Usar C++17 ou superior.
Usar std::vector<float> para áudio.
Amostras normalizadas entre -1.0 e +1.0.
Evitar dependências de plataforma no core/.
Separar headers e fontes.

## Fluxo recomendado

Implementar nesta ordem:

Codificação e sanitização de texto em Python.
CRC16 em Python.
Montagem e desmontagem de quadro.
Modulador 2-FSK.
Demodulador 2-FSK.
Scripts TX/RX WAV.
Testes com ruído.
Portabilidade para C++.
