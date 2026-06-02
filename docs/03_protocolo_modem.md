# Protocolo do modem

## Objetivo

Definir o formato inicial de quadro e a codificação de mensagem.

## Versão inicial do protocolo

Nome: HFText Basic v0.1

## Alfabeto inicial

A primeira versão usa um alfabeto reduzido de 61 símbolos ativos, codificados em 6 bits:

```text
0  = espaço
1  = a
2  = b
...
26 = z
27 = 0
28 = 1
...
36 = 9
37 = .
38 = ,
39 = ?
40 = !
41 = /
42 = -
43 = +
44 = :
45 = ;
46 = @
47 = #
48 = $
49 = %
50 = &
51 = *
52 = (
53 = )
54 = _
55 = =
56 = <
57 = >
58 = \
59 = |
60 = shift
61-63 = reservado
```


Caracteres não suportados devem ser substituídos por espaço.

## Letras maiúsculas

Letras são transmitidas como símbolos minúsculos.

Para transmitir uma letra maiúscula, o transmissor deve emitir o símbolo `shift` seguido da letra minúscula correspondente.

Exemplos:

```text
a -> [a]
A -> [shift, a]
Ab -> [shift, a, b]
AB -> [shift, a, shift, b]
```

Na recepção, `shift` converte apenas a próxima letra `a-z` para maiúscula na apresentação.

Se `shift` aparecer antes de um símbolo que não seja letra, o receptor deve ignorar o `shift` e apresentar o símbolo seguinte normalmente.

Se `shift` aparecer no fim da mensagem, o receptor deve ignorá-lo.

## Estrutura de quadro

```text
SYNC | LENGTH | PAYLOAD | CRC16
```

O preâmbulo de transmissão é necessário para preparar a detecção e habilitar corretamente o TX do rádio, mas não faz parte do quadro do protocolo v0.1. Ele será especificado e implementado em etapa posterior.

## Campos

### SYNC

Palavra fixa de sincronismo.

Valor:

```text
0x2DD4
```

Tamanho:

```text
2 bytes
```

Serialização:

```text
big-endian: 0x2D 0xD4
```

SYNC não entra no cálculo do CRC.

### LENGTH

Tamanho lógico do `PAYLOAD`.

Tamanho:

```text
1 byte
```

Regras:

- usa apenas os 7 bits inferiores;
- o bit 7 deve ser zero;
- valores válidos: 0 a 127;
- representa a quantidade de símbolos de 6 bits no `PAYLOAD`;
- letras maiúsculas contam como 2 símbolos, pois usam `shift` + letra;
- símbolos reservados não devem ser gerados pelo transmissor na versão v0.1.

### PAYLOAD

Mensagem transmitida.

Regras:

- tamanho de 0 a 127 símbolos de 6 bits;
- usa o alfabeto reduzido;
- caracteres não suportados devem ser substituídos por espaço;
- se a mensagem codificada tiver mais de 127 símbolos, rejeitar.

### Indicativo

O indicativo não é um campo separado do protocolo.

Quando configurado, o transmissor deve inserir automaticamente o indicativo no início do `PAYLOAD`, seguido por um único espaço.

Exemplo:

```text
indicativo: pu5lrk
texto digitado: teste
payload textual antes da codificação: pu5lrk teste
```

O indicativo pode ter qualquer tamanho desde que `indicativo + espaço + texto` caiba no limite total de 127 símbolos codificados.

### CRC16

CRC calculado sobre:

```text
PAYLOAD
```

Não incluir `SYNC` nem `LENGTH` no cálculo do CRC.

Usar CRC-16/CCITT-FALSE:

```text
Polinômio: 0x1021
Inicial:   0xFFFF
RefIn:     false
RefOut:    false
XorOut:    0x0000
```

O CRC é calculado sobre a representação compactada em bytes dos símbolos de 6 bits do `PAYLOAD`.

O valor CRC16 é serializado em big-endian, byte mais significativo primeiro.

## Empacotamento de símbolos

Símbolos de 6 bits são empacotados em bytes na ordem MSB-first.

Cada símbolo é emitido do bit mais significativo para o bit menos significativo. O fluxo contínuo de bits resultante é dividido em bytes de 8 bits, também MSB-first.

Se o último byte não tiver 8 bits completos, preencher os bits menos significativos restantes com zero.

Na recepção, `LENGTH` define quantos símbolos de 6 bits devem ser recuperados do `PAYLOAD`; bits de preenchimento no final devem ser ignorados.

O quadro completo também deve ser convertido para bits em ordem MSB-first antes da modulação.

## Modulação inicial

Versão 0.1: 2-FSK

```text
bit 0 -> 1200 Hz
bit 1 -> 1600 Hz
```

Parâmetros iniciais:

```text
sampleRate = 48000 Hz
symbolDuration = 0.5 s
baseFrequency = 1200 Hz
toneSpacing = 400 Hz
toneCount = 2
```

Taxa bruta:

```text
2 bits/s
```

## Versões futuras

### 4-FSK

```text
00 -> 1000 Hz
01 -> 1200 Hz
10 -> 1400 Hz
11 -> 1600 Hz
```

### 8-FSK

```text
000 -> f0
001 -> f1
...
111 -> f7
```

## Observações

O protocolo inicial deve privilegiar simplicidade, não eficiência.

Depois de validado, adicionar:

- FEC;
- interleaving;
- repetição;
- ACK;
- timestamp opcional;
- tipo de mensagem.
