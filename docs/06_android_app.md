# Aplicação Android

## Objetivo

Criar uma aplicação Android para transmissão e recepção de texto via áudio usando rádio HF.

## Plataforma

- Kotlin.
- Jetpack Compose.
- AudioTrack para TX.
- AudioRecord para RX.
- C++/NDK via JNI para núcleo DSP.

## Telas previstas

### Tela principal

Elementos:

- campo de mensagem;
- botão transmitir;
- botão iniciar recepção;
- botão parar recepção;
- texto recebido;
- nível de áudio de entrada;
- nível de áudio de saída;
- status do modem.

### Tela de configurações

Parâmetros:

- frequência do tom 0;
- espaçamento dos tons;
- duração do símbolo;
- quantidade de tons;
- volume de transmissão;
- tamanho máximo da mensagem;
- modo de modulação;
- indicativo do usuário;
- habilitar/desabilitar CRC;
- habilitar/desabilitar FEC futuramente.

## Permissões

A aplicação deve solicitar permissão de microfone.

## Áudio

### Transmissão

Usar AudioTrack para reproduzir amostras PCM.

### Recepção

Usar AudioRecord para capturar amostras PCM.

## Cuidados

- Não transmitir sem ação explícita do usuário.
- Mostrar aviso quando o áudio de entrada estiver saturando.
- Permitir ajuste de volume de TX.
- Evitar Bluetooth no MVP.
- Preferir cabo ou interface USB de áudio.
- Não usar processamento automático do microfone se puder ser desativado.

## Integração com C++

Inicialmente, parte do DSP pode ser implementada em Kotlin para testes simples.

A versão definitiva deve chamar o núcleo C++ por JNI.

Exemplo de interface desejada:

```kotlin
object HfTextNative {
    external fun modulateText(text: String, config: ModemConfig): FloatArray
    external fun demodulateSamples(samples: FloatArray, config: ModemConfig): DecodeResult
}
MVP Android

A primeira versão Android deve apenas transmitir.

Fluxo:

usuário digita texto;
app gera áudio;
app reproduz áudio pela saída do celular.

A recepção pode vir depois.