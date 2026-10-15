# Entregas de aprimoramento do firmware

15/10/2026. Ordem antes/depois da primeira bancada, sem prazo físico presumido.

| Ordem | ID | Entrega | Critério de software |
|---|---|---|---|
| 1 | FW-07 | Base temporal de 64 bits | Plataforma fixa, asserts contra regressão, validação uniforme e testes além de 2038; entrega 0.3.0 |
| 2 | FW-03a | Retenção segura de ocorrências | Implementado em software na 0.3.1: corte temporal/journal recente, migração conservadora e testes de falha/16 mil ocorrências; target/bancada pendentes |
| 3 | FW-03b | Conflitos e recuperação detalhados | Implementado no escopo 0.3.2: contexto por agenda, conflitos, recuperação e telemetria deduplicada; limites de fila/histórico offline explícitos |
| 4 | FW-08 | Portal local completo | Implementado no escopo 0.4.0: editor visual, sensores/calibração, eventos paginados e fila; testes nativos/modelo/navegador; target/bancada pendentes |
| 5 | FW-06 | Provisionamento e atualização | Parcial 0.5.0: rede/contas/integração/bindings via NVS e GUI/USB, recuperação física e GUID preservado; cifragem/produção, migração física e bancada pendentes |

FW-03 permanece parcial: a 0.2.1 já trouxe resumo de observação e bloqueio de retomada tardia; retenção implementada em software na 0.3.1; detalhes por agenda implementados no escopo 0.3.2; memória/target/bancada e histórico remoto offline continuam pendentes. Não confundir funcionalidades do simulador Node com equivalência integral no firmware.

A primeira bancada deve partir de um build confirmado da versão escolhida, provisionamento privado e saídas desabilitadas. Começar com portal/sensores e depois driver sem válvula. Ensaios elétricos/hidráulicos, HTTPS real e 100 m continuam em FW-02/FW-04/FW-05/HW-01/HW-02. Não há placa conectada a este ambiente, nem acesso MCP à placa confirmado.

MQTT, OTA e fusos globais ficam fora desta primeira sequência, salvo mudança explícita de escopo.

Conferência 0.5.1: seis builds alvo aprovados, com tempo64 e endereços de gravação corrigidos/verificados. Próximo marco é a bancada de Wi-Fi/portal do usuário; não equivale a validação física. Ver VALIDACAO_FIRMWARE_V051.md.
