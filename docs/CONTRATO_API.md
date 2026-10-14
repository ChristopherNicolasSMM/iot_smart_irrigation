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

## Protocolo e conjuntos v0.5

Conjunto continua em zones, com actuator_id, sensor_id opcional, temperature_sensor_id opcional e area_id; agendas continuam com zone_id. Eventos incluem group_id/group_name. GET /iot/v1/configuration retorna revisão/hash/configuração; POST /iot/v1/configuration/ack exige revisão/hash/status e idle=true para aplicação. GET /iot/v1/commands retorna comandos pendentes ou reconcile_only=true; estes últimos nunca autorizam novo acionamento. POST /iot/v1/commands/:id/events recebe transições e executed_at em confirmação tardia iniciado. POST /iot/v1/runtime informa active_groups e runs opcionais. Bearer identifica dispositivo. Expiração de comandos HTTP usa UTC real; relógio acelerado é exclusivo do simulador interno.

POST /api/device/transport escolhe internal/http; GET /api/commands?guid=... consulta estados. POST /api/clock ajusta relógio interno, /api/clock-valid altera qualidade e /api/power altera energia simulada. Detalhes/limites em VALIDACAO_V05.md.

## APIs implementadas na v0.6

Todas usam sessão humana e proteção de origem existente.

| Método/rota | Corpo | Permissão e resultado |
|---|---|---|
| POST /api/config/check | {guid,config} | Admin da propriedade; valida configuração e GUID autorizado; retorna {warnings,truncated,scope,note}, sem publicar |
| POST /api/config | {guid,config} | Publicação existente acrescenta analysis com a mesma estrutura; avisos não bloqueiam publicação |
| POST /api/account/password | {current_password,password} | Própria conta; senha 10–200 caracteres; revoga sessões e limpa cookie atual |
| POST /api/user/password | {username,password} | Administrador do sistema; redefinir senha de outro usuário e revogar sessões |
| POST /api/user/enabled | {username,enabled:boolean} | Administrador do sistema; estado global e revogação de sessões |
| POST /api/member/revoke | {username} | Administrador do sistema; remove vínculo da propriedade selecionada e revoga sessões |
| GET /api/members | — | Resposta existente acrescenta enabled (0/1); sem dados de autenticação |

Avisos: code (same_solenoid_overlap, simultaneous_limit, dynamic_capacity), message, groups e, quando aplicável, schedules/example_date/start_time. scope contém timezone, weekly_from, weekly_through e specific_dates. Máximo 50 avisos; truncated sinaliza limite. A análise usa o relógio do laboratório.

401: sessão ausente/expirada/revogada ou conta desativada. 403: perfil insuficiente ou usuário sem vínculo. 404: GUID/vínculo/usuário não disponível na operação. 400: senha/estado inválido ou tentativa de autobloqueio administrativo. Auditoria persistida em account_audit; consulta/exportação pela interface é evolução futura. Migração e roteiro em VALIDACAO_V06.md.

## Diagnóstico implementado na v0.7

GET /api/diagnostics?guid=UUID exige sessão humana e vínculo com a propriedade do dispositivo; administrador/operador podem consultar (GUID não autorizado: 404). GET /api/state acrescenta o mesmo objeto diagnostics por dispositivo.

| Campo | Conteúdo |
|---|---|
| device_guid/generated_at | Identidade e instante da observação (virtual no simulador, UTC real no HTTP) |
| configuration_revision/desired_revision/configuration_status | Revisão aplicada e estado da publicação |
| communication | source, online, last_seen_at, age_s, clock_basis e heartbeat_limit_s |
| controller | known, received_at, age_s, clock_valid e clock_offset_s (diferença entre relógio informado e servidor) |
| sensors[] | component_id/name/kind/enabled/value, reported_quality, quality, recorded_at/received_at, age_s, control_age_limit_s, source e current_control_known |
| groups[] | group_id/name/mode, IDs de componentes, observed_output (on/off/unknown), automation_state (running/waiting/eligible/unknown), reasons, cooldown_remaining_s, next_schedule e forecast_kind |
| next_schedule | null ou schedule_id, slot_key, occurrence_id, date/timezone, start_at/end_at/eligible_at, window_open, conditional e duration_limit_s |
| forecast | weekly_horizon_days=14, specific_future_dates, basis=applied_configuration, clock_basis e note |

POST /iot/v1/runtime permanece compatível: active_groups e runs existentes. Campo opcional controller_status: {revision,clock_valid,now,uptime,sensors:{component_id:{value,quality,updated_at?,updated_uptime?}},last_stop_uptime:{group_id:number},consumed_occurrences:[string]}. now/updated_at são segundos UTC do controlador; uptime/updated_uptime e last_stop_uptime são segundos monotônicos locais. Revisão deve corresponder à aplicada; leitura válida exige valor finito (umidade 0–100); IDs devem existir; tempos monotônicos não ultrapassam uptime. Relatórios inválidos retornam 400 sem alteração parcial.

