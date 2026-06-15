# Aplicação PC

## Objetivo

Criar uma aplicação para PC que permita testar o modem em ambiente real com placa de som e rádio.

## Plataforma recomendada

C++ com Qt.

## Requisitos da primeira versão

A primeira versão da aplicação PC deve conter:

- campo de texto para mensagem;
- campo de indicativo do usuário;
- botão "Gerar WAV";
- botão "Transmitir";
- botão "Receber";
- área de texto recebido;
- seleção de dispositivo de saída;
- seleção de dispositivo de entrada;
- indicador de nível de áudio RX;
- indicador de nível de áudio TX;
- log de eventos.

## Recursos desejados

- salvar WAV transmitido;
- carregar WAV recebido;
- decodificar WAV offline;
- mostrar contagem de simbolos do payload TX durante a digitacao;
- mostrar duracao estimada da transmissao antes de gerar ou transmitir;
- mostrar progresso durante a transmissao;
- substituir caracteres invalidos por `?` diretamente no campo de mensagem TX, antes da transmissao;
- mostrar espectro básico;
- mostrar waterfall;
- salvar log de recepção;
- salvar pacote manual de evidencia de campo com log e audio RX recente.

## Arquitetura da aplicação PC

```text
pc-app/
├── main.cpp
├── MainWindow.cpp
├── MainWindow.h
├── AudioInput.cpp
├── AudioInput.h
├── AudioOutput.cpp
├── AudioOutput.h
├── ModemController.cpp
├── ModemController.h
└── CMakeLists.txt
```

## Responsabilidades

`MainWindow`:

- interface grafica;
- botoes;
- campos;
- exibicao de mensagens.

`AudioInput`:

- captura de audio;
- bufferizacao;
- conversao para float.

`AudioOutput`:

- reproducao de audio;
- controle de volume.

`ModemController`:

- conecta UI, audio e nucleo C++;
- nao deve implementar DSP diretamente.

## MVP PC historico

O primeiro MVP podia ignorar recepção em tempo real.

Fluxo mínimo:

usuário configura indicativo e digita texto;
aplicação chama modulateText;
aplicação salva WAV;
usuário testa WAV externamente;
aplicação carrega WAV;
aplicação chama demodulateSamples;
aplicação mostra texto decodificado.

## Estado atual da interface

A interface atual do `pc-app/` e uma aplicacao Qt Widgets com operacao normal em formato de chat e recursos de debug por WAV:

- historico de mensagens recebidas no topo da aba `Operacao`;
- waterfall RX no centro da aba `Operacao`, com marcadores amarelos nos tons configurados;
- campo curto de mensagem na parte inferior;
- botao iconografico de envio ao lado da mensagem;
- o mesmo botao de envio passa a parar/cancelar o TX enquanto houver audio em transmissao;
- barra discreta de progresso TX, sem texto;
- limpeza do historico RX por menu de contexto no historico de mensagens;
- indicativo e parametros do modem na aba `Configuracao`;
- seletor de modulacao fisica, mantendo `2-FSK robust v0.1` como padrao e oferecendo `4-FSK experimental v0.2` para testes controlados;
- botoes `Receber` e `Parar RX` na aba `Configuracao`;
- botoes `Gerar WAV` e `Decodificar WAV` mantidos na aba `Configuracao` como ferramentas de debug;
- configuracao de sample rate TX/WAV, sample rate RX, duracao de simbolo, tom 0, tom 1, amplitude, preambulo e modulacao;
- recepcao continua iniciada automaticamente ao abrir o app quando ha dispositivo de entrada disponivel;
- reinicio automatico do RX ativo quando parametros de recepcao mudam, como sample rate RX, duracao de simbolo, tons, preambulo, entrada de audio ou log detalhado;
- opcao `Log RX detalhado` para alternar entre log operacional resumido e telemetria completa de debug;
- estimativa TX ao vivo com simbolos de payload, bits totais e duracao aproximada;
- sanitizacao visual da mensagem TX, preservando os acentos suportados do portugues e substituindo caracteres invalidos por `?` durante a digitacao;
- barra de progresso RX baseada no bloco robusto em acumulacao;
- selecao de dispositivo de saida de audio;
- selecao de dispositivo de entrada de audio;
- indicador simples de nivel RX;
- indicador simples de qualidade RX baseado na confianca media do demodulador;
- linha `Estado RX` com resumo operacional de sincronismo, ultimo `PHYS_LENGTH`, qualidade e ultimo motivo de rejeicao;
- linha `Sessao RX` com duracao e contadores consolidados da recepcao atual;
- waterfall RX simples para observacao visual do audio recebido, com marcadores amarelos nos tons RX configurados ou derivados pela modulacao escolhida;
- area de texto recebido;
- log simples;
- botao `Salvar Evidencia RX` para gravar um WAV do audio RX recente e um TXT com configuracao, texto recebido e log;
- botao `Salvar Log` para gravar o log operacional em arquivo texto;
- botao `Limpar Log` para limpar apenas o log operacional da interface;
- persistencia local de indicativo, parametros do modem, dispositivos selecionados e estado do log detalhado;
- icone proprio do HFText no executavel e na janela.

