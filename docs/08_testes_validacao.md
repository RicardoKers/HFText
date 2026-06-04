# Testes e validação

## Objetivo

Definir como validar o funcionamento do modem e das aplicações.

## Testes unitários

### Encoder

- texto simples deve gerar símbolos corretos;
- letras maiúsculas devem usar símbolo shift e voltar como maiúsculas na decodificação;
- vogais acentuadas em português devem usar modificadores `acute` ou `tilde` e voltar como texto acentuado;
- `ç` e `Ç` devem ser recuperados corretamente;
- caracteres inválidos devem ser tratados;
- decodificação deve recuperar texto original.

### CRC

- CRC de vetor conhecido deve bater com valor esperado;
- alteração de um bit deve invalidar o CRC.

### Frame

- montar quadro;
- desmontar quadro;
- detectar tamanho;
- validar payload;
- detectar CRC incorreto.

### Modulador

- saída não deve ultrapassar -1.0 a +1.0;
- duração do áudio deve corresponder ao número de símbolos;
- frequência dominante deve corresponder ao bit transmitido.

### Demodulador

- deve detectar tom 0;
- deve detectar tom 1;
- deve decodificar sequência conhecida;
- deve recuperar quadro limpo.

## Testes de canal

Simular:

- ruído branco;
- atenuação;
- offset DC;
- clipping;
- desvio de frequência;
- fading por blocos.

## Métricas

Registrar:

- mensagem transmitida;
- mensagem recebida;
- número de bits;
- número de erros;
- BER;
- sucesso/falha de CRC;
- SNR estimado;
- confianca media estimada pelo demodulador;
- duração da transmissão.

Na simulação Python inicial, as métricas mínimas são:

- potência média do sinal;
- número de erros de bit;
- BER;
- estado do CRC;
- validade do payload.

Os testes de canal iniciais devem cobrir recuperação em condições moderadas de:

- AWGN;
- atenuação;
- offset DC;
- clipping leve;
- pequeno desvio de frequência;
- fading leve por blocos.

Para estimar desempenho por SNR, a varredura deve executar múltiplas sementes por nível de ruído e registrar:

- taxa de sucesso de CRC;
- taxa de payload válido;
- BER média;
- pior BER;
- confianca media;
- menor confianca;
- mínimo e máximo de erros de bit.

Além da varredura por SNR, a simulação deve possuir uma varredura por cenários nomeados de canal, cobrindo efeitos isolados e combinação moderada de efeitos.

## Teste mínimo de aceitação da Fase 1

Configuração:

```text
indicativo = pu5lrk
mensagem = Teste
```

Payload esperado:

```text
pu5lrk Teste
```

Condições:

- WAV gerado pelo transmissor Python;
- WAV lido pelo receptor Python;
- sem ruído;
- CRC válido.

Resultado esperado:

```text
pu5lrk Teste
```

## Teste com ruído

Configuração:

```text
indicativo = pu5lrk
mensagem = cq cq
```

Condição:

- ruído branco moderado;
- SNR inicial de referência: 6 dB em teste determinístico;
- sem clipping;
- sem fading severo.

Resultado esperado:

- payload `pu5lrk cq cq` recuperado corretamente ou CRC deve falhar;
- nunca exibir mensagem errada como se fosse válida.

## Regra importante

Se o CRC falhar, o sistema não deve apresentar o texto como mensagem válida.

Deve mostrar:

```text
Quadro detectado, mas CRC inválido.
```

## Testes com rádio real

Etapas:

- teste com cabo direto entre saída e entrada de áudio;
- teste com dois dispositivos próximos sem RF;
- teste com rádio em carga fantasma, se aplicável;
- teste em VHF/FM para validação simples;
- teste em HF/SSB;
- teste com sinal fraco.

## Logs

Cada recepção deve salvar opcionalmente:

- timestamp;
- configuração do modem;
- nível médio;
- pico;
- resultado do CRC;
- texto recebido;
- confiança estimada.

