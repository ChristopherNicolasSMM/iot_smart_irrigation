# Plano de entrega, esforços e decisões — v0.1

## Etapas e gates

| Fase | Entrega | Gate para avançar |
|---|---|---|
| 0 | Requisitos, arquitetura, exemplos e aceite | Esta documentação revisável |
| 1 | Portal central/local, API, banco e simulador | Cenários de ACEITE reproduzíveis |
| 2 | Avaliação completa do firmware e hardware | Placa, válvulas, sensores, memória, pinagem e comunicação definidos |
| 3 | Firmware C++ e documentação de compilação | Build reproduzível, testes de lógica e contratos |
| 4 | Compilação/validação física pelo usuário e ajustes | Evidências de bancada e campo |

A fase 3 não é entregue junto de um protótipo visual. O usuário solicitou portal/simulação primeiro, avaliação depois e então firmware compilável.

## Backlog da fase 1

P1-01: estrutura do projeto, ambiente, banco e schemas.
P1-02: autenticação admin/operador e propriedade/áreas.
P1-03: cadastro de GUIDs e componentes com limites.
P1-04: setores e editor dos três modos.
P1-05: agendas semanais/datas e hora final/duração.
P1-06: controlador simulado com relógio virtual e journal.
P1-07: portal local conectado ao simulador.
P1-08: teste/manual/parada local e remoto com ACK.
P1-09: histórico de 15 minutos e eventos imediatos.
P1-10: integração configurável e cenários de erro.
P1-11: testes de contrato, permissões e comportamento.
P1-12: manual, seed e roteiro de demonstração.

## Estimativa revisada

Estimativa de engenharia, não compromisso de prazo. Considera um desenvolvedor experiente, stack reaproveitada conceitualmente e nenhum hardware real na fase 1.

| Bloco fase 1 | Horas |
|---|---:|
| Base, schemas, banco e autenticação | 24–40 |
| Dispositivos, áreas e componentes | 20–32 |
| Agendas, modos e regras | 32–48 |
| Simulador determinístico e falhas | 32–48 |
| Portal local e comandos web | 24–40 |
| Histórico e integração configurável | 32–48 |
| Testes integrados, documentação e demo | 24–40 |
| Total fase 1 | 188–296 |

Reserva ~20%: 226–355 h, aproximadamente 6–9 semanas a 40 h/semana. Construção por entregas menores permite validar antes disso. Esta estimativa detalha o escopo novo, que inclui simulador determinístico e dois portais; não deve ser somada mecanicamente à estimativa anterior.

Firmware/hardware: estimativa definitiva somente após fase 2. Referência preliminar de 140–240 h para avaliação, firmware de uma placa, drivers definidos, integração e ajustes de bancada, sem PCB própria. OTA/MQTT/RS485 e validação prolongada de campo alteram esforço.

## Decisões pendentes

| Decisão | Proposta inicial | Momento limite |
|---|---|---|
| Máximo de sensores de umidade | Parametrizar no simulador; não presumir limite confirmado | Antes dos schemas definitivos |
| Segundo usuário | Operador | Antes da autenticação |
| Stack do portal | Nuxt/Vue + Fastify/TypeScript + Supabase | Início da fase 1 |
| Novo frontend por AI Studio | Opcional, mesmo contrato | Antes de refazer telas |
| Híbrido | Umidade dentro de janela; um ciclo por ocorrência | Antes do motor simulado |
| Simultaneidade | 1 válvula, ajustável depois | Antes do motor simulado |
| Teste | 5 s padrão, máximo 30 s | Antes da tela de controle |
| Sensor controlador | Um por setor | Antes do editor de setores |
| Fila/retencão | Limites medidos; perda visível | Antes da produção |
| Licença | Definida pelo proprietário; preservar origens | Antes de redistribuir código |
| Modelo ESP32/flash | Pendente | Fase 2 |
| Tensão/tipo de válvula | Pendente | Fase 2 |
| Sensores e calibração | Pendente | Fase 2 |
| Bomba/vazão/pressão | Fora do primeiro piloto salvo alteração | Fase 2 |

## Avaliação obrigatória antes do firmware

Confirmar placa e variante ESP32; flash/PSRAM; alimentação e drivers; válvulas DC/AC/biestáveis; estado físico seguro; barramentos; pinagem com Wi-Fi; proteção elétrica; calibração; partições; TLS; usuários locais; recuperação da configuração; desgaste da flash; relógio; fila offline; tempo máximo; watchdog; distribuição/compilação.

Medir memória com portal, TLS, sensores e fila simultâneos. Limites são baseados em medição, não somente em tamanho dos arquivos.
Pacote de firmware terá versão, toolchain fixada, comando de compilação, binários se compilados, pinagem validada, instruções e limitações. Nunca declarar validação física baseada apenas em build.

## Uso em novas conversas

Referenciar este repositório e docs/PLANO.md + docs/REQUISITOS.md; pedir leitura do estado atual antes de implementar. Registro canônico de decisões e progresso fica nos arquivos versionados. URL no chat não garante contexto automático de futuras conversas.