A interface agora separa operacao e configuracao em abas:

- `Operacao`: historico de mensagens recebidas, waterfall, progresso TX, estimativa curta e campo de mensagem com botao de enviar/parar;
- `Configuracao`: indicativo, sample rates, duracao de simbolo, tons, amplitude, preambulo, dispositivos de audio, metricas RX, controles RX, debug WAV, log e botoes para salvar evidencia RX, salvar log ou limpar o log.

A area `Texto recebido` funciona como historico: novas mensagens ou resultados de decodificacao sao adicionados abaixo dos anteriores.
O comando `Limpar RX`, no menu de contexto desse historico, limpa o historico visual sem apagar WAVs, log ou configuracoes.

Ao fechar, o app salva localmente indicativo, modulacao, sample rates, duracao de simbolo, tons, amplitude, preambulo, dispositivos de audio selecionados, estado do `Log RX detalhado` e geometria da janela. Esses valores sao restaurados na proxima abertura. A mensagem TX digitada nao e persistida, para evitar reabrir o app com texto antigo pronto para transmissao.

O app usa `ModemController` apenas como ponte entre a interface, `hftext_core` e o utilitario de leitura/escrita WAV. Ele nao implementa logica DSP.

O `pc-app/` e incluido pelo CMake raiz, mas e ignorado automaticamente quando `Qt6 Widgets` nao esta instalado no ambiente.

No Windows, o alvo `hftext_pc` deve ser gerado como aplicacao grafica, sem abrir uma janela de console junto com a interface Qt. Ferramentas CLI, testes e utilitarios WAV continuam sendo executaveis de console.

## Estado atual do audio e RX continuo

A reproducao de audio TX no `pc-app/` usa `AudioOutput`.

Na operacao normal, o botao de envio gera o audio diretamente a partir do texto atual e transmite pelo dispositivo de saida selecionado, sem exigir salvar WAV antes. Enquanto a transmissao esta em andamento, o mesmo botao muda para a funcao de parar/cancelar TX. O audio so e transmitido apos acao explicita do operador no botao de envio.

Ainda nao ha selecao avancada de formato de audio ou controle automatico de ganho.

A captura RX basica tambem foi iniciada com `AudioInput`.

Nesta etapa, a escuta continua e iniciada automaticamente ao abrir o app quando ha dispositivo de entrada disponivel. O botao `Receber` permanece como controle manual para religar a escuta depois de uma parada, e `Parar RX` interrompe a recepcao. Os blocos de audio capturados alimentam o `StreamingReceiver` em uma thread de segundo plano, enquanto a interface continua atualizando `Nivel RX`, qualidade e waterfall.

O RX foi projetado para permanecer ligado por horas. Para isso, a fila entre captura e worker e limitada, o historico interno recente de `AudioInput` e limitado, a evidencia manual usa uma janela circular recente e o contador total de amostras fica separado do audio armazenado. Assim a duracao registrada continua correta sem crescimento indefinido de memoria.

Quando o operador altera parametros que afetam a recepcao enquanto o RX esta ativo, o app reinicia a escuta automaticamente para recriar o `StreamingReceiver` com a nova configuracao. Isso evita a necessidade de parar e iniciar manualmente depois de mudar modulacao, tons, sample rate RX, duracao de simbolo, preambulo, dispositivo de entrada ou o estado do log detalhado.