Até 8 sensores, 4 conjuntos, 512 IDs consumidos de até 300 caracteres; corpo limitado a 128 KB. Ausência do campo opcional remove relatório diagnóstico anterior, para não tratar cliente legado como atualizado. Atualização persiste estado JSON e horário de recebimento, sem inserir amostra histórica. Relatório confiável exige idade de recebimento até 90 s, dispositivo conectado e revisão correspondente; idade do sensor limita atualidade a 60 s. Runtime recente confirma somente estado de saída, independentemente de informar condições de controle. Limites/semântica completos em VALIDACAO_V07.md.

## Histórico implementado na v0.8

| Rota GET | Resultado |
|---|---|
| /api/history/query | Objeto paginado de leituras normalizadas |
| /api/events/query | Objeto paginado de eventos com campos explícitos |
| /api/history/export | Download CSV (padrão) ou JSON: type=readings/events, format=csv/json |

Todas exigem guid e sessão humana autorizada à propriedade; operador/admin podem consultar/exportar. GUID não autorizado: 404; sessão ausente/revogada: 401. APIs antigas /api/history e /api/events continuam retornando arrays, com limite de 500 registros.

Filtros opcionais: from/to em ISO com fuso explícito, normalizados UTC; início inclusivo/fim exclusivo pelo recorded_at do registro de coleta/evento. component_id e group_id combinados. quality=valid/invalid/stale/missing/disabled afeta leituras; kind afeta eventos. Sem metadados históricos de grupo, amostra não satisfaz filtro de grupo. Evento satisfaz sensor se registrar component_id/actuator_id/sensor_id/temperature_sensor_id. IDs removidos continuam consultáveis quando presentes no histórico.

Paginação: limit=1..500 (padrão 200), before_id e snapshot_id inteiros positivos. Ordenação por ID de recebimento decrescente. Resposta: {schema_version:1,device_guid,type,filters,items,page:{records,has_more,next_before_id,snapshot_id},catalog:{components,groups,event_kinds},note}. Página limita registros de coleta, não a quantidade de leituras normalizadas. Próxima página deve usar next_before_id/snapshot_id e mesmos filtros. Novas chegadas não entram nesse snapshot.

Leituras: record_id, device_guid, recorded_at, reading_at, received_at, configuration_revision, component_id/component_name, metric/unit, value (null quando ausente), quality, group_ids/group_names, context_known, online (boolean/null), simulated. Eventos: record_id, device_guid, recorded_at/received_at, configuration_revision, event_id, kind/reason, group_id/group_name, component_id, source/actor, command_id/occurrence_id, area_id/area_name, simulated. Dados de autenticação/integração não integram colunas exportadas.

Exportação ignora limit e exporta toda consulta filtrada, até 5000 registros ou 20000 leituras. before_id não é aceito em exportação. Erro 400 se exceder, sem arquivo parcial; filtros/formato/paginação inválidos também retornam 400. CSV: Content-Type text/csv, UTF-8/BOM, separador ponto e vírgula, aspas escapadas e proteção de fórmula em textos; JSON inclui metadados da resposta e mantém texto original. Content-Disposition identifica download.

Amostras internas novas acrescentam revision, components e groups no JSON persistido. Telemetria continua aceitando envelope anterior; configuration_revision opcional deve ser inteiro positivo. Revisão aplicada conhecida recupera metadados históricos da configuração; ausente/desconhecida mantém contexto não comprovado. Não altera tabelas nem reescreve registros legados. Preserva coleta a cada 900 s. Detalhes de uso/limites em VALIDACAO_V08.md.

## Ciclo de vida implementado na v0.9

| Rota | Corpo/permissão/resultado |
|---|---|
| POST /api/device/archive | {guid}; administrador da origem, ociosidade comprovada; revoga token, cancela pendências de envio, marca arquivado |
| POST /api/device/restore | {guid}; administrador atual; restaura com saídas/agendas desabilitadas (revisão HTTP pendente) |
| POST /api/device/transfer | {guid,property_id,area_id}; administrador do sistema e das duas propriedades; exige arquivado; preserva GUID, chega arquivado ao destino |
| GET /api/device/transfer-options | Administrador do sistema; destinos administrativos permitidos e suas áreas |
| GET /api/device/audit?guid=UUID | Sessão autorizada ao histórico; últimas 100 ações da propriedade da consulta |

GET /api/state acrescenta history_devices: {guid,name,timezone,archived,historical_only}. devices contém somente dispositivos atualmente vinculados à propriedade, incluindo arquivados. Arquivados bloqueiam APIs humanas de operação/configuração/credencial com 400 e APIs de máquina com 401; consulta de histórico permanece disponível.

Histórico/query/exportação, endpoints antigos, catálogos, revisões, comandos, fila e auditoria usam property_id do registro. Na origem, GUID transferido permite consulta histórica, nunca comando/diagnóstico atual. Na transferência, credencial e cabeçalhos atuais são removidos; mensagens antigas permanecem canceladas/encerradas na origem. O destino não lê suas filas/comandos/revisões anteriores.

