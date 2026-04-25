# Troubleshooting

Guia prático para problemas comuns ao montar, compilar, gravar e testar o robô sumô.

## Ultrassônico Não Detecta Alvo

Verifique:

- TRIG conectado ao `GPIO21`;
- ECHO conectado ao `GPIO32`;
- GND comum entre HC-SR04 e ESP32;
- alimentação correta do módulo;
- alvo dentro do limite real do código: `150 mm` ou `15 cm`;
- posição física do sensor no robô.

O firmware só ataca quando a distância medida é maior que zero e menor ou igual a `150 mm`.

## HC-SR04 Retornando Timeout

Possíveis causas:

- ECHO não está chegando ao ESP32;
- TRIG e ECHO foram invertidos;
- sensor sem alimentação;
- alvo fora do alcance ou com superfície ruim para reflexão;
- ruído de alimentação causado pelos motores.

Use o monitor serial para observar se o robô entra apenas no modo de busca. Se necessário, adicione logs temporários para imprimir a distância medida.

## ECHO em Nível Incompatível

O HC-SR04 comum pode emitir ECHO em 5 V. O ESP32 não é tolerante a 5 V nos GPIOs.

Soluções:

- usar divisor resistivo no ECHO;
- usar conversor de nível lógico;
- usar módulo compatível com 3,3 V.

Não conecte ECHO em 5 V diretamente ao ESP32.

## Sensor TCRT Lendo Errado

Verifique:

- sensor frontal no `GPIO34 / ADC1_CH6`;
- sensor traseiro no `GPIO35 / ADC1_CH7`;
- saída analógica do módulo ligada ao ESP32;
- alimentação do módulo;
- distância do sensor até o chão;
- contraste entre a arena e a borda;
- iluminação ambiente.

O código usa média de quatro leituras, mas ainda depende de calibração física.

## Sensor Inverte Preto e Branco

Alguns módulos podem responder de forma diferente. Nesta versão do firmware:

- leitura `<= 1750` significa branco/borda detectado;
- leitura `>= 2000` libera o estado de branco.

Se a sua montagem mostrar o oposto, ajuste a lógica em `update_tcrt_states()` ou recalibre os thresholds. Faça isso com o robô suspenso ou com as rodas sem contato com o chão durante os primeiros testes.

## Threshold ADC Mal Calibrado

Sintomas:

- robô para sem estar na borda;
- robô sai da arena;
- robô recua ou avança de segurança no momento errado;
- leituras oscilam perto do limite.

Solução prática:

1. imprima `front_raw` e `back_raw` no monitor serial;
2. anote valores sobre a área escura e sobre a borda clara;
3. ajuste `TCRT_WHITE_RELEASE_THRESHOLD` e `TCRT_WHITE_DETECT_THRESHOLD`;
4. mantenha uma faixa de histerese entre os dois valores.

## Robô Saindo da Arena

Possíveis causas:

- sensores muito altos ou desalinhados;
- threshold invertido;
- borda com pouco contraste;
- motores rápidos demais para a resposta do loop;
- sensor frontal/traseiro trocado na montagem.

Confirme primeiro se a leitura de cada sensor muda corretamente ao passar sobre a borda.

## Motor Invertido

Se o comando de avanço faz um lado andar para trás:

- inverta os fios do motor no canal da L298N;
- ou ajuste a lógica dos pinos `IN1..IN4`.

Prefira testar com duty baixo e o robô suspenso.

## Robô Girando para o Lado Errado

O sentido de busca depende de:

```c
#define SPIN_LEFT_FORWARD 1
```

Altere para `0` se quiser inverter o sentido do giro de busca, ou ajuste a fiação dos motores.

## Robô Não Avança ao Detectar Oponente

Verifique:

- se nenhum sensor TCRT está detectando borda, pois borda tem prioridade;
- se a distância medida está realmente abaixo de `150 mm`;
- se ENA/ENB estão conectados ao ESP32;
- se os jumpers ENA/ENB da L298N foram removidos;
- se os motores têm alimentação suficiente.

## Problema de Alimentação

Sintomas comuns:

- ESP32 reinicia quando os motores ligam;
- motores tremem ou perdem força;
- leituras dos sensores ficam instáveis;
- upload falha com o robô energizado.

Soluções:

- usar fonte adequada para os motores;
- manter GND comum;
- separar alimentação lógica e potência quando possível;
- evitar fios longos e soltos;
- adicionar capacitores conforme necessário.

## PWM Não Funciona

Verifique:

- ENA ligado ao `GPIO22`;
- ENB ligado ao `GPIO23`;
- jumpers ENA/ENB removidos;
- GND comum entre ESP32 e L298N;
- alimentação dos motores presente na L298N.

Com os jumpers instalados, a L298N pode ignorar o PWM externo e manter o canal sempre habilitado.

## PlatformIO Não Compila

Verifique:

- extensão PlatformIO instalada no VS Code;
- projeto aberto na pasta que contém `platformio.ini`;
- ambiente `esp32dev` selecionado;
- internet disponível na primeira compilação para baixar dependências;
- toolchain do ESP-IDF instalado pelo PlatformIO.

Comando esperado:

```bash
pio run
```

## ESP32 Não Entra em Modo Upload

Tente:

- usar cabo USB com dados;
- selecionar a porta correta;
- pressionar `BOOT` durante o início do upload;
- soltar `BOOT` quando a gravação começar;
- verificar drivers USB da placa.

## Diferença Entre Ligação Física e Código

Se a montagem não corresponder à pinagem do firmware, atualize os `#define` em `src/main.c` ou refaça as conexões. Documente qualquer mudança para evitar confusão durante novos testes.
