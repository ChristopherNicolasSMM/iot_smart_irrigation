# Firmware experimental 0.3.2 — diagnóstico por agenda

15/10/2026. FW-03b entregue no escopo delimitado abaixo. Sem gravação ou validação física.

## Comportamento

A configuração local preserva IDs originais de agenda e de horário semanal expandido. Eventos de execução estática/híbrida passam a incluir schedule_id, time_slot_id (quando existir), occurrence_id, window_start e window_end. O cálculo do identificador de ocorrência foi extraído para função compartilhada sem alterar os valores usados pelo journal 0.3.1. Configuração/hash/GUID/partições/contrato v1 permanecem iguais.

Conflitos no mesmo conjunto geram `omitted` com `schedule_overlap` ou `active_run_conflict`; quando há agenda concorrente, incluem competing_schedule_id e competing_time_slot_id quando disponível. A ocorrência omitida é consumida antes de liberar o controlador, impedindo início híbrido tardio após o concorrente terminar. Se a persistência falhar, não libera acionamento automático.

A política de seleção preserva a ordem de configuração: uma janela startNow substitui a anterior; em empate de início, vence a última janela percorrida. Janelas retiradas pelo corte de retenção não substituem uma candidata válida. Uma execução já ativa conserva sua própria janela original, sem estender o híbrido por outra agenda sobreposta. Não há fila automática de agendas omitidas.

No instante exato do término de duração, o controlador pode finalizar a execução antiga e iniciar uma nova janela estática no mesmo tick. O diagnóstico não classifica essa fronteira como conflito. Eventos de conclusão conservam a agenda anterior, enquanto reserva/início usam o novo planejamento. Híbrido/dinâmico continuam respeitando pausa mínima e validade de leitura.

## Recuperação e transporte

A varredura de recuperação agora visita cada janela encerrada e gera `recovery_observation` / `unobserved_window` com a agenda, ocorrência e janela originais. Esse evento significa **intervalo sem observação**, não comprovação de falha de irrigação. Janelas abertas no retorno geram `omitted` / `open_window_not_resumed` e permanecem sem retomada.

Datas específicas antigas continuam contempladas; semanais limitam a varredura a sete dias. Eventos expõem history_truncated e weekly_lookback_days quando semanal. Resumo local de truncamento por conjunto permanece. Não há acionamento tardio de janelas recuperadas nem paridade global de fusos IANA: UTC/BRT fixo permanece.

A auditoria é persistida localmente antes do envio pela outbox já existente a /iot/v1/telemetry. Observações usam event_id estável derivado de dispositivo/revisão/slot/instante/motivo; a ingestão central existente deduplica event_id mesmo com outro message_id e horário de recebimento. Uma cache RAM por categoria/slot evita tempestade em ticks repetidos. Após reboot pode haver repetição local e reenvio com o mesmo event_id, sem duplicar o evento central.

**Limite importante:** envio segue a política atual de melhor esforço. A outbox tem 16 mensagens, o anel local 64 registros e lost_events é um contador desde o boot. Saturação não bloqueia irrigação nem cria fila infinita: o evento pode ficar apenas local e a perda de envio é contabilizada. Não há recuperação automática dos eventos locais depois que a fila esvazia; durabilidade completa de histórico remoto offline é evolução pendente. Falha real de persistência local continua bloqueando automação. Não se promete entrega remota de todas as observações de uma interrupção longa.

Os campos novos são extensões opcionais já permitidas pelo schema v1; nenhum endpoint ou tabela mudou. O portal armazena os metadados no JSON do evento e nos fluxos que preservam esse JSON. Não foram criadas colunas/telas novas de filtragem por agenda nesta entrega.

## Evidências

- Oito executáveis C++/UBSan com -Wall/-Wextra/-Werror; cinco fixtures de bytes/hash.
- Política de empate, conflito com execução manual, corte de retenção, IDs estáveis por motivo/revisão, recuperação de data antiga e cancelamento do visitante.
- Fronteira de duração com overflow de millis, seleção da janela original do híbrido e duas janelas estáticas consecutivas com dois acionamentos no tick exato.
- 271 testes do portal aprovados, incluindo ingestão HTTP real local de diagnóstico, preservação de metadados, deduplicação após reboot/reenvio e ausência de comandos criados por observações.
- Sintaxe e 26 fixtures de contrato sem falhas. Sem dependência nova de runtime.

Build ESP32 não executado nesta sessão: o módulo PlatformIO não está instalado no runtime atual (`No module named platformio`). O último bloqueio de download documentado na 0.3.1 foi de certificado; não se declara que os builds 0.3.2 passaram. O CI continua configurado para três builds padrão e três sintéticos. Memória adicional de IDs de agenda/candidatas/cache, latência de NVS/telemetria e funcionamento real precisam ser medidos na placa.

## Próxima entrega

FW-08: ampliar o portal local com telas para configuração, sensores/calibração, eventos e fila, usando as APIs e permissões existentes. Transporte offline sem perdas, filtros específicos de agenda e fusos globais continuam fora desta entrega.