O RX continuo usa o modo robusto unico do core. Quando o demodulador fornece confianca por simbolo, o bloco robusto e decodificado por Viterbi soft-decision; o CRC do frame logico continua sendo a validacao final. O receptor em fluxo tambem testa pequenos deslocamentos comuns de frequencia nos dois tons, incluindo um passo intermediario de `7,5 Hz`, alinhando a escuta continua com a tolerancia do decoder WAV offline para erro leve de sintonia/BFO/SDR.

Na waterfall, linhas verticais amarelas indicam os tons usados pela modulacao atual. Em 2-FSK sao `Tom 0` e `Tom 1`; em 4-FSK experimental sao quatro tons igualmente espacados derivados de `Tom 0` e `Tom 1`. Elas sao referencias visuais para sintonia: se as trilhas recebidas aparecerem deslocadas para um lado das linhas, o operador pode ajustar a sintonia do radio/SDR ou os tons configurados. As linhas nao alteram a decodificacao.

A paleta da waterfall tambem ajuda no ajuste de nivel: trilhas fracas ou normais tendem ao verde, trilhas proximas de saturacao puxam para amarelo e blocos de entrada no fundo de escala fazem as trilhas daquele instante aparecerem em vermelho. O amarelo/vermelho e aviso visual de possivel saturacao/clipping da entrada de audio; a aceitacao da mensagem continua dependendo apenas do core e do CRC.

Quando o `StreamingReceiver` encontra um quadro com CRC e payload validos, o app adiciona a mensagem ao historico de `Texto recebido` e registra o texto recebido, offset/fases testadas e confianca media no log.

A barra `Progresso RX` mostra a acumulacao aproximada do `ROBUST_FRAME` depois que o receptor encontra `START_SYNC` e recupera `PHYS_LENGTH`. No modo normal, ela acompanha um candidato forte por vez e so avanca dentro do mesmo candidato, evitando oscilacao visual causada por eventos internos de fases diferentes. Ela chega a 100% quando um quadro valido e publicado, volta a zero ao iniciar ou parar RX e tambem volta a zero quando um candidato forte completo e rejeitado.

A linha `Estado RX` resume o estado operacional mais util do lote recente de eventos sem exigir que o operador leia todo o log: parado ou escutando, sync detectado, `PHYS_LENGTH` recuperado, progresso do frame, candidato rejeitado ou mensagem aceita. A prioridade visual e: quadro valido, frame em recebimento, `PHYS_LENGTH`, sync e, por ultimo, rejeicao isolada. Assim uma rejeicao nao rouba a linha principal quando o receptor tambem esta acumulando um frame. No modo normal, rejeicoes fracas sem sync forte sao omitidas dessa linha e do resumo da sessao; elas continuam disponiveis no `Log RX detalhado`. A linha preserva o ultimo tamanho fisico recuperado, a qualidade do ultimo candidato completo e o ultimo motivo de rejeicao, como CRC, payload ou divergencia entre `LENGTH` logico e fisico.

A linha `Sessao RX` e resetada ao iniciar uma nova recepcao e acumula a duracao da escuta e contadores operacionais consolidados: quadros aceitos, candidatos fortes rejeitados, `PHYS_LENGTH` recuperados e candidatos de sync forte. Esses numeros seguem a mesma consolidacao do log operacional normal, evitando contar todas as fases internas e candidatos fracos como eventos separados. O log detalhado continua sendo a fonte para telemetria bruta por fase. Ao parar RX, o app tambem registra esse resumo no log.

O log do app inclui timestamp em cada linha. Quando uma mensagem e aceita, o texto recebido tambem e registrado no log. Por padrao, o RX continuo mostra um resumo operacional compacto com sync forte, tamanho fisico, progresso consolidado, rejeicoes agregadas e quadro valido. Marcos repetidos por fases diferentes sao omitidos no modo normal para preservar legibilidade. A opcao `Log RX detalhado` mostra a telemetria completa por fase:

- `START_SYNC` encontrado;
- `PHYS_LENGTH` recuperado ou invalido;
- progresso de acumulacao do `ROBUST_FRAME`;
- quadro rejeitado por CRC/payload/tamanho;
- quadro valido, com confianca e latencia estimada apos o fim do frame.

Durante TX direto, o log tambem informa o pico do audio gerado. Esse valor
deve acompanhar o parametro `Amplitude`; se o pico gerado mudar mas o volume
recebido parecer igual, a normalizacao provavelmente esta ocorrendo fora do
HFText, por exemplo no mixer, no radio, no SDR ou em algum AGC.

