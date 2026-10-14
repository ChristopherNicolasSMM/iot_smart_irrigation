# Backlog — IoT Smart Irrigation

Atualizado: 14/10/2026 · entrega v0.12.0. Fonte canônica das próximas entregas.

Status: **Entregue/testado** = implementação e testes de software; **A validar** = depende do usuário; **Pendente** = não implementado; **Parcial** = cobertura/escopo ainda incompletos. Nenhum status de software comprova validação física.

## Entrega atual

| ID | Prioridade | Item | Status | Evidência / próxima ação |
|---|---|---|---|---|
| P0-01 | P0 | Validar visualmente portal, checkboxes e horários | A validar | v0.5 validada pelo usuário; controles v0.6–v0.10 e fluxo atual aguardam validação de uso na próxima entrega consolidada |
| P0-02 | P0 | Políticas de tempo e recuperação | Entregue/testado nos cenários principais | Relógio monotônico, ajuste até 7 dias, falha de energia, DST comum; ampliar transições raras |
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
| P1-09 | P1 | Calibração e tratamento de dados do sensor | Hardware a definir | Unidade relativa/volumétrica correta, qualidade e calibração explícitas |
| P1-10 | P1 | Expandir testes temporais raros e recuperação longa | P0-02 | Transições além do DST comum e reconciliação após longas paradas |
| P1-11 | P1 | Encaminhamento/integração dos dados do cliente HTTP para terceiros | Entregue/testado v0.12 | Perfil por GUID, IDs estáveis/origem, rastros de salto, deduplicação/rollback e controle independente |
| P1-12 | P1 | Consolidar schemas/OpenAPI e contrato versionado | P0-03 | Fixtures comuns para portal, cliente e futuro firmware |

## Firmware e hardware — após fechamento do piloto

| ID | Prioridade | Item | Dependência | Critério de conclusão |
|---|---|---|---|---|
| HW-01 | P2 | Escolher ESP32, flash, sensores, válvulas, drivers e fonte | Avaliação de hardware | Lista e pinagem aprovadas; estado seguro e capacidades definidos |
| HW-02 | P2 | Medir memória, TLS, armazenamento e limites de agenda/fila | HW-01/protocolo | Limites do Node não presumidos para ESP32 |
| FW-01 | P2 | Firmware C++: drivers, controlador local e portal leve | HW-01/HW-02 | Build reproduzível e mesmas regras verificadas |
| FW-02 | P2 | Compilação/bancada/campo com usuário | FW-01 | Evidências físicas de boot, duração, sensores, falhas e retomada |
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

## Duas entregas consolidadas aprovadas

1. **Entrega atual v0.12 — etapas 1 a 3:** P1-06 proteção/recuperação, P1-07 estabilidade/desempenho no ensaio delimitado e P1-11 encaminhamento HTTP. Detalhes e limites em docs/VALIDACAO_V012.md. Alerta nativo de encerramento do ensaio longo registrado para reprodução; código 0 e asserções aprovadas, sem afirmar ensaio livre de alertas.
2. **Próxima entrega — etapas 4 a 6:** P1-12 schemas/OpenAPI/fixtures, P1-10 testes temporais raros/longas paradas e P0-01 conferência de uso/roteiro de validação do portal. Não implementadas antecipadamente nesta v0.12.
3. **Depois das duas entregas:** avaliação conjunta, escolha/aprovação de hardware HW-01, medidas/limites HW-02 e preparação para firmware. Validação visual final do usuário e ensaio prolongado real na hospedagem não são substituídos por testes de software.

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

## Política de atualização

Toda entrega funcional deve atualizar status, evidência, limitações, versão e próximos itens neste arquivo. Não marcar validação visual/física como concluída com base somente em testes de código. Manter IDs estáveis, registrar novos pedidos e preservar itens pendentes. Progresso dos relatórios por versão complementa este backlog, sem substituí-lo.
