# Arquitetura proposta — v0.1

## Direção

Repositório novo como fonte canônica de requisitos, portal, API, simulador e futuro firmware. Não copiar integralmente os dois projetos antigos sem revisão. Reaproveitamento será explícito e documentado.

Estrutura futura proposta (ainda não implementada):
- apps/web: portal central.
- apps/api: autenticação, ingestão, comandos e administração.
- apps/device-simulator: simulador de múltiplos ESP32.
- apps/device-portal: portal local leve, reutilizável no firmware.
- packages/contracts: schemas e exemplos versionados.
- firmware: projeto C++ após avaliação completa.
- database/migrations: esquema, índices e isolamento de acesso.
- docs e examples: documentação canônica.

## Stack proposta

Nuxt/Vue + Fastify/TypeScript para aproveitar conhecimento do ConectaAgro; PostgreSQL/Supabase para persistência. Versões suportadas serão verificadas na implementação. Simulador Node.js/TypeScript com relógio e gerador pseudoaleatório injetáveis. Firmware C++/PlatformIO, framework definitivo pendente de avaliação.

Não migrar para Python apenas por preferência de linguagem; Python permanece alternativa útil para ferramentas e testes. Não introduzir Redis como requisito do piloto; só se medição justificar.

## Fronteiras de responsabilidade

- Portal administra configuração desejada; API valida e versiona.
- Simulador/futuro ESP32 aplica revisão validada e retorna versão aplicada.
- Controle de irrigação reside no dispositivo; backend não precisa permanecer disponível para encerrar ciclo.
- Backend não declara válvula ligada ao apenas criar comando.
- Parada e duração máxima são locais.
- Telemetria recebida é persistida antes do ACK de aplicação.
- Estado desejado e observado são armazenados separadamente.
- Aplicação de configuração precisa de ACK; versão publicada não equivale a versão aplicada.

## Modelo de dados proposto

| Entidade | Finalidade |
|---|---|
| properties, memberships | Propriedade e administrador/operador |
| areas | Áreas e identificação espacial |
| devices | GUID, propriedade, função, versão, last_seen e modo real/simulado |
| device_credentials | Credencial revogável por dispositivo, armazenada de modo apropriado |
| components | Sensores/atuadores com ID estável, nome, tipo e vínculo |
| zones | Setor, válvula e sensor controlador opcional |
| schedules | Data/dias, janela, fuso e revisão |
| configuration_versions | Configuração desejada/aplicada e histórico |
| readings | Série temporal, qualidade, recorded_at/received_at e boot_id |
| commands | Solicitação, expiração e estado reportado |
| irrigation_runs | Uma execução com duração, motivo e origem |
| decision_events | Tentativas, omissões, transições e falhas |
| integration_profiles | Destinos e mapeamentos; segredos separados |

Integridade por propriedade em todas as consultas e políticas do banco. Índices por device_guid, componente e horário. Retenção e agregações definidas antes da produção. Dados simulados não se misturam silenciosamente aos reais.

## Comunicação inicial

HTTPS primeiro: telemetria/eventos em lotes e consulta periódica de comandos/configuração.
Eventos imediatos quando conectado. Telemetria histórica a cada 15 minutos.
Consulta de comandos proposta: 5 segundos no piloto; heartbeat proposto: 60 segundos. Assim, “teste web” não promete latência instantânea.
Futuro MQTT utiliza mesmas mensagens, ACL por dispositivo e ACK de aplicação, sem depender somente de QoS.

## Segurança

Identidade humana e máquina separadas; verificar escopo e propriedade no servidor.
Sessões humanas com expiração e proteção compatível com cookies/CSRF; nunca confiar apenas em flags do frontend.
Ciclo de vida do dispositivo: provisionar, vincular, autenticar, rotacionar, revogar.
Não usar GUID como senha. Não expor segredos em logs, exports ou respostas.
No ESP32 futuro, hash com salt para contas locais e portal acessível conforme rede/provisionamento definidos.
Não permitir “modo DEV” que aceite token inválido em produção. Simulação é recurso explícito com permissões próprias.

## Reconexão e consistência

Mensagem idempotente por device_guid + message_id.
Comando por command_id; repetição não reinicia temporizador.
Ocorrência de agenda com ID derivado de agenda/revisão/data local, persistido para não duplicar.
Depois de queda/reinício, saída vai ao estado seguro e execução incompleta é reconciliada como interrompida.
Dados atrasados preservam recorded_at e received_at. Relógio inválido usa uptime/boot_id e qualidade do tempo, sem inventar UTC.
Não armazenar cada tick de controle na flash; usar journal de eventos e amostras agrupadas com limites.

## Referências e reaproveitamento

DeviceMQTT fornece interface genérica e conceito de configuração; a main examinada não possui firmware.
ConectaAgro fornece base de portal, mas requer correção de injeção Supabase, id/userId, registro das rotas de sensores, autenticação de DEV e fallbacks simulados. Cliente de banco e autenticação por requisição precisam revisão. Banco real e RLS não foram inspecionados.

Antes de copiar qualquer arquivo, verificar licença e preservar avisos exigidos. Licença do novo projeto é decisão do proprietário.

## Google AI Studio

Alternativa de desenvolvimento de interface, não requisito operacional.
Pode gerar um frontend separado consumindo os mesmos contratos. Não alterar backend/contrato/identidade de dispositivos por conveniência de geração.
Primeiro decidir stack e API; depois preparar prompts por tela com estados, permissões e aceite.
Código gerado exige revisão e testes; regras de irrigação devem ser determinísticas.

## Piloto v0.2.0

Implementação inicial em src/ e public/, usando Node.js nativo/SQLite e interface HTML/CSS/JS. A estrutura futura acima continua proposta. Ver VALIDACAO_V02.md para decisão e limites; não confundir API local com protocolo de máquina futuro.
