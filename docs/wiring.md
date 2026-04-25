# Wiring

Este documento descreve as conexões elétricas esperadas pelo firmware em `src/main.c`.

> Atenção: os GPIOs do ESP32 trabalham em 3,3 V e não são tolerantes a 5 V. Verifique os níveis lógicos dos módulos antes de conectar ao microcontrolador.

## Tabela de Conexões

| Componente | Pino do componente | Pino do ESP32 | Observações |
| --- | --- | --- | --- |
| L298N | IN1 | GPIO16 | Direção do motor esquerdo |
| L298N | IN2 | GPIO17 | Direção do motor esquerdo |
| L298N | IN3 | GPIO18 | Direção do motor direito |
| L298N | IN4 | GPIO19 | Direção do motor direito |
| L298N | ENA | GPIO22 | PWM via LEDC CH0; remover jumper ENA |
| L298N | ENB | GPIO23 | PWM via LEDC CH1; remover jumper ENB |
| L298N | OUT1/OUT2 | Motor esquerdo | Sentido pode inverter conforme ligação física |
| L298N | OUT3/OUT4 | Motor direito | Sentido pode inverter conforme ligação física |
| TCRT5000 frontal | AO / saída analógica | GPIO34 / ADC1_CH6 | GPIO34 é somente entrada |
| TCRT5000 traseiro | AO / saída analógica | GPIO35 / ADC1_CH7 | GPIO35 é somente entrada |
| HC-SR04 | TRIG | GPIO21 | Saída digital do ESP32 |
| HC-SR04 | ECHO | GPIO32 | Entrada digital; adaptar nível se o módulo operar em 5 V |
| Sensores | VCC | 3V3 ou 5V conforme módulo | Ajustar conforme o módulo usado |
| Sensores | GND | GND comum | Compartilhar terra com ESP32 e L298N |
| L298N | +12V / VMOT | Fonte dos motores | Ajustar conforme motores e bateria do protótipo |
| L298N | GND | GND comum | Obrigatório para referência dos sinais |
| ESP32 | 5V/VIN ou USB | Alimentação da placa | Ajustar conforme montagem física do robô |

## GND Comum

O GND do ESP32, da ponte H, dos sensores e da alimentação dos motores deve estar em comum. Sem essa referência compartilhada, os sinais de controle podem ficar instáveis ou simplesmente não serem reconhecidos pela L298N e pelos sensores.

## PWM nos ENA/ENB da L298N

O firmware usa PWM nos pinos:

- `GPIO22` para `ENA`;
- `GPIO23` para `ENB`.

Em muitos módulos L298N, ENA e ENB vêm com jumpers instalados para manter os canais sempre habilitados. Para que o PWM do ESP32 controle a potência dos motores, remova esses jumpers e conecte os pinos ENA/ENB ao ESP32.

## TCRT5000 no ADC

O código lê:

- sensor frontal em `GPIO34`, que corresponde a `ADC1_CH6`;
- sensor traseiro em `GPIO35`, que corresponde a `ADC1_CH7`.

GPIO34 e GPIO35 são pinos somente de entrada no ESP32, adequados para leitura analógica. A resposta do TCRT5000 depende da altura do sensor, da iluminação e do contraste da arena. Faça calibração antes de testes de combate.

## HC-SR04 e Nível Lógico

O TRIG é acionado pelo ESP32 em 3,3 V. O ECHO do HC-SR04 comum, quando alimentado em 5 V, costuma retornar nível alto próximo de 5 V. Como isso é incompatível com o ESP32, use:

- divisor resistivo;
- conversor de nível lógico;
- ou módulo compatível com 3,3 V, como algumas versões HC-SR04P.

Não conecte ECHO em 5 V diretamente ao GPIO32.

## Motores e Sentido de Rotação

O firmware assume:

- `IN1=1` e `IN2=0` como avanço do motor esquerdo;
- `IN3=1` e `IN4=0` como avanço do motor direito.

Se o robô andar para trás ao chamar avanço, inverter para o lado errado ou girar em sentido inesperado, troque os fios do motor correspondente ou ajuste a lógica com testes controlados.

## Alimentação

Motores DC podem causar queda de tensão e ruído elétrico. Recomendações práticas:

- não alimentar motores pelo pino `3V3` do ESP32;
- usar fonte/bateria adequada para os motores;
- manter GND comum;
- usar fios curtos e bem fixados;
- considerar capacitores próximos à alimentação dos motores, conforme a montagem.
