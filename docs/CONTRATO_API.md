# Contrato preliminar de API — v0.1

Contrato completo ainda proposto. A v0.3 implementa POST /iot/v1/telemetry e APIs de piloto /api/*; os demais caminhos /api/v1 e /iot/v1 da tabela permanecem futuros. Caminhos e payloads serão estabilizados com schemas/OpenAPI na fase do portal. Exemplos JSON não substituem validação formal.

## APIs humanas e dispositivo

| Método e rota proposta | Uso | Autenticação |
|---|---|---|
| POST /api/v1/auth/login | Login humano | Credenciais humanas |
| GET/POST /api/v1/devices | Listar/cadastrar vínculo | Sessão humana |
| GET/PUT /api/v1/devices/:guid/configuration | Consultar/publicar revisão | Humana; escrita admin |
| POST /api/v1/devices/:guid/commands | Manual/teste/parada | Humana autorizada |
| GET /api/v1/devices/:guid/readings | Histórico por período/componente | Humana |
| GET /api/v1/devices/:guid/events | Execuções, omissões e falhas | Humana |
| POST /iot/v1/telemetry | Persistir lote de amostras/eventos | Credencial dispositivo |
| GET /iot/v1/commands | Consultar comandos válidos | Credencial dispositivo |
| POST /iot/v1/commands/:id/events | Reportar estado do comando | Credencial dispositivo |
| GET /iot/v1/configuration | Consultar revisão desejada | Credencial dispositivo |
| POST /iot/v1/configuration/ack | Aplicada/rejeitada com motivo | Credencial dispositivo |

Identidade da máquina é derivada da credencial; device_guid no corpo deve corresponder. Nunca aceitar GUID arbitrário como autorização.

## Comando

Campos: schema_version, command_id (UUID), device_guid, zone_id, actuator_id, action (start/stop), source (manual/test), duration_s para start, issued_at, expires_at, configuration_revision e requested_by.

- Exemplo de start de teste: action=start, source=test, duration_s=5.
- stop não exige duration_s e retorna sucesso idempotente se já parado.
- API valida vínculo, autorização, revisão e duração.
- Dispositivo valida expiração, configuração atual, conflitos e limites novamente.
- Teste expira rapidamente (proposta: 30 segundos) e não permanece para retomada offline.
- Intervalo de consulta influencia latência; registrar horários de solicitação/recebimento/execução.
- Um ID repetido retorna estado existente sem reiniciar duração.
- Timeout do portal significa confirmação desconhecida, não prova de que nada aconteceu.

Estados de comando: requested → received → started → completed.
Alternativas: rejected, expired, failed, interrupted. Stop possui evento de parada associado ao ciclo quando houver.
Cada evento tem event_id, command_id, device_guid, boot_id, zone_id, actuator_id, timestamp/time_quality, reason e observed_state.

## Telemetria

Ver examples/telemetry.json.
Envelope: schema_version, message_id, device_guid, boot_id, sequence, recorded_at, time_quality, configuration_revision, simulated, readings, outputs e events.
Reading: component_id, metric, value (null se inválido), unit, quality, recorded_at.
Output: component_id, commanded_state e feedback_kind. electrical_output não significa vazão.

ACK após transação/persistência: accepted=true, message_id, received_at, duplicate.
Duplicata deve receber confirmação coerente sem inserir outra leitura/evento.
Erro de validação informa campos inválidos, sem segredos.
401/403: interromper repetição agressiva e informar falha de autenticação.
429/5xx/rede: repetir com espera crescente e IDs originais.
Entrega fora de ordem mantém timestamps originais e received_at; estado atual não deve regredir para evento mais antigo.

## Configuração

Ver examples/device-config.json:
- schema_version e revision.
- limites por dispositivo: 0–4 sensores de umidade, 0–4 sensores de temperatura e 0–4 solenoides.
- setor com mode, moisture_control e limites.
- agenda calendar (weekly/date), termination (duration/end_time) e crosses_midnight.
- dinâmica não precisa agenda; híbrida usa janela.
- estática pode armazenar alvo informativo, sem condicionar acionamento.
- zero componentes permitido; referências devem ser consistentes.
- segredos não fazem parte do exemplo/exportação.
- esquema permite parâmetros de hardware, mas drivers/pinagem reais serão definidos depois.

## Mapeamento configurável

Campos permitidos do template: device.guid, message.id, message.recorded_at e sensores cadastrados.
Engine produz JSON com tipos corretos, escapando strings; não faz concatenação textual insegura.
Resposta configurável: códigos aceitos e caminho simples do ACK/message_id; sem scripts/eval ou expressões ilimitadas.
Tela mostra preview, último envio, tempo, status e confirmação; headers secretos são mascarados.
Para destinos externos, prever idempotência contratada. Sem suporte remoto, documentar possibilidade de duplicatas.
Transformações complexas ficam no servidor.

## APIs implementadas na v0.3

POST /api/config publica desejada; GET /api/configurations?guid=... consulta estados de revisão. POST /api/integration salva perfil por GUID; headers são opcionais e não retornam. POST /api/integration/preview não envia; POST /api/integration/test enfileira envio real. GET /api/outbox?guid=... lista diagnóstico sem segredos. POST /api/device/credential gera/rotaciona Bearer de máquina, mostrado uma única vez. POST /iot/v1/telemetry exige Bearer e confirma após commit. Detalhes em EXECUCAO.md e VALIDACAO_V03.md.

## Agendas semanais v0.4

calendar=weekly aceita day_times: lista de {weekday, times}, onde weekday segue ISO 1–7 e cada time tem id, start_time, termination e duration_s OU end_time/crosses_midnight. Dias ausentes estão desmarcados. Máximo 7 dias distintos/8 horários por dia/32 agendas por dispositivo. Exemplo completo em examples/weekly-per-day.json. Formato legado weekdays + horário comum permanece compatível.

GET /api/properties lista vínculos; POST /api/property cria/edita; POST /api/property/select seleciona escopo da sessão. POST /api/area cria/edita na propriedade corrente; POST /api/user cria conta com vínculo e POST /api/member altera/concede perfil. GET /api/members exige administrador do sistema. Estado, eventos, histórico, fila, revisões e ações verificam escopo; GUID fora da propriedade retorna 404. Corpo de criação de dispositivo não determina propriedade: escopo é derivado da sessão.
