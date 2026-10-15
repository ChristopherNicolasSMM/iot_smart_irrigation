# Montagem econômica e fontes

## Materiais por necessidade

| Material | Quantidade | Observação |
|---|---|---|
| Placa ESP32 | 1 | Modelo exato pendente |
| Fonte regulada 12 V DC | 1 | Só confirmar após escolher válvula DC |
| Conversor buck 12 → 5 V | 1 | Saída conferida com multímetro antes de conectar placa |
| Módulo de relé | 1–4 canais usados | Alimentação conforme bobina; comando 3,3 V documentado |
| Solenoide normalmente fechada | 0–4 | Convencional DC proposta; modelo pendente |
| HD-38 e módulo de leitura | 0–4 | Zero é configuração válida |
| Sonda DS18B20 | 0–4 | Alimentação externa de três fios |
| Pull-up 1-Wire 4,7 kΩ | 1 se houver temperatura | Valor inicial para bancada; não garantir cabos longos |
| Fusíveis, proteção indutiva, bornes, caixa e prensa-cabos | Conforme montagem | Dimensionar por circuito e ambiente |

Não comprar ADC externo, RTC, RS485 ou sensores adicionais sem necessidade demonstrada. Uma fonte pode atender válvulas e eletrônica, desde que suporte transientes e não cause resets.

## Cálculo da fonte

I12 = soma das correntes das válvulas simultâneas + P5/(12 × eficiência do buck) + consumo de outros equipamentos 12 V.

P5 = 5 × (corrente de projeto da placa no barramento 5 V + corrente das bobinas/módulos 5 V + periféricos nesse barramento).

Sensores 3,3 V entram no orçamento do regulador da placa e no consumo de entrada; não contar duas vezes. Corrente de pico da fonte e capacidade de transientes devem ser verificadas separadamente da margem contínua. Proposta inicial: margem contínua 30%, ajustada por temperatura e especificação da fonte.

**Exemplo didático, não valores dos componentes escolhidos:** válvula 0,5 A; placa 5 V/0,5 A; quatro relés 5 V/0,08 A cada; buck 85%.

| Cenário hipotético | Corrente 12 V com margem 30% | Fonte candidata para avaliação |
|---|---:|---|
| 1 válvula e 1 relé | (0,5 + 2,9/10,2) × 1,3 = 1,02 A | 12 V / 2 A |
| 4 válvulas e 4 relés simultâneos | (2 + 4,1/10,2) × 1,3 = 3,12 A | 12 V / 4–5 A |

Não definir compra da fonte por estes exemplos. Usar corrente real da válvula e módulo; medir pico Wi-Fi/acionamento. Conversor 5 V deve sustentar a carga real e aquecimento, não apenas o valor publicitário. Fonte maior em amperes não força corrente maior na carga.

## Ligações

Fonte +12 V → fusível do ramal → COM do relé; NO → positivo da válvula; negativo da válvula → negativo 12 V. Contatos devem ser adequados para **DC indutivo**, não apenas corrente resistiva estampada.

Para válvula DC convencional: supressor dimensionado junto à carga; diodo de roda livre com cátodo/faixa no positivo e ânodo no negativo é opção inicial. Avaliar tempo de fechamento; TVS ou diodo/Zener pode ser necessário. Não aplicar esse circuito em válvula AC/biestável sem projeto específico. Bobina do relé também precisa de supressão, frequentemente já presente no módulo.

12 V → buck 5 V → entrada 5 V/USB prevista na placa. Nunca aplicar 12 V nos GPIO ou 3,3 V. Evitar alimentação simultânea USB/5 V externa sem verificar proteção da placa. GND comum para circuitos não isolados; módulo optoisolado exige seguir esquema específico, não presumir isolamento só por possuir optoacoplador.

Cabos das válvulas separados das leituras, retornos de potência sem passar pelo terra de sensores; desacoplamento próximo aos módulos. Emendas/placas protegidas; resistência à corrosão da sonda não prova IP68. Dimensionar seção por corrente/comprimento/queda de tensão. Tensão de rede permanece em fonte fechada apropriada, separada da eletrônica de baixa tensão.

Referência sobre proteção de cargas: https://www.ia.omron.com/support/faq/answer/36/faq02804/
