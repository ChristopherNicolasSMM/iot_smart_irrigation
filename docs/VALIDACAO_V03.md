# Entrega v0.3.0 — integrações e revisões

Data: 13/10/2026. A execução da v0.2.0 foi confirmada pelo usuário e suas validações locais começaram. Firmware continua fora desta fase.

## Entregue

- Perfis de envio por GUID: URL, POST/PUT, cabeçalhos, modelo JSON, timeout, tentativas, capacidade de fila e regra de confirmação.
- Envio HTTP(S) real em segundo plano. HTTPS público por padrão; destinos locais mediante ALLOW_LOCAL_INTEGRATIONS=1.
- Variáveis preservam números, listas, null e strings escapadas. Compatibilidade com message.id da v0.2; preferir message.message_id.
- Fila persistente no SQLite: pending, sending, delivered, dead. IDs preservados nas repetições; espera crescente; sem envio enquanto dispositivo está offline.
- Automação de fila para amostras e eventos quando habilitada. Teste real de envio separado da prévia.
- Descarte explícito quando fila cheia, sem prometer retenção infinita.
- Receptor POST /iot/v1/telemetry com Bearer individual por dispositivo, revogação por rotação, validação, deduplicação e ACK após transação.
- Leituras recebidas fora de ordem preservam histórico sem regredir last_received.
- Configuração desejada e aplicada separadas. Publicação pendente até dispositivo conectado e ocioso; avanço do relógio processa aplicação.
- Histórico persistente de revisões: pending, applied, rejected, superseded.
- Operador não publica perfil, não envia teste de integração e não gera credencial.

## Verificação

Node 24.19.0; npm run check e npm test. **59 testes aprovados**: os 36 anteriores e 23 adicionais. Envio real HTTP entre cliente e receptor local validado; falha de banco não retorna ACK aceito. Nenhuma requisição a servidor externo do usuário foi realizada nesta validação.

Não houve nova validação visual automatizada em navegador nesta entrega. A v0.2 foi executada pelo usuário; as novas telas precisam de sua validação local. HTTPS com certificado público não foi exercitado em servidor externo; usa a verificação padrão do Node, sem desligar TLS.

## Migração

Novas tabelas criadas automaticamente: outbox, integration_headers, device_credentials, inbox, incoming_events, configuration_versions. Configurações, GUIDs, histórico e contas existentes são preservados. Envio automático permanece desabilitado na migração da v0.2. Perfis antigos são normalizados e copiados para cada dispositivo; credenciais só são criadas por ação explícita.

## Limites restantes

- Um laboratório/propriedade compartilhado; isolamento multiempresa/áreas estruturadas (AT-30) continua pendente.
- Protocolo completo de consulta de comandos/configuração pelo ESP32 e ACK físico não implementado. Revisões assíncronas são simuladas localmente, embora auditadas/persistidas.
- Política detalhada de DST, relógio inválido e agendas perdidas (AT-34) permanece pendente.
- Headers e cópias de jobs contêm segredos em texto no banco local porque são necessários ao envio. Não retornam na API, prévia, fila ou logs. O token do receptor fica somente como hash. Proteção/encriptação de segredos em produção fica pendente.
- Desabilitar envio automático impede novos jobs; os já enfileirados mantêm URL, modelo e headers originais. Teste real pode ser solicitado com automático desabilitado.
- Perfil alterado não redireciona silenciosamente mensagens antigas. Token rotacionado exige atualizar Authorization; jobs antigos podem falhar com 401.
- Com ACK obrigatório, exigir campo booleano true e message_id idêntico; sem ACK, entregue significa somente status HTTP aceito, sem garantia de persistência.
- Históricos, inbox e jobs concluídos ainda não têm limpeza por retenção; lista exibe os últimos 100 jobs, capacidade limita apenas pendentes/em envio por dispositivo.
- Modelo customizado deve preservar message_id no nível principal. Receptor interno exige envelope padrão, mostrado pelo botão Modelo do receptor local.
- Receptor interno e simulador usam o mesmo banco para demonstração. IDs evitam duplicar localmente eventos/amostras exportados pelo próprio laboratório. Não existe loop de reenvio na ingestão.

## Gate

AT-26, AT-27 e AT-28 agora têm implementação e testes para recepção/fila de dados HTTP. AT-32 tem publicação/aplicação assíncrona no simulador. Isso não fecha o gate integral da fase 1 nem valida comunicação com ESP32 real.
