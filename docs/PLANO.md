# Plano de entrega — v0.5.0

Atualizado em 13/10/2026. Consulte [BACKLOG.md](../BACKLOG.md) para prioridades, status, decisões e pendências. Este plano descreve fases; não mantém uma segunda lista concorrente de tarefas.

| Fase | Estado | Condição para avançar |
|---|---|---|
| Requisitos/arquitetura | Documentados, evoluem com pedidos | Requisitos e decisões versionados |
| Portal/API/simulação | Em desenvolvimento; v0.5 entregue | Validação de uso, segurança, recuperação e estabilidade |
| Avaliação de hardware/firmware | Pendente | Placa, sensores, válvulas, drivers, fonte e pinagem definidos |
| Firmware C++ | Pendente | Avaliação anterior, contrato e limites de memória fechados |
| Bancada/campo | Pendente | Build reproduzível e evidências físicas |

## Entregas

- v0.2: portal central/local, controlador interno e persistência.
- v0.3: fila/envio/recepção HTTP e revisões desejada/aplicada.
- v0.4: propriedades/áreas, acesso e horários individuais por dia.
- v0.5: conjuntos por solenoide, políticas temporais principais, cliente HTTP independente e backlog na raiz.

Relatórios de cada versão em VALIDACAO_V02.md a VALIDACAO_V05.md. Testes de software não encerram validação visual ou física.

## Próximos blocos

1. Validar conjuntos/agendas/cliente da v0.5 e implementar avisos prévios de conflito.
2. Gestão completa de acesso/dispositivos, diagnóstico, filtros/exportação de histórico.
3. Retenção, reprocessamento, backup, proteção de segredos, estabilidade e schemas do protocolo.
4. Avaliar hardware e então implementar firmware.

## Decisões já adotadas

Node 24+/SQLite/HTML/CSS/JS para piloto; perfis administrador/operador; 0–4 sensores por tipo e 0–4 solenoides; até 4 conjuntos; agenda por conjunto; temperatura somente monitoramento; padrão 1 saída simultânea; teste padrão 5 s/máximo 30 s; híbrido com um ciclo por janela. Configuração de produção/hardware continua pendente.

## Estimativas

As estimativas originais eram preliminares e não são saldo de horas restantes. Reestimar cada pacote com escopo e critérios do backlog. Firmware tinha referência inicial de 140–240 h para uma placa/drivers definidos, sem PCB própria; confirmar após avaliação física, sem compromisso de prazo.

## Avaliação antes do firmware

Modelo ESP32/flash/PSRAM, alimentação, válvula DC/AC/biestável, estado seguro, drivers/proteções, barramentos/pinagem com Wi-Fi, calibração, partições/TLS, armazenamento, fila, relógio monotônico, watchdog e distribuição. Medir memória com portal/rede/sensores ativos; limites do Node não são automaticamente limites do ESP32.

Em novas conversas, referenciar este repositório, BACKLOG.md, AGENTS.md e docs/REQUISITOS.md. Ler estado atual antes de implementar.
