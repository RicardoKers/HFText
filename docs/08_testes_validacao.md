# Testes e validação

## Objetivo

Definir como validar o funcionamento do modem e das aplicações.

## Testes unitários

### Encoder

- texto simples deve gerar símbolos corretos;
- letras maiúsculas devem usar símbolo shift e voltar como maiúsculas na decodificação;
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
- registrar offset inicial aceito e quantidade de offsets testados;
- aceitar o texto recebido apenas se o CRC estiver valido.

Ao abrir um WAV manualmente no app PC, a validacao tambem deve registrar duracao, sample rate, pico de audio e amostras proximas de clipping antes da decodificacao.

Durante uma captura RX ativa, o app tambem deve tentar decodificacao por fluxo com `StreamingReceiver`. Se um quadro valido for encontrado antes de parar RX, o log deve registrar `RX streaming` e a area de texto recebido deve mostrar a mensagem recuperada.

O indicador de clipping e aproximado e usa amostras com magnitude muito proxima do fundo de escala. Ele serve como alerta operacional para reduzir ganho ou volume quando necessario.
