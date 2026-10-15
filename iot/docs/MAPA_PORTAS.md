# Mapa preliminar de portas

**Aplicável apenas ao ESP32 clássico, em placa que exponha esses GPIO. Não usar diretamente em ESP32-C3/S2/S3 ou outra placa.** Numeração GPIO, não posição física do conector. Conferir esquema da placa antes de montar.

| Função | GPIO proposto | Interface | Observação |
|---|---:|---|---|
| Umidade 1 / AO | 32 | ADC1_CH4 | Alimentação/saída do HD-38 a conferir |
| Umidade 2 / AO | 33 | ADC1_CH5 | Leitura analógica |
| Umidade 3 / AO | 34 | ADC1_CH6 | Apenas entrada; sem pull-up interno |
| Umidade 4 / AO | 35 | ADC1_CH7 | Apenas entrada; sem pull-up interno |
| Temperaturas 1–4 / DQ | 18 | 1-Wire | ROM individual; pull-up inicial 4,7 kΩ para 3,3 V |
| Relé / solenoide 1 | 25 | Saída digital | Nível desligado depende do módulo |
| Relé / solenoide 2 | 26 | Saída digital | Driver obrigatório |
| Relé / solenoide 3 | 27 | Saída digital | Driver obrigatório |
| Relé / solenoide 4 | 23 | Saída digital | Driver obrigatório |
| Reserva I2C | 21 / 22 | SDA / SCL | Não exige periférico comprado |
| Serial de programação | 1 / 3 | UART0 | Preservar para diagnóstico |

Total máximo: 9 GPIO. Um conjunto sem sensores usa um GPIO para relé. Quatro válvulas sem sensores usam quatro; quatro umidades e quatro temperaturas somam cinco GPIO adicionais.

ADC1 evita conflito ADC2/Wi-Fi no ESP32 clássico. Evitar GPIO de flash 6–11 e de strapping 0/2/5/12/15 nesta proposta. Escolher pull-up/pull-down externo do driver para desligamento antes do firmware e testar boot, reset, programação e falta de alimentação parcial.

HD-38: preferir ensaio alimentado em 3,3 V se documentação da unidade permitir. **Não aplicar AO/DO de módulo alimentado em 5/12 V diretamente ao ESP32** sem medição e condicionamento. Faixa útil do ADC depende de atenuação; uma saída abaixo de 3,3 V pode ainda saturar a leitura. Definir divisor/filtro somente após medir faixa, impedância e ruído. DO não será usado como leitura percentual.

Referência oficial: https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/peripherals/gpio.html

## Limite após definição de 100 m

Este mapa é do protótipo direto com cabos curtos. Para a montagem distribuída, sensores/relés ficam no nó remoto e matriz usa RS485; sua pinagem será revista após escolha de nós/transceptores. Ver [CABOS_E_PLACA.md](CABOS_E_PLACA.md).