## Validacao no app PC

No app PC, o fluxo inicial de validacao com audio real e:

- gerar um WAV TX com a configuracao desejada;
- transmitir explicitamente o WAV pelo dispositivo de saida selecionado;
- iniciar RX pelo dispositivo de entrada selecionado;
- parar RX para salvar o WAV capturado;
- registrar duracao, pico de audio e amostras proximas de clipping;
- tentar decodificar automaticamente o WAV salvo;
- registrar offset inicial aceito, quantidade de offsets testados e confianca media estimada;
- aceitar o texto recebido apenas se o CRC estiver valido.

Ao abrir um WAV manualmente no app PC, a validacao tambem deve registrar duracao, sample rate, pico de audio e amostras proximas de clipping antes da decodificacao.

Durante uma captura RX ativa, o app deve priorizar apenas a gravacao dos buffers de audio. A decodificacao automatica ocorre depois de `Parar RX`, usando o WAV salvo. A decodificacao em fluxo com `StreamingReceiver` fica para uma etapa posterior com fila/thread propria, para nao atrasar a captura.

Ao testar captura por placa de som, conferir no log do app:

- `RX iniciado` deve informar a taxa de captura RX;
- `RX duracao` deve informar o mesmo sample rate salvo no WAV;
- a duracao exibida deve bater com o tempo real de gravacao.

Se o WAV recebido parecer comprimido ou esticado, a primeira verificacao e comparar o sample rate RX configurado com o sample rate mostrado no log e no arquivo WAV salvo. Em Windows, usar 48000 Hz para RX e o ponto de partida recomendado.

## Testes futuros de interface PC

As proximas melhorias do app PC devem incluir validacoes manuais simples:

- ao digitar mensagem TX com letras maiusculas, o contador deve incluir os simbolos `shift`;
- ao digitar vogais acentuadas, o contador deve incluir os modificadores `acute` ou `tilde`;
- ao digitar `ç`, o contador deve tratar como um simbolo, e `Ç` como `shift` + `ç`;
- ao digitar caracteres invalidos, o campo TX deve mostrar `?` antes de gerar WAV;
- ao alterar duracao de simbolo ou preambulo, a duracao estimada deve atualizar;
- durante `Transmitir WAV`, a barra de progresso deve avancar ate o fim do arquivo ou parar corretamente ao clicar `Parar TX`;
- a waterfall RX deve atualizar visualmente durante captura sem encurtar o WAV salvo nem atrapalhar a decodificacao ao parar RX.

Na primeira versao, a waterfall e validada manualmente: durante `Receber`, tons proximos da faixa do modem devem aparecer como trilhas horizontais, e a duracao do WAV capturado deve continuar coerente com o tempo real de gravacao.

O indicador de clipping e aproximado e usa amostras com magnitude muito proxima do fundo de escala. Ele serve como alerta operacional para reduzir ganho ou volume quando necessario.

## Testes futuros do modo robusto experimental

Antes de portar `conv_k3 + interleaving` para C++, a simulacao Python deve validar:

- round-trip limpo com `conv_k3` sem interleaving;
- round-trip limpo com `conv_k3 + interleaving`;
- Viterbi recuperando quadros com erros esparsos antes da verificacao de CRC;
- deinterleaving restaurando exatamente o fluxo codificado antes do Viterbi;
- geometria de interleaving derivada de forma deterministica a partir do tamanho codificado;
- rejeicao de geometrias que nao encaixem exatamente no fluxo codificado;
- varredura por SNR comparando Basic, repeticao 3x, Hamming(7,4), `conv_k3` e `conv_k3 + interleaving`;
- comparacao por tamanho de payload curto, medio e longo;
- registro de taxa de CRC, payload valido, BER recuperada, confianca media, pior BER e distancia media do Viterbi.

O modo robusto experimental deve continuar aceitando texto recebido apenas quando o CRC do frame logico estiver valido. O decoder FEC nao substitui o CRC.
