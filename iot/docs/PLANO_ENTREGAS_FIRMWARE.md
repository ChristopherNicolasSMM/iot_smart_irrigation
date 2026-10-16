# Entregas de aprimoramento do firmware

15/10/2026. Ordem antes/depois da primeira bancada, sem prazo físico presumido.

| Ordem | ID | Entrega | Critério de software |
|---|---|---|---|
| 1 | FW-07 | Base temporal de 64 bits | Plataforma fixa, asserts contra regressão, validação uniforme e testes além de 2038; entrega 0.3.0; 0.6.0 amplia para 597 fusos IANA/DST |
| 2 | FW-03a | Retenção segura de ocorrências | Implementado em software na 0.3.1: corte temporal/journal recente, migração conservadora e testes de falha/16 mil ocorrências; build alvo confirmado; bancada pendente |
| 3 | FW-03b | Conflitos e recuperação detalhados | Implementado no escopo 0.3.2: contexto por agenda, conflitos, recuperação e telemetria deduplicada; 0.6.0 adiciona backfill automático/manual do anel retido, com deduplicação |
| 4 | FW-08 | Portal local completo | Implementado no escopo 0.4.0: editor visual, sensores/calibração, eventos paginados e fila; testes nativos/modelo/navegador; build alvo confirmado; bancada pendente |
| 5 | FW-06 | Provisionamento e atualização | Parcial 0.6.0: backup externo cifrado, restauração retomável/hold, migrações de fila e guard de produção; integração SDK/chaves/produção, migração física e bancada pendentes |

FW-03 permanece parcial: a 0.2.1 já trouxe resumo de observação e bloqueio de retomada tardia; retenção implementada em software na 0.3.1; detalhes por agenda implementados no escopo 0.3.2; histórico offline limitado ao anel recebe backfill na 0.6.0; heap/latência/NVS reais e bancada continuam pendentes. Não confundir funcionalidades do simulador Node com equivalência integral no firmware.

A primeira bancada deve partir de um build confirmado da versão escolhida, provisionamento privado e saídas desabilitadas. Começar com portal/sensores e depois driver sem válvula. Ensaios elétricos/hidráulicos, HTTPS real e 100 m continuam em FW-02/FW-04/FW-05/HW-01/HW-02. Não há placa conectada a este ambiente, nem acesso MCP à placa confirmado.

MQTT e OTA ficam fora desta sequência. Fusos globais foram incluídos na 0.6.0 por solicitação de aprimoramento.

Conferência 0.5.1: seis builds alvo aprovados, com tempo64 e endereços de gravação corrigidos/verificados. Próximo marco é a bancada de Wi-Fi/portal do usuário; não equivale a validação física. Ver VALIDACAO_FIRMWARE_V051.md.

Conferência 0.6.0: 14 executáveis nativos, comparação IANA, 285 testes Node e 22 cenários de navegador em API simulada; resultados dos builds em VALIDACAO_FIRMWARE_V06.md. Pendência de desenvolvimento de produção: SDK/bootloader/NVS cifrada, chaves e recuperação segura; o perfil hardened apenas exige e detecta essas proteções.

Complemento 0.6.0: auditor offline e candidata de partições verificada pelo gerador oficial, sem ativação. FW-06 permanece parcial; ver AUDITORIA_PREPRODUCAO_ESP32.md para pendências de SDK/chaves/migração/recuperação/transporte.
