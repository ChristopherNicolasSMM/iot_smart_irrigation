# Decisões e pendências

## Registro de decisões

| ID | Decisão | Estado |
|---|---|---|
| IOT-D01 | HD-38 do Baú para umidade; DS18B20 encapsulado para temperatura | Base solicitada pelo usuário; bancada pendente |
| IOT-D02 | 0–4 sensores de cada tipo e 0–4 solenoides; adquirir somente quantidade necessária | Confirmado |
| IOT-D03 | Cada conjunto tem uma solenoide; umidade e temperatura opcionais e compartilháveis | Confirmado |
| IOT-D04 | Sem sensores: estático/manual/teste. Dinâmico/híbrido exigem umidade válida; temperatura apenas monitora | Confirmado |
| IOT-D05 | ESP32 clássico com ADC1, uma linha 1-Wire e quatro saídas | Proposta; depende da placa exata |
| IOT-D06 | Fonte 12 V DC, conversor 5 V e módulo relé compatível com comando 3,3 V | Proposta; depende da válvula/relé |
| IOT-D07 | Válvula convencional normalmente fechada, saída desligada em boot/reset | Proposta de segurança a validar |
| IOT-D08 | Simultaneidade física inicial 1; quatro canais não obrigam quatro válvulas simultâneas | Proposta econômica, depende de hidráulica |
| IOT-D09 | Umidade relativa calibrada; não apresentar como teor volumétrico | Proposta para HD-38 |
| IOT-D10 | Uma linha 1-Wire para até quatro sondas, identificadas por ROM | Proposta econômica; distâncias pendentes |

## Pendências até firmware

| ID | Item | Resultado necessário |
|---|---|---|
| IOT-P01 | Placa ESP32 | Link/modelo, GPIO expostos, flash, entrada 5 V/USB, regulador e orçamento 3,3 V |
| IOT-P02 | HD-38 recebido | Pinout, tensão AO/DO, consumo medido, comportamento desconectado, vedação |
| IOT-P03 | Relé | Link/modelo, alimentação da bobina, entrada 3,3 V, polaridade, corrente, contatos DC indutivos e proteção |
| IOT-P04 | Solenoide | 12 V DC ou AC, corrente/pico, NC/NO, convencional ou biestável, pressão mínima, vazão e rosca |
| IOT-P05 | Instalação | Distância de cada cabo, exposição, alimentação disponível, rede Wi-Fi e proteção mecânica |
| IOT-P06 | Hidráulica | Gravidade/rede/bomba, pressão e vazão, simultaneidade segura; intertravamento se houver bomba |
| IOT-P07 | Fonte e proteção | Corrente calculada/medida, fusíveis, seção dos cabos, queda de tensão e driver seguro |
| IOT-P08 | Sensores e calibração | Ensaio seco/molhado, comparação no solo, idade válida e resposta sob irrigação |
| IOT-P09 | Recursos e relógio | RAM/flash/TLS/JSON/fila/agendas; sincronização SNTP e comportamento após falta de energia offline |
| IOT-P10 | Firmware/build | Escolher framework/versões fixas, teste de contrato e limites; somente após avaliação de hardware |
| IOT-P11 | Bancada/campo | Boot/reset, falhas de rede/sensor, duração, desenergização e logs; nenhuma evidência física disponível |
| IOT-P12 | Portal v0.13 | Confirmação no ambiente do usuário; alerta nativo do ensaio prolongado permanece no backlog |

Sem relógio confiável após desligamento, agendas não iniciam. Avaliar RTC com bateria somente se horários após retorno offline forem requisito; não incluir custo automaticamente.