O botao `Salvar Log` grava o conteudo atual do log em um arquivo `.txt`. O arquivo inclui um cabecalho com timestamp de exportacao, indicativo, sample rates, duracao de simbolo, tons, amplitude, preambulo, dispositivos de audio e estado do log detalhado. Isso serve para anexar testes de campo sem depender de prints da interface.

O botao `Limpar Log` limpa apenas a area de log operacional exibida na interface. Ele nao apaga o historico de `Texto recebido`, configuracoes locais, WAVs gerados ou capturas.

O botao `Salvar Evidencia RX` grava manualmente, na pasta escolhida pelo operador, dois arquivos com o mesmo prefixo: um `.wav` contendo a janela circular de audio RX recente e um `.txt` contendo cabecalho de configuracao, caminho do WAV, duracao salva, resumo CSV de campo, historico de `Texto recebido` e log atual. A janela inicial e limitada aos ultimos 300 segundos para evitar crescimento indefinido durante escutas longas. O resumo CSV inclui configuracao, contadores da sessao, diagnostico recente e, quando houver, tamanho, qualidade, offset e fases/tentativas do ultimo quadro aceito. Esse recurso e voltado para depurar falhas de campo e reproduzir capturas depois, sem alterar o protocolo nem a decodificacao em tempo real.

O mesmo TXT de evidencia tambem inclui a secao `Quadros aceitos CSV`, com uma
linha para cada quadro aceito pelo RX continuo desde o inicio da recepcao atual.
Cada linha preserva o instante de aceite, tempo decorrido da sessao, modulacao,
duracao de simbolo, sample rate RX, tons, amplitude configurada, preambulo,
tamanho, qualidade, offset/fases e texto aceito. Essa tabela deve ser usada
para comparar transmissoes individuais quando a mesma sessao tiver misturado
parametros, por exemplo 0,1 s e 0,3 s antes de salvar a evidencia.

Para manter a interface responsiva durante ruido ou muitos candidatos falsos, a fila de audio entre captura e worker RX deve ser limitada. Se o worker ficar atrasado, amostras antigas pendentes podem ser descartadas em vez de bloquear a interface. O waterfall tambem deve limitar atualizacoes pendentes, e o log detalhado pode omitir eventos por lote quando houver excesso. A validacao de mensagem continua sendo feita somente pelo core com CRC e payload validos.

Ao parar RX, o app encerra a escuta e registra duracao capturada, pico de audio e a quantidade aproximada de amostras proximas de clipping, incluindo porcentagem sobre o total. Quando ha clipping, o log classifica como picos isolados, clipping ocasional ou clipping frequente. A parada do RX nao dispara mais uma decodificacao offline de toda a captura.

Ao usar `Decodificar WAV` manualmente, o app tambem registra duracao, sample rate, pico e clipping aproximado do arquivo aberto antes de tentar recuperar o quadro.

O sample rate de TX/WAV e o sample rate de captura RX sao configuracoes separadas. A captura RX usa 48000 Hz por padrao, que e a taxa mais comum em dispositivos de audio no Windows. O receptor em fluxo decodifica usando essa taxa. Isso evita audio RX com duracao aparente comprimida ou esticada por divergencia entre a taxa real de captura e a taxa assumida pelo demodulador.

Arquivos WAV continuam suportados para debug e reproducao de capturas, mas nao sao o caminho principal da operacao em radio.

Ainda nao ha controle automatico de ganho nem rastreamento continuo fino de clock.

## Melhorias planejadas de operacao

As proximas melhorias de interface devem ajudar o operador a prever e acompanhar a transmissao:

- manter a estimativa TX ao vivo sincronizada com indicativo, mensagem, preambulo e duracao de simbolo;
- estender a sanitizacao visual futuramente para outros campos de texto transmitidos, como indicativo, se necessario;
- manter a barra de progresso TX sincronizada com pausas/interrupcoes futuras quando houver controle mais avancado de audio.

A primeira waterfall RX foi adicionada ao app. Ela e apenas visual, usa blocos capturados de audio para mostrar energia aproximada entre 300 Hz e 3 kHz, possui escala horizontal de frequencia em passos de 300 Hz, mostra marcadores amarelos nos tons RX configurados, nao altera a decodificacao e roda no thread da UI para nao bloquear a reciclagem dos buffers de captura.
