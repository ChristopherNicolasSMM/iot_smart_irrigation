# Backlog — IoT Smart Irrigation

Atualizado: 14/10/2026 · entrega v0.13.0. Fonte canônica das próximas entregas.

Status: **Entregue/testado** = implementação e testes de software; **A validar** = depende do usuário; **Pendente** = não implementado; **Parcial** = cobertura/escopo ainda incompletos. Nenhum status de software comprova validação física.

## Entrega atual

| ID | Prioridade | Item | Status | Evidência / próxima ação |
|---|---|---|---|---|
| P0-01 | P0 | Validar visualmente portal, checkboxes e horários | Conferência automatizada entregue; revisão do usuário pendente | Chromium desktop/móvel, API/código e roteiro UI-01..UI-16; v0.12.1 validada no Windows, v0.13 aguarda confirmação local |
| P0-02 | P0 | Políticas de tempo e recuperação | Entregue/testado nos cenários principais | Relógio monotônico, ajustes/falhas e cenários raros v0.13; recorte semanal da recuperação longa documentado |
| P0-03 | P0 | Protocolo de dispositivo independente | Entregue/testado em software | Cliente HTTP separado, config/hash/ACK, comandos, expiração, reconciliação; hardware posterior |
| P0-04 | P0 | Conflitos, simultaneidade e duração máxima | Entregue/testado em software | Bloqueio/log e avisos prévios v0.6; análise temporal delimitada em VALIDACAO_V06.md |
| P0-05 | P0 | Conjunto por solenoide com umidade/temperatura/área e agenda própria | Entregue/testado | zones/zone_id preservados; até 4 conjuntos, 1 solenoide por conjunto |
| P0-06 | P0 | Backlog na raiz e manutenção por entrega | Entregue | Este arquivo e AGENTS.md |

## Próximas entregas

| ID | Prioridade | Item | Dependência | Critério de conclusão |
|---|---|---|---|---|
| P1-01 | P1 | Completar gestão de contas: senha, desativação e revogação de vínculo | Entregue/testado v0.6 | Sessões revogadas; vínculos removidos persistem após reinício; auditoria sem segredos |
| P1-02 | P1 | Desativar/arquivar dispositivos e transferir propriedade | Entregue/testado v0.9 | GUID/histórico preservados e separados por propriedade; arquivo/restauração, revogação, auditoria e migração |
| P1-03 | P1 | Diagnóstico de comunicação/leitura e próxima execução por conjunto | Entregue/testado v0.7 | Última comunicação, idade/fonte/qualidade, motivos, pausa e previsão por conjunto; limites em VALIDACAO_V07.md |
| P1-04 | P1 | Histórico com filtros, todos os sensores e exportação | Entregue/testado v0.8 | Período/sensor/conjunto/qualidade/tipo, gráficos, paginação e CSV/JSON; contexto legado desconhecido preservado |
| P1-05 | P1 | Fila: cancelamento, reprocessamento e retenção | Entregue/testado v0.10 no portal | Cancelar/reprocessar com ID e total preservados; retenção manual com prévia/limites/auditoria, sem apagar pendências |
| P1-06 | P1 | Backup/restauração e proteção de segredos | Entregue/testado no piloto v0.12 | Chave externa, cifragem de estado/fila, migração/rotação e backup/recuperação do cliente; limites em OPERACAO_SEGURA.md |
| P1-07 | P1 | Ensaio prolongado, desempenho e refatoração | Entregue no ensaio delimitado v0.12; alerta nativo a reproduzir | 300 s de carga + 7 dias virtuais, métricas/filas/integridade e módulos separados; validação prolongada real posterior |
| P1-08 | P1 | Avisos prévios de sobreposição e limites hidráulicos | Parcial: software entregue v0.6 | Editor identifica sobreposição/capacidade; dimensionamento hidráulico depende de HW-01 |
| P1-09 | P1 | Calibração e tratamento de dados do sensor | Parcial: HD-38/DS18B20 como base; ensaio pendente | Unidade relativa/volumétrica correta, qualidade e calibração explícitas |
| P1-10 | P1 | Expandir testes temporais raros e recuperação longa | Entregue/testado nos cenários delimitados v0.13 | Lord Howe, Chatham, Apia, meia-noite, 90 dias e cliente persistido; semanal limitado a 7 dias com resumo explícito |
| P1-11 | P1 | Encaminhamento/integração dos dados do cliente HTTP para terceiros | Entregue/testado v0.12 | Perfil por GUID, IDs estáveis/origem, rastros de salto, deduplicação/rollback e controle independente |
| P1-12 | P1 | Consolidar schemas/OpenAPI e contrato versionado | Entregue/testado v0.13 | JSON Schema/OpenAPI de máquina, 26 fixtures, validação compartilhada/hash e exemplos executáveis; API humana no manual |

