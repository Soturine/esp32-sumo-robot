# Materials

Lista de materiais para uma montagem compatível com o firmware documentado.

Alguns itens podem variar conforme a versão física do protótipo. Quando necessário, ajuste conforme a montagem real do robô.

## BOM

| Item | Quantidade | Observações |
| --- | ---: | --- |
| ESP32 WROOM / DevKit 38 pinos | 1 | Controlador principal |
| Sensor ultrassônico HC-SR04 ou HC-SR04P | 1 | Detecção de oponente/alvo |
| Sensor TCRT5000 | 2 | Detecção de borda/linha branca, frontal e traseiro |
| Ponte H L298N | 1 | Controle dos motores DC |
| Motor DC N20 | 2 | Motores de tração, ajustar conforme versão do protótipo |
| Rodas compatíveis com N20 | 2 | Ajustar ao eixo dos motores |
| Chassi/base | 1 | Pode ser acrílico, MDF, impressão 3D ou caixa adaptada |
| Bateria/fonte para motores | 1 | Dimensionar conforme tensão/corrente dos motores |
| Alimentação para ESP32 | 1 | USB, regulador, power bank ou fonte adequada |
| Chave liga/desliga | 1 | Recomendado para a alimentação principal |
| Fios jumper | vários | Para prototipagem e ligação dos módulos |
| Protoboard ou placa de apoio | 1 | Opcional, conforme montagem |
| Divisor de tensão/conversor de nível | 1 | Recomendado para ECHO do HC-SR04 se operar em 5 V |
| Parafusos, suportes e espaçadores | conforme necessário | Ajustar conforme chassi |

## Observações

- Não alimente os motores pelo pino `3V3` do ESP32.
- Garanta GND comum entre ESP32, L298N, sensores e fonte dos motores.
- Para PWM nos canais da L298N, remova os jumpers ENA/ENB.
- Se usar HC-SR04 em 5 V, proteja o GPIO do ESP32 no sinal ECHO.
- A altura dos sensores TCRT5000 em relação ao chão influencia diretamente os thresholds ADC.

## Itens para Documentação Visual

Para deixar o portfólio mais completo, adicione depois:

- foto superior do robô mostrando a montagem;
- foto frontal mostrando sensores;
- diagrama de ligação;
- vídeo curto ou GIF do robô buscando/atacando;
- foto da arena usada nos testes.
