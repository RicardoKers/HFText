# Requisitos do projeto HFText

## Requisitos funcionais

### RF001 — Digitação de mensagem

O sistema deve permitir que o usuário digite uma mensagem curta de texto.

Critérios de aceitação:
- aceitar letras minúsculas;
- aceitar letras maiúsculas por meio do símbolo shift;
- aceitar números;
- aceitar espaço;
- aceitar pontuação básica;
- limitar o tamanho máximo da mensagem a 127 símbolos codificados do alfabeto reduzido.

### RF002 — Codificação de texto em bits

O sistema deve converter a mensagem em uma sequência de bits.

Critérios de aceitação:
- codificação determinística;
- permitir decodificação reversa;
- rejeitar ou substituir caracteres não suportados;
- ter testes automatizados.

### RF003 — Geração de quadro

O sistema deve montar um quadro contendo pelo menos:

- preâmbulo;
- palavra de sincronismo;
- tamanho da mensagem;
- payload;
- CRC.

### RF004 — Modulação em áudio

O sistema deve converter bits em amostras de áudio PCM normalizadas.

Critérios de aceitação:
- saída em `float`, faixa -1.0 a +1.0;
- taxa de amostragem configurável;
- frequência dos tons configurável;
- duração de símbolo configurável.

### RF005 — Geração de WAV

A versão de simulação deve gerar arquivo WAV contendo o áudio modulado.

### RF006 — Leitura de WAV

A versão de simulação deve ler arquivo WAV e tentar recuperar o texto.

### RF007 — Demodulação

O sistema deve identificar símbolos recebidos a partir do áudio.

Critérios de aceitação:
- detectar tons em áudio limpo;
- recuperar bits em áudio limpo;
- funcionar com ruído moderado em testes simulados.

### RF008 — Validação por CRC

O sistema deve calcular e verificar CRC16 do quadro recebido.

Critérios de aceitação:
- quadro correto deve passar;
- quadro alterado deve falhar.

### RF009 — Aplicação PC

A aplicação PC deve permitir:

- digitar mensagem;
- transmitir áudio pela placa de som;
- receber áudio pela placa de som;
- mostrar texto recebido;
- selecionar dispositivo de entrada e saída;
- mostrar nível de áudio.

### RF010 — Aplicação Android

A aplicação Android deve permitir:

- digitar mensagem;
- transmitir áudio pela saída do celular;
- receber áudio pela entrada de microfone ou interface de áudio;
- mostrar texto recebido;
- configurar parâmetros básicos do modem.

## Requisitos não funcionais

### RNF001 — Portabilidade

O núcleo do modem deve ser portável entre PC e Android.

### RNF002 — Independência da interface

O núcleo DSP não deve depender de Qt, Android, Python ou APIs de áudio.

### RNF003 — Robustez

O sistema deve priorizar robustez em canal ruidoso em vez de taxa de transmissão.

### RNF004 — Baixa complexidade inicial

A primeira versão deve ser simples, mesmo que lenta.

### RNF005 — Testabilidade

Cada módulo do núcleo deve possuir testes automatizados.

### RNF006 — Código didático

O código deve ser claro, comentado e adequado para estudo.

### RNF007 — Segurança operacional

O software não deve transmitir automaticamente sem ação explícita do usuário.

### RNF008 — Sem criptografia

O projeto não deve implementar criptografia ou ocultação de conteúdo.

## Requisitos de desempenho iniciais

- Taxa bruta inicial: 1 a 10 bps.
- Mensagem típica: até 80 símbolos.
- Payload máximo: 127 símbolos codificados.
- Faixa de áudio inicial: 1000 Hz a 2000 Hz.
- Taxa de amostragem inicial: 48000 Hz.
- Amostras normalizadas em ponto flutuante.
