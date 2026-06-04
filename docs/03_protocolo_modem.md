# Protocolo do modem

## Objetivo

Definir o formato inicial de quadro e a codificação de mensagem.

## Versão atual do protocolo

Nome: HFText v0.1

O modo de transmissao unico e robusto. Nao existe modo operacional sem FEC/interleaving.

O frame logico antes da camada de robustez continua sendo:

```text
SYNC | LENGTH | PAYLOAD | CRC16
```

Antes da modulacao, esse frame logico deve passar por:

```text
codigo convolucional rate 1/2, K=3, geradores 111 e 101
-> interleaving retangular deterministico
-> 2-FSK
```

## Alfabeto inicial

A primeira versão usa um alfabeto reduzido de 64 símbolos ativos, codificados em 6 bits:

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
61 = acute
62 = tilde
63 = ç
```


Caracteres não suportados devem ser substituídos por `?`, para deixar a substituição visível ao operador.

Os símbolos `acute` e `tilde` são modificadores de apresentação. Eles não representam caracteres isolados: indicam que a próxima vogal deve ser apresentada com acento.

Regras:

- `acute` pode modificar `a`, `e`, `i`, `o` e `u`, produzindo `á`, `é`, `í`, `ó` e `ú`;
- `tilde` pode modificar `a` e `o`, produzindo `ã` e `õ`;
- `ç` usa o símbolo 63 diretamente;
- `acute` ou `tilde` digitados como caracteres soltos não devem ser gerados pelo transmissor; se aparecerem no texto de entrada como caracteres isolados, devem ser substituídos por `?`;
- se um modificador recebido aparecer antes de um alvo inválido, o receptor deve apresentar `?` antes do alvo para tornar a anomalia visível.

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

Para letras acentuadas maiúsculas, o modificador de acento deve preceder o `shift`:

```text
á -> [acute, a]
Á -> [acute, shift, a]
ã -> [tilde, a]
Ã -> [tilde, shift, a]
ç -> [ç]
Ç -> [shift, ç]
```

## Estrutura de quadro

```text
SYNC | LENGTH | PAYLOAD | CRC16
```

O preâmbulo de transmissão é necessário para preparar a detecção e habilitar corretamente o TX do rádio, mas não faz parte do frame lógico `SYNC | LENGTH | PAYLOAD | CRC16`.

O transmissor deve antepor um preâmbulo simples ao fluxo codificado:

```text
10101010 ... 1010
```

Tamanho inicial:

```text
64 bits
```

O preâmbulo não entra no cálculo do CRC e não faz parte de `SYNC | LENGTH | PAYLOAD | CRC16`.

Como o fluxo transmitido passa por FEC/interleaving, `SYNC` não aparece diretamente antes do Viterbi. O receptor deve procurar candidatos de bloco robusto no fluxo de bits demodulado, desfazer interleaving, aplicar Viterbi e aceitar apenas o frame lógico recuperado cujo CRC e payload sejam válidos.

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
- vogais com `acute` ou `tilde` contam como 2 símbolos;
- vogais acentuadas maiúsculas contam como 3 símbolos, pois usam modificador + `shift` + letra;
- `ç` conta como 1 símbolo e `Ç` conta como 2 símbolos.

### PAYLOAD

Mensagem transmitida.

Regras:

- tamanho de 0 a 127 símbolos de 6 bits;
- usa o alfabeto reduzido;
- caracteres não suportados devem ser substituídos por `?`, para deixar a substituição visível ao operador;
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

A taxa util e menor que a taxa bruta porque o frame logico passa por codigo convolucional rate 1/2 e bits de cauda antes da modulacao.

## Camada de robustez

O HFText v0.1 usa sempre a camada robusta abaixo. Esta camada nao e opcional.

TX:

```text
frame logico v0.1
-> codigo convolucional rate 1/2, K=3, geradores 111 e 101
-> interleaving retangular derivado do tamanho codificado
-> preambulo + 2-FSK
```

RX:

```text
bits demodulados
-> busca de candidatos de bloco robusto
-> deinterleaving
-> Viterbi hard-decision
-> frame logico v0.1
-> CRC16 normal
```

Regras:

- o frame logico antes do FEC continua sendo `SYNC | LENGTH | PAYLOAD | CRC16`;
- o CRC continua protegendo o payload logico original, nao os bits codificados;
- o codigo convolucional usa tail bits zero para retornar ao estado zero;
- o receptor deve remover os bits de cauda apos o Viterbi;
- o interleaving deve ser aplicado depois do FEC e revertido antes do Viterbi;
- a geometria do interleaving e derivada do tamanho do fluxo codificado;
- o receptor deve aceitar texto apenas quando o frame logico recuperado tiver CRC e payload validos.

Regra deterministica de interleaving:

- calcular o tamanho do fluxo codificado apos FEC;
- considerar apenas geometrias cujo numero de linhas divida exatamente esse tamanho;
- limitar inicialmente as linhas a 2..16;
- escolher o numero de linhas mais proximo de 6;
- em caso de empate, escolher o menor numero de linhas;
- colunas = tamanho codificado / linhas.

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

- repetição;
- ACK;
- timestamp opcional;
- tipo de mensagem.

### Repeticao experimental futura

A repeticao nao faz parte do HFText v0.1.

Quando for implementada, deve ser tratada como modo explicito de uma versao futura do protocolo, para evitar que receptores v0.1 interpretem um fluxo repetido de forma ambigua.

Direcao inicial recomendada:

- repetir bits ou simbolos antes da modulacao;
- no RX, usar voto majoritario por grupo;
- manter CRC sobre o payload logico original, nao sobre as copias repetidas;
- registrar no quadro ou na configuracao negociada qual fator de repeticao esta ativo;
- validar primeiro em Python com varreduras de SNR e fading antes de portar para C++.
