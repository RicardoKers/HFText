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
