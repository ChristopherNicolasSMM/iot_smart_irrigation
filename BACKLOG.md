# Backlog — IoT Smart Irrigation

Atualizado: 13/10/2026 · entrega v0.5.0. Fonte canônica das próximas entregas.

Status: **Entregue/testado** = implementação e testes de software; **A validar** = depende do usuário; **Pendente** = não implementado; **Parcial** = cobertura/escopo ainda incompletos. Nenhum status de software comprova validação física.

## Entrega atual

| ID | Prioridade | Item | Status | Evidência / próxima ação |
|---|---|---|---|---|
| P0-01 | P0 | Validar visualmente portal, checkboxes e horários | A validar | Execução inicial v0.2 confirmada; coletar retorno de v0.4/v0.5 |
| P0-02 | P0 | Políticas de tempo e recuperação | Entregue/testado nos cenários principais | Relógio monotônico, ajuste até 7 dias, falha de energia, DST comum; ampliar transições raras |
| P0-03 | P0 | Protocolo de dispositivo independente | Entregue/testado em software | Cliente HTTP separado, config/hash/ACK, comandos, expiração, reconciliação; hardware posterior |
| P0-04 | P0 | Conflitos, simultaneidade e duração máxima | Parcial | Bloqueio e log em execução; aviso antecipado de sobreposição na configuração ainda pendente |
| P0-05 | P0 | Conjunto por solenoide com umidade/temperatura/área e agenda própria | Entregue/testado | zones/zone_id preservados; até 4 conjuntos, 1 solenoide por conjunto |
| P0-06 | P0 | Backlog na raiz e manutenção por entrega | Entregue | Este arquivo e AGENTS.md |

## Próximas entregas

| ID | Prioridade | Item | Dependência | Critério de conclusão |
|---|---|---|---|---|
| P1-01 | P1 | Completar gestão de contas: senha, desativação e revogação de vínculo | Acesso por propriedade | Sessões abertas perdem acesso conforme alteração |
| P1-02 | P1 | Desativar/arquivar dispositivos e transferir propriedade | Protocolo e auditoria | Histórico/GUID preservados; credencial anterior revogada |
| P1-03 | P1 | Diagnóstico de comunicação/leitura e próxima execução por conjunto | P0-02/P0-03/P0-05 | Última comunicação, idade da leitura, motivo de bloqueio e próxima agenda claros |
| P1-04 | P1 | Histórico com filtros, todos os sensores e exportação | Dados e conjuntos | Consultar período/componente/conjunto e distinguir ausência de dados |
| P1-05 | P1 | Fila: cancelamento, reprocessamento e retenção | Envio HTTP | Limpeza auditada sem apagar pendências silenciosamente |
| P1-06 | P1 | Backup/restauração e proteção de segredos | SQLite e contas | Recuperação demonstrada; definir encriptação/gestão de chave para produção |
| P1-07 | P1 | Ensaio prolongado, desempenho e refatoração | Portal/cliente separados | Teste de estabilidade, limites medidos, módulos legíveis |
| P1-08 | P1 | Avisos prévios de sobreposição e limites hidráulicos | P0-04 | Editor identifica conflito antes de publicar; operação continua protegida |
| P1-09 | P1 | Calibração e tratamento de dados do sensor | Hardware a definir | Unidade relativa/volumétrica correta, qualidade e calibração explícitas |
| P1-10 | P1 | Expandir testes temporais raros e recuperação longa | P0-02 | Transições além do DST comum e reconciliação após longas paradas |
| P1-11 | P1 | Encaminhamento/integração dos dados do cliente HTTP para terceiros | P0-03/envio HTTP | Aplicar perfil de saída a dados recebidos sem loops; canal de controle preservado |
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
- Histórico a cada 900 s; eventos imediatos. Fila configurável, retenção ainda pendente.
- Implementação de portal/clientes é simulada; nenhuma solenoide física é acionada pelo Node.

## Sequência recomendada

1. Validar v0.5 e concluir P0-04/P1-08 com correções de uso.
2. Gestão de acesso/dispositivos, diagnóstico e histórico.
3. Fila/retencão, segredos, backup, estabilidade e contrato definitivo.
4. Avaliação de hardware, firmware e validação física.

## Política de atualização

Toda entrega funcional deve atualizar status, evidência, limitações, versão e próximos itens neste arquivo. Não marcar validação visual/física como concluída com base somente em testes de código. Manter IDs estáveis, registrar novos pedidos e preservar itens pendentes. Progresso dos relatórios por versão complementa este backlog, sem substituí-lo.