## Firmware e hardware — após fechamento do piloto

| ID | Prioridade | Item | Dependência | Critério de conclusão |
|---|---|---|---|---|
| HW-01 | P2 | Escolher ESP32, flash, sensores, válvulas, drivers e fonte | Parcial: proposta documentada em iot/docs | Lista e pinagem aprovadas; estado seguro e capacidades definidos |
| HW-02 | P2 | Medir memória, TLS, armazenamento e limites de agenda/fila | HW-01/protocolo | Limites do Node não presumidos para ESP32 |
| FW-01 | P2 | Firmware C++: drivers, controlador local e portal leve | Parcial: experimental 0.2 compilado; desenvolvimento autorizado | Build reproduzível e mesmas regras verificadas |
| FW-02 | P2 | Compilação/bancada/campo com usuário | Parcial: build aprovado, bancada/campo pendentes | Evidências físicas de boot, duração, sensores, falhas e retomada |
| EX-01 | P3 | MQTT e OTA | Piloto físico estável | Necessidade definida; confirmação/recuperação preservadas |
| EX-02 | P3 | RS485, vazão, pressão e intertravamento de bomba | Hardware/necessidade | Drivers e proteções validados |
| EX-03 | P3 | Hospedagem, stack de produção e eventual novo frontend | Piloto validado | Decisão de arquitetura e implantação revisadas |

## Decisões vigentes

- ESP32: 0–4 sensores de umidade, 0–4 temperaturas e 0–4 solenoides.
- Conjunto: uma solenoide, zero/um sensor de umidade e zero/um de temperatura; sensores opcionais podem ser compartilhados entre conjuntos. Agenda aponta para o conjunto, portanto para sua solenoide.
- Dinâmico/híbrido exigem sensor de umidade válido; temperatura é monitoramento nesta fase.
- Estático/manual/teste independem de umidade. Híbrido faz até um ciclo por janela.
- Simultaneidade: padrão 1, configurável até 4 no piloto; dimensionamento físico pendente.
- Teste: padrão 5 s, máximo 30 s e nunca acima do limite do conjunto.
- Portal piloto: Node 24+, SQLite e HTML/CSS/JS; produção ainda em avaliação.
- Histórico a cada 900 s; eventos imediatos. Fila configurável; retenção manual auditada de conteúdo no portal, sem limpeza automática/local do cliente.
- Implementação de portal/clientes é simulada; nenhuma solenoide física é acionada pelo Node.

## Duas entregas consolidadas

1. **v0.12/v0.12.1 — etapas 1 a 3:** proteção/recuperação P1-06, ensaio delimitado P1-07 e encaminhamento P1-11. Usuário confirmou v0.12.1 no Windows em 14/10/2026. Alerta nativo do ensaio longo v0.12 ainda a reproduzir/avaliar na hospedagem.
2. **v0.13 — etapas 4 a 6:** contrato P1-12 e tempo/recuperação P1-10 implementados/testados; portal conferido em Chromium desktop/móvel e API/código, com roteiro manual. Não equivale à confirmação desta versão no Windows ou bancada.
3. **Próxima sequência:** revisão conjunta, HW-01/HW-02 e preparação do ESP32 conforme docs/PREPARACAO_ESP32.md. Firmar lista/pinagem/limites antes de FW-01; compilar e validar fisicamente em FW-02.

## Evidência da v0.6

112 testes aprovados e verificação de sintaxe; migração incremental sem apagar dados. Roteiro, APIs e limites em docs/VALIDACAO_V06.md. Análise semanal limitada, até 50 avisos; híbrido/dinâmico são condicionais. Sem validação física nem nova validação visual automatizada.

## Evidência da v0.7

131 testes aprovados e verificação de sintaxe; sem nova alteração de tabelas. Roteiro/API/compatibilidade em docs/VALIDACAO_V07.md. Previsão usa revisão aplicada, horizonte semanal de 14 dias e todas as datas específicas futuras; não garante acionamento. HTTP sem relatório atual não presume estado local nem sensor atual. Histórico continua a cada 900 s. Sem validação física ou nova validação visual automatizada.

## Evidência da v0.8

152 testes aprovados, verificação de sintaxe e conferência estática HTML/JS. Sem alteração de tabelas ou exclusão de dados. Contexto é capturado em amostras novas; legado sem prova de vínculo não é associado por adivinhação. Página padrão 200 registros, exportação até 5000 registros/20000 leituras com rejeição explícita se exceder; consulta fixa e gráfico só dos dados carregados. Roteiro e limites em docs/VALIDACAO_V08.md. Sem nova validação visual automatizada ou física.

## Evidência da v0.9

164 testes aprovados; migração incremental de propriedade nos registros, sem apagar dados ou alterar IDs/corpos existentes. HTTP exige configuração desabilitada aplicada e runtime ocioso recente antes de arquivar; arquivo não equivale a desligamento físico. Transferência chega arquivada, exige novo provisionamento e mantém registros anteriores na origem. Roteiro/limites em docs/VALIDACAO_V09.md. Sem nova validação visual automatizada/física.

## Evidência da v0.10

184 testes aprovados; migração preserva IDs, payloads e tentativas, sem aplicar retenção. Gestão restrita a administradores/propriedade; reenvio preserva ID/conteúdo/perfil e respeita vínculo. Retenção manual tem prévia de 5 min, lotes até 5000 mensagens/16 MiB, remove só conteúdo terminal e conserva tombstones/auditoria. Sem limpeza automática, compactação ou gestão da fila local do cliente nesta entrega; ver docs/VALIDACAO_V010.md. Sem nova validação visual automatizada/física.

## Evidência da v0.11

189 testes aprovados e verificação de sintaxe. Backup do portal AES-256-GCM com senha externa, snapshot SQLite/WAL até 64 MiB e recuperação em novo banco, sem sobrescrever origem. Cópia entra pausada/arquivada, revoga credenciais e não reenvia pendências; comandos já recebidos ficam inconclusivos. P1-06 parcial: SQLite ativo ainda contém segredos em claro, recuperação do cliente e gestão de chave em produção pendentes. Ver docs/VALIDACAO_V011.md; sem nova validação visual/física.

## Evidência da v0.12

215 testes aprovados e sintaxe. Estado e segredos da fila podem ser cifrados por chave externa; bancos protegidos recusam ausência/chave incorreta. Migração/rotação geram cópia nova, preservando origem. Backup do cliente mantém recovery_hold até configuração desabilitada confirmada; término offline e reconexão testados. Encaminhamento preserva dados recebidos e separa ACK local da entrega externa, sem encaminhar seus próprios rastros. Ensaio: 6000 mensagens locais, 301 duplicatas deduplicadas, 4369 envios entregues, 731 falhas e 900 descartes externos, sem perda do histórico local; 300 s reais + 7 dias virtuais/2688 amostras. Métricas/alerta nativo/limites em docs/VALIDACAO_V012.md. Sem nova validação visual ou física. Etapas 4–6 reservadas para a próxima entrega.

## Correção v0.12.1 — validação Windows

Retorno do usuário na v0.12: 209 aprovados e 6 falhas no Windows. Cinco ocorreram na limpeza de temporários antes de fechar SQLite (EBUSY); sexta na expectativa de confirmação imediata após reconexão. Corrigida ordem de cleanup, poll explícito serializa ciclo ocupado e executa ciclo novo, timer não acumula chamadas, fechamento aguarda trabalho em andamento. Teste de reconexão injeta uma falha transitória e verifica ACK em até três ciclos sem perder registros. 217 testes aprovados no ambiente Linux desta entrega e guardas de regressão; usuário confirmou a v0.12.1 no Windows em 14/10/2026. Não alterar dados ou esquemas. Etapas 4–6 foram consolidadas na v0.13; ver docs/CORRECAO_V0121.md e docs/VALIDACAO_V013.md.

## Evidência da v0.13

269 testes aprovados, sintaxe e contracts:check; 26 fixtures e schema também verificados em validador independente 2020-12 com formato habilitado. Cliente confere hash antes de aplicar configuração; legado de protocolo/agenda preservado. Data inexistente inteira e datas específicas antigas em interrupção longa tratadas sem acionamento tardio; weekly continua com recorte explícito de 7 dias. Conferência real Chromium 153, desktop 1366×900 e móvel 390×844, sem exceções JS/overflow; corrigiu logout GET→POST, resposta atrasada de sessão, resultado de comando interno e IDs padrão repetidos. Registro em docs/benchmarks/V013_UI.json e roteiro em docs/ROTEIRO_PORTAL_V013.md. Sem migração de banco, nova dependência runtime ou validação física. Confirmar v0.13 no ambiente do usuário antes da avaliação HW-01/HW-02.

## Política de atualização

Toda entrega funcional deve atualizar status, evidência, limitações, versão e próximos itens neste arquivo. Não marcar validação visual/física como concluída com base somente em testes de código. Manter IDs estáveis, registrar novos pedidos e preservar itens pendentes. Progresso dos relatórios por versão complementa este backlog, sem substituí-lo.

## Planejamento IoT — 14/10/2026

Criada pasta iot/docs com índice, decisões/pendências IOT-P01..12, pinagem preliminar condicionada ao ESP32 clássico, fontes por cálculo, materiais mínimos, diagramas e bibliotecas propostas. Base solicitada: HD-38 do Baú e DS18B20; sensores opcionais por conjunto reafirmados. Sem firmware/dependências novas ou mudança de versão do portal. Verificação: links locais e git diff --check; sem validação física/compilação. Fonte/relé/válvula/placa/cabos ainda não aprovados; exemplos de corrente não são especificações de compra.

### Adendo: cabos de 100 m e placa proposta

Usuário informou válvulas 12 V (AC/DC ainda pendente), Cat5 externo até 100 m e simultaneidade 1 aceitável/até 4 desejada quando possível. Proposto DevKitC V4/WROOM-32E; GPIO e fonte seguem condicionais. Em iot/docs/CABOS_E_PLACA.md: queda resistiva, restrição de AO/1-Wire longo e alternativas de aquisição/alimentação local ou RS485. HW-01 permanece parcial; confirmar corrente, cabo cobre/bitola, topologia e energia nos pontos remotos. Verificação documental git diff --check, sem validação física.

### Matriz com energia e dados no mesmo cabo

Confirmado: ambos sensores/válvulas até 100 m, energia exclusivamente da matriz. Proposta condicionada de ramais independentes RS485 + distribuição 24 V e conversão local para 12 V/lógica, aquisição local dos HD-38/DS18B20 e relé com timeout local. Sem implementar firmware ou protocolo de nó. EX-02 passa a avaliação de necessidade dentro de HW-01, não apenas evolução futura. Anúncio de válvula informa pressão 0,02–0,8 MPa na indexação; corrente e AC/DC permanecem pendentes. Decisão/pinout anterior de conexão direta só vale para protótipo curto, não para 100 m. Ver iot/docs/CABOS_E_PLACA.md.

## Firmware inicial autorizado — 14/10/2026

Usuário autorizou explicitamente iniciar firmware/PlatformIO com hardware ainda parcial. FW-01 iniciado, não concluído: iot/firmware com perfil matriz sem acionamento, perfil sensores de bancada opcionais, GUID NVS e primitiva monotônica testável. Drivers remotos RS485, agendas, autenticação, HTTPS/ACK/hash, persistência de logs/calibração e portal local permanecem pendentes. HW-01/HW-02/FW-02 não concluídos. Não há saídas físicas habilitadas nem ensaio de campo. Evidência em iot/docs/FIRMWARE_INICIAL.md.

## Firmware experimental 0.2 — 14/10/2026

Conferência final da continuação: três builds padrão repetidos com sucesso; quatro executáveis C++/UBSan, cinco fixtures de bytes/hash, sintaxe, 26 fixtures de contrato e 269 testes do portal novamente aprovados. Entrega local documentada, sem validação física ou publicação remota nesta conferência.

Código para acionamento condicionado a driver/provisionamento, estático/dinâmico/híbrido, configuração/hash/ACK, portal local admin/operador/CSRF, cliente HTTPS/Bearer e RS485 com lease/token de boot. Sensores opcionais, bindings por IDs estáveis, GUID NVS e registros limitados. Três perfis compilados padrão e com provisionamento sintético; quatro executáveis C++/UBSan, cinco fixtures de bytes/hash, 26 fixtures do contrato e 269 testes existentes do portal aprovados. FW-01/FW-02 parciais: sem ESP32 real→portal, navegador na placa, relé/solenoide, 100 m ou ensaio físico. Não implementar/afirmar produção a partir de build.

| ID | Próxima etapa de firmware | Estado |
|---|---|---|
| FW-03 | Completar recuperação de agendas perdidas, conflitos por agenda e retenção segura de ocorrências | Parcial: resumo na 0.2.1; retenção/conflitos detalhados pendentes |
| FW-04 | Validar HTTPS real, perfis locais, outbox/ACK e recursos TLS/NVS no ESP32 | Pendente |
| FW-05 | Validar nó RS485, driver, perdas, alimentação e topologia 100 m | Pendente, depende HW-01 |
| FW-06 | Provisionamento/segredos em produção e migração física de partições | Pendente |

Limites: UTC/BRT atual, datas até 2037, 64 horários expandidos, 128 ocorrências/conjunto, outbox 16, auditoria 64, uma UART RS485. Nó final econômico ainda não escolhido; perfil remoto usa ESP32 como referência. Firmware 0.2 é versão independente do portal 0.13; contrato remoto v1 preservado. Documentação: iot/firmware/README.md e iot/docs/VALIDACAO_FIRMWARE_V02.md.

## Firmware experimental 0.2.1 — 15/10/2026

FW-03 avançou parcialmente: checkpoint NVS aditivo por revisão, resumo local de janelas encerradas durante interrupção, datas específicas antigas e recorte semanal de sete dias. Nenhuma recuperação aciona saída; retrocesso bloqueia avaliação sem recuar checkpoint. Resumo distingue observação ausente de irrigação comprovadamente perdida e pode repetir na queda durante gravação. Cinco executáveis C++/UBSan e cinco fixtures de bytes/hash aprovados; sintaxe, 26 fixtures do contrato e 269 testes do portal aprovados. Compilação ESP32 desta versão bloqueada no download do framework Arduino: checksum divergente no primeiro espelho e HTTP 403 nos seguintes; PlatformIO terminou com PackageException/código 1 antes de compilar. Retenção segura das 128 ocorrências, conflitos por agenda, testes reais NVS/HTTPS/RS485 e campo continuam pendentes. Ver iot/docs/VALIDACAO_FIRMWARE_V021.md. Entrega local; sem publicação remota.

## Firmware experimental 0.2.2 — portal embutido, 15/10/2026

Confirmado portal local já existente e separado em iot/firmware/portal/LocalPortalPage.h, incluído pelo firmware em flash PROGMEM e servido por send_P. Sem partição de arquivos ou uploadfs. Documentadas compilação/upload/acesso e condições de provisionamento. Portal central Node.js permanece separado; interface local atual oferece estado e teste/parada, sem editor visual completo. Publicação solicitada pelo usuário inclui a documentação IoT e firmware 0.2/0.2.1 antes locais. Cinco executáveis nativos C++/UBSan e cinco fixtures de bytes/hash aprovados; página compila em host com stub PROGMEM e JavaScript passa no node --check. Sintaxe do portal central, 26 fixtures de contrato e 269 testes aprovados. Isso não equivale a build ESP32: framework bloqueado nos downloads anteriores e PlatformIO ausente no ambiente atual. Sem placa conectada, gravação ou validação física.

## Firmware experimental 0.3.0 — tempo64, 15/10/2026

FW-07: removidas restrições explícitas de 2037/INT32_MAX. Plataforma fixada pioarduino 55.03.38-1, Arduino oficial 3.3.8/IDF 5.5.4, PlatformIO 6.1.19; assert impede time_t assinado menor que 64 bits e SDK anterior a 5. Calendário/relógio/checkpoint compartilham política ISO 2020–9999. Não promete SNTP além das convenções do SDK até 2104. Atualizadas APIs SHA256/SNTP e diagnóstico. GUID, NVS, partições e contrato do portal preservados; migração física não ensaiada.

Seis executáveis C++/UBSan e cinco fixtures de bytes/hash aprovados, fronteira exata de 2038 e cenários 2040/2100/2400/9999; 26 fixtures de contrato, sintaxe e 269 testes do portal aprovados. Script de seis builds padrão/sintéticos em cópia temporária incluído no CI. Build target bloqueado antes de compilar por SSLCertVerificationError no download do core 3.3.8; nenhum dos seis builds confirmado. Sem gravação, ensaio físico ou promessa de placa pronta. Próximas entregas FW-03a/FW-03b/FW-08/FW-06 em iot/docs/PLANO_ENTREGAS_FIRMWARE.md.
