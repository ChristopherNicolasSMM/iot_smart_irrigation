# Entregas de aprimoramento do firmware

15/10/2026. Ordem antes/depois da primeira bancada, sem prazo físico presumido.

| Ordem | ID | Entrega | Critério de software |
|---|---|---|---|
| 1 | FW-07 | Base temporal de 64 bits | Plataforma fixa, asserts contra regressão, validação uniforme e testes além de 2038; entrega 0.3.0 |
| 2 | FW-03a | Retenção segura de ocorrências | Não esgotar em 128 eventos; não repetir após queda, relógio retrocedido ou troca de revisão; migração conservadora dos registros atuais |
| 3 | FW-03b | Conflitos e recuperação detalhados | Identificar agendas/ocorrências executadas/omitidas, deduplicar registros e encaminhar ao portal sem inventar confirmação de água |
| 4 | FW-08 | Portal local completo | Telas para APIs existentes de configuração, sensores/calibração, eventos e fila; permissões, CSRF e limites preservados |
| 5 | FW-06 | Provisionamento e atualização | Configuração sem recompilar, preservação de GUID/dados e plano explícito para credenciais, recuperação e migrações |

FW-03 permanece parcial: a 0.2.1 já trouxe resumo de observação e bloqueio de retomada tardia; retenção e detalhes continuam pendentes. Não confundir funcionalidades do simulador Node com equivalência integral no firmware.

A primeira bancada deve partir de um build confirmado da versão escolhida, provisionamento privado e saídas desabilitadas. Começar com portal/sensores e depois driver sem válvula. Ensaios elétricos/hidráulicos, HTTPS real e 100 m continuam em FW-02/FW-04/FW-05/HW-01/HW-02. Não há placa conectada a este ambiente, nem acesso MCP à placa confirmado.

MQTT, OTA e fusos globais ficam fora desta primeira sequência, salvo mudança explícita de escopo.
