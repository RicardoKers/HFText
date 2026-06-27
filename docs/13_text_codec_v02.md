# Text Codec v0.2

This document defines the active HFText text codec.

HFText Basic v0.1 still defines the logical frame, robust layer, and physical
flow in `docs/03_protocolo_modem.md`. Text Codec v0.2 defines the meaning of
the 6-bit payload symbols inside that frame.

This codec is an incompatible replacement for the original v0.1 alphabet.
Because HFText has not been distributed publicly, the project does not implement
a legacy text-codec compatibility mode.

## Goals

- Keep common lowercase letters, digits, and uppercase letters efficient.
- Use `shift` as a second-layer prefix for punctuation, accents, and less common
  symbols.
- Improve practical Portuguese support while still covering English, Spanish,
  basic German names/words, and common technical/radio notation.
- Preserve a 64-symbol physical alphabet and the existing 127-symbol payload
  length concept.

## Table

`63` remains the `shift` prefix. A shifted symbol means `shift` followed by the
listed base value.

```text
0  = space   | shift 0  = newline

1  = a       | shift 1  = á
2  = b       | shift 2  = à
3  = c       | shift 3  = â
4  = d       | shift 4  = ã
5  = e       | shift 5  = é
6  = f       | shift 6  = ê
7  = g       | shift 7  = í
8  = h       | shift 8  = ó
9  = i       | shift 9  = ô
10 = j       | shift 10 = õ
11 = k       | shift 11 = ú
12 = l       | shift 12 = ü
13 = m       | shift 13 = ç
14 = n       | shift 14 = ñ

15 = o       | shift 15 = .
16 = p       | shift 16 = ,
17 = q       | shift 17 = ?
18 = r       | shift 18 = !
19 = s       | shift 19 = :
20 = t       | shift 20 = ;
21 = u       | shift 21 = '
22 = v       | shift 22 = "
23 = w       | shift 23 = -
24 = x       | shift 24 = _
25 = y       | shift 25 = /
26 = z       | shift 26 = \

27 = 0       | shift 27 = +
28 = 1       | shift 28 = =
29 = 2       | shift 29 = *
30 = 3       | shift 30 = %
31 = 4       | shift 31 = &
32 = 5       | shift 32 = #
33 = 6       | shift 33 = @
34 = 7       | shift 34 = $
35 = 8       | shift 35 = <
36 = 9       | shift 36 = >

37 = A       | shift 37 = (
38 = B       | shift 38 = )
39 = C       | shift 39 = [
40 = D       | shift 40 = ]
41 = E       | shift 41 = {
42 = F       | shift 42 = }
43 = G       | shift 43 = |

44 = H       | shift 44 = Á
45 = I       | shift 45 = Â
46 = J       | shift 46 = Ã
47 = K       | shift 47 = É
48 = L       | shift 48 = Ê
49 = M       | shift 49 = Í
50 = N       | shift 50 = Ó
51 = O       | shift 51 = Ô
52 = P       | shift 52 = Õ
53 = Q       | shift 53 = Ú
54 = R       | shift 54 = Ü
55 = S       | shift 55 = Ç
56 = T       | shift 56 = Ñ

57 = U       | shift 57 = `
58 = V       | shift 58 = ~
59 = W       | shift 59 = ^
60 = X       | shift 60 = °
61 = Y       | shift 61 = reserved
62 = Z       | shift 62 = reserved
63 = shift
```

## Decode Rules

- `shift` affects only the next symbol.
- `shift + reserved`, `shift + shift`, or trailing `shift` should display `?`.
- Newline is optional in user interfaces; a compact UI may render it as a line
  break or sanitize it to a space depending on the transmit context.
- Characters not listed in either layer must still be replaced with `?`.

## Expected Tradeoffs

Advantages:

- Uppercase callsigns and abbreviations become one symbol per character.
- Portuguese accents are directly representable with two symbols each.
- Spanish `ñ/Ñ`, German/Spanish `ü/Ü`, and degree `°` are available.
- Common punctuation and technical symbols are grouped predictably.

Costs:

- Common punctuation becomes two symbols instead of one when compared with the
  v0.1 alphabet.
- Accented lowercase characters become two symbols, similar to v0.1 modifiers,
  but no longer use standalone accent-modifier state.
- Field evidence, fixtures, and all codec tests created with the old alphabet
  must be regenerated.

## Implementation Checklist

1. Keep `docs/03_protocolo_modem.md` and this reference synchronized.
2. Keep Python codec tests and implementation synchronized.
3. Keep C++ codec tests and implementation synchronized.
4. Keep C ABI expectations synchronized when symbol counts or sanitization
   behavior changes.
5. Keep PC and Android UI sanitization expectations synchronized.
6. Regenerate field-test examples and replay fixtures when the codec changes.
