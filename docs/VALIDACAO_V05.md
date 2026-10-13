# Entrega v0.5.0 — conjuntos, tempo, protocolo e backlog

13/10/2026. BACKLOG.md na raiz é a fonte canônica do andamento, com manutenção orientada por AGENTS.md.

## Conjuntos de irrigação

Cada conjunto reúne uma solenoide, sensor de umidade opcional, sensor de temperatura opcional e área atendida. Nome, identificador, modo, limite de duração e agendas pertencem ao conjunto. Agenda continua usando zone_id e o conjunto continua em zones para compatibilidade. Histórico acrescenta group_id/group_name, componentes associados e área.

Até quatro conjuntos por dispositivo. Uma solenoide não pode pertencer a dois conjuntos. Sensores podem ser compartilhados; não são obrigatórios no estático/manual/teste. Dinâmico/híbrido exigem umidade válida. Temperatura é monitoramento, sem alterar a regra de irrigação.

Conjuntos existentes preservam IDs/agendas. Sensor de temperatura não é associado por adivinhação: selecionar no editor quando necessário. Ao remover sensor, desfazer sua associação antes de publicar. Área do conjunto deve pertencer à mesma propriedade.

## Tempo e segurança

- Duração máxima e pausa usam contador monotônico, independente de ajustes no calendário.
- Ajuste manual de relógio interno limitado a ±7 dias; saltos maiores exigem abordagem futura de reprovisionamento.
- Ajuste de calendário encerra automação em curso por segurança; manual/teste mantém a duração restante.
- Ocorrências puladas são registradas e não executadas tardiamente.
- Relógio inválido bloqueia novos acionamentos/automação e encerra automação existente.
- Energia simulada desligada impede operação; retorno registra agendas perdidas e não retoma ciclo interrompido.
- Varredura de perdas limitada a sete dias; períodos maiores geram registro de truncamento.
- Horários inexistentes em transição DST comum são omitidos; horário repetido usa a primeira ocorrência UTC, sem execução dupla.
- Sobreposição durante uma execução é omitida com motivo conflito. Aviso antecipado de sobreposição no editor permanece pendente em BACKLOG.md.
- Transições raras que eliminam um dia inteiro e testes mais abrangentes de fusos continuam no backlog; não afirmar suporte universal validado.

## Protocolo de máquina

GET /iot/v1/configuration, POST /iot/v1/configuration/ack, GET /iot/v1/commands, POST /iot/v1/commands/:id/events e POST /iot/v1/runtime. Todas exigem Bearer vinculado ao GUID, assim como telemetria.

ACK de configuração exige revisão, hash correspondente e controlador ocioso. Revisão aplicada não pode regredir. Comandos usam IDs estáveis, revisão aplicada e expiração de 30 segundos em UTC real. O servidor não indica acionamento antes do ACK iniciado. Duplicatas de ACK final são idempotentes. Após recebimento sem confirmação de execução, o estado pode ficar unconfirmed: não significa sucesso nem prova de que nunca executou. Comandos em reconciliação não são autorização para novo acionamento.

Cliente Node independente disponível em src/client.js/npm run client. Possui controlador próprio, banco local, fila persistente de telemetria e ACKs, e continua automação local com última configuração válida sem servidor. Usado para ensaiar contrato futuro do ESP32; não é firmware.

Dispositivo HTTP deixa de ser controlado pelo motor interno do servidor. Relógio acelerado do laboratório não acelera esse cliente; seu tempo/expiração são reais. Heartbeat ausente por 90 segundos deixa estado offline. Cache de saída não é confirmação física quando offline. Reiniciar servidor não declara parada física do cliente.

## Verificações

Node 24.19.0; npm run check e npm test: **94 testes aprovados**, incluindo 75 anteriores, 10 de conjuntos/tempo e 9 de protocolo/cliente. Cliente independente e servidor foram testados por HTTP local com configuração, acionamento e encerramento. Testes de DST comum, ajuste de relógio, energia, conflito e agenda por conjunto passaram.

Dois testes existentes de remoção de componentes foram ajustados para desfazer associação do sensor removido. A validação agora rejeita referência inválida também em conjunto estático/temperatura, conforme integridade de configuração.

Referências de elementos HTML/JS verificadas. Renderização e cliques em navegador ainda dependem da validação local do usuário. Nenhum hardware foi compilado/acionado/validado.

## Migração e limitações

Tabela machine_commands e campos monotônicos são inicializados automaticamente. GUIDs, usuários, áreas, histórico, perfis e agendas são preservados. Modo padrão continua simulador interno; cliente HTTP é habilitado explicitamente, com nova revisão pendente.

Banco do cliente separado: data/client-GUID.sqlite. Credenciais do ambiente entram nas cópias de jobs para envio; proteger banco local. Limites de retenção/encriptação permanecem no backlog.

Perfil de saída do portal atende simulador interno/eventos do portal. Encaminhamento automático dos dados HTTP recebidos a terceiros não foi incluído; P1-11 registra essa evolução para evitar loops. Cliente envia telemetria canônica ao portal.

Trocar controlador exige saídas paradas; antes de voltar ao interno, parar e encerrar cliente independente. Portal /local ainda é uma interface de laboratório; portal embarcado real e GPIOs pertencem à fase de firmware. O gate integral do piloto permanece aberto pelos itens de BACKLOG.md.