Arquivo HTTP exige config_status applied, runtime ocioso até 90 s, solenoides/agendas desabilitadas e ausência de comandos solicitados/recebidos/iniciados/sem confirmação. Envios sending impedem operação de ciclo de vida. Transferência exige área válida do destino (404 caso não pertença); operação inválida retorna 400, perfil insuficiente 403. Todas as mutações são transacionais, com auditoria e eventos.

Após transferência, telemetria exige configuration_revision igual ou superior à revisão inicial do novo vínculo; eventos também precisam de revisão desse vínculo. Pacotes anteriores retornam 400 e não são reatribuídos ao destino. IDs de comando usados em outro vínculo são indisponíveis; ACKs não acessam comandos da propriedade anterior.

Migração: property_id em events/samples/configuration_versions/outbox/machine_commands; índices e triggers de atribuição; device_registry/device_transfers/device_audit. Histórico existente da v0.8 é atribuído à propriedade do GUID antes de habilitar transferências. Corpos/IDs preservados; órfãos não são atribuídos por adivinhação. Sem atualização destrutiva do banco. Procedimento e limites em VALIDACAO_V09.md.

## Gestão da fila implementada na v0.10

| Rota | Corpo/permissão/resultado |
|---|---|
| GET /api/outbox/query | guid obrigatório; status/before_id opcionais; sessão vinculada; página de 100 metadados e has_more/next_before_id |
| GET /api/outbox/summary | guid; contagem por status/retained de toda a propriedade e automatic_retention=false |
| GET /api/outbox/audit | guid; últimas 100 ações da propriedade, sem conteúdo/cabeçalhos |
| POST /api/outbox/cancel | {guid,message_id}; administrador; somente pending, preserva conteúdo/ID |
| POST /api/outbox/reprocess | {guid,message_id,use_current_headers?:boolean}; administrador; dead/cancelled do vínculo atual com conteúdo; mesmo ID/payload/perfil, novo budget de ciclo |
| POST /api/outbox/retention/preview | {guid,days:1..3650}; administrador; prévia vinculada a GUID/ator/propriedade por 5 min |
| POST /api/outbox/retention/apply | {guid,preview_id}; administrador; verifica candidatos e remove conteúdo terminal em transação auditada |

Consulta aceita GUID histórico da propriedade, mas reprocessamento exige dispositivo atualmente vinculado/ativo e revisão de vínculo comprovada. 404 para GUID/mensagem não autorizado; 403 para operador em mutação; 400 para estado/opção/cursor/prévia inválidos, capacidade cheia, prévia alterada/expirada ou reenvio de vínculo anterior. Atualização de cabeçalhos exige igualdade exata entre URL capturada e atual; por padrão mantém autenticação original.

/api/outbox antigo continua retornando array de até 100 metadados, acrescentando cycle_attempts/reprocess_count/finished_at/retained. Query acrescenta row_id e can_reprocess, sem payload/profile/headers. Estados: pending/sending/delivered/dead/cancelled; retained sinaliza conteúdo removido, não novo estado. attempts é cumulativo; cycle_attempts respeita orçamento do perfil original.

Prévia retorna preview_id, expires_at, cutoff_before, candidates, has_more, by_status, estimated_bytes, batch_byte_limit (16777216), legacy_reference_count, sample_ids e protected_pending. Elegibilidade terminal usa finished_at → delivered_at → created_at quando ausente. Máximo 5000 mensagens/16 MiB por lote. Aplicação retorna retained, identifiers_preserved e pending_untouched; não elimina registros/IDs ou dados históricos de sensores/eventos. Legado sem término usa criação, explicitado na prévia. Não há job automático de retenção nesta versão.

Migração: outbox acrescenta cycle_attempts/reprocess_count/finished_at/retained/ownership_revision e preserva conteúdo/ID/tentativas; queue_audit registra ator/propriedade/ID/ação/detalhe, incluindo checksum do payload retirado. Dados encerrados só são removidos mediante aplicação autorizada, nunca durante migração. Documentação/limites em VALIDACAO_V010.md.

## Adendo v0.12 — encaminhamento HTTP

POST /iot/v1/telemetry aceita integration_trace opcional (até quatro identificadores distintos de 1–100 caracteres) e source_message_id opcional de 1–100 caracteres. Resposta acrescenta forwarding.status e message_id externo quando uma tentativa de encaminhamento é criada. accepted confirma apenas armazenamento local; duplicate não cria novo encaminhamento. IDs externos derivados por gateway/propriedade/GUID/ID recebido são estáveis; payload externo força integration_trace/source_message_id. Perfil, autenticação, ACK e limites permanecem configurados por GUID. Detalhes/status/limites em VALIDACAO_V012.md. OpenAPI/fixtures e consolidação definitiva ficam na próxima entrega, conforme solicitado.
