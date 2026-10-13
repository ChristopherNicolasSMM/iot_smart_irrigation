# Contrato preliminar de API — v0.1

Contrato proposto, ainda sem servidor implementado. Caminhos e payloads serão estabilizados com schemas/OpenAPI na fase do portal. Exemplos JSON não substituem validação formal.

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
- limites de componentes.
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
