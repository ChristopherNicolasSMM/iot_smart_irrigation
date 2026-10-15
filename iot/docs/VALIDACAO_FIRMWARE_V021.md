# Firmware experimental 0.2.1 — recuperação de observação

15/10/2026. Continuidade de FW-03; não encerra validação de hardware.

## Mudança

A matriz/bancada persiste a cada 60 s um checkpoint JSON NVS `observation` com revisão e epoch. Após SNTP e configuração, compara esse registro com o relógio atual. Registra na auditoria local, por conjunto, `recovery_summary` com `unobserved_closed_windows_N` e, quando aplicável, `weekly_history_limited_7d`. São janelas completamente encerradas entre os instantes; nenhuma gera comando ou alimentação de saída. Lacunas em execução acima de 3 s também são examinadas. Janelas ainda abertas no retorno são consumidas no journal antes de liberar avaliação, impedindo início tardio também no modo híbrido, com resumo `open_window_not_resumed`. Falha ou saturação desse journal mantém automação bloqueada.

Datas específicas são examinadas independentemente da idade, dentro das datas suportadas pelo firmware. Recorrências semanais têm recorte de sete dias, com aviso explícito. Trabalho limitado a 64 slots × no máximo nove datas; não percorre os dias de toda a interrupção.

Primeiro uso sem checkpoint ou revisão diferente estabelece a observação atual, sem atribuir uma configuração nova ao passado. Retrocesso do relógio bloqueia avaliação até alcançar o instante anterior, sem regravar checkpoint para trás. Registro inválido ou falha de escrita causa bloqueio de automação, preservando dados para diagnóstico.

## Compatibilidade e limites

- Chave NVS aditiva; GUID, journal de ocorrências, calibração, configuração e contrato central preservados. Sem alteração de partições ou banco do portal.
- Resumo não prova falha de irrigação: o intervalo entre checkpoint e queda pode incluir funcionamento observado antes da queda. No máximo um minuto em condições normais; não há garantia de tempo real sob bloqueio do loop.
- Resumo local, sujeito ao anel existente de 64 registros; sem transporte novo ao portal, nem detalhe por agenda/ocorrência. Pode repetir se houver queda entre auditoria e checkpoint.
- Checkpoint tem resolução de 60 s entre boots. Não substitui journal de deduplicação e não autoriza sua limpeza. Retenção segura das 128 ocorrências, conflitos por agenda e paridade temporal integral continuam pendentes em FW-03.
- Fusos UTC/BRT fixo, sem suporte global IANA/DST; mesma limitação anterior.
- Sem testes de NVS/queda real, relé, comunicação real ou campo. Saídas continuam desabilitadas de fábrica.

## Verificações

- Cinco executáveis C++ com UBSan/-Wall/-Wextra/-Werror e cinco fixtures de bytes/hash aprovados.
- Novo teste de recuperação: data 90 dias antes, limites exatos do intervalo, janela ainda aberta, retrocesso, semanal com recorte, desabilitado e virada de dia em BRT; janela híbrida consumida na recuperação permanece sem acionamento em ticks seguintes.
- `npm run check`, `npm run contracts:check` (26 fixtures) e `npm test` (269 aprovados) sem falhas.
- Compilação ESP32 desta versão bloqueada antes de compilar: PlatformIO Core 6.1.18 e espressif32 6.10.0 instalados, mas o framework Arduino não pôde ser obtido. Resultados da 0.2 não comprovam esta alteração.

Comando de reprodução a partir da raiz (PlatformIO instalado):

```sh
pio run -d iot/firmware -e esp32_matrix -e esp32_sensor_bench -e esp32_remote_node
```

No ambiente desta entrega foi executado com `PYTHONPATH=/tmp/irrigation-platformio python -m platformio` no lugar de `pio`. Código de saída 1. O primeiro arquivo baixado não conferiu SHA-256; não se desabilitou a verificação. Os espelhos seguintes retornaram `Tunnel connection failed: 403 Forbidden`, terminando em:

```text
PackageException: Could not install package 'platformio/framework-arduinoespressif32 @ ~3.20017.0' for 'linux_x86_64' system
```

Não houve gravação em placa nem alteração da versão das dependências para contornar o bloqueio.

## Próxima bancada

Em placa sem carga: obter checkpoint após SNTP, reiniciar depois de uma janela, conferir resumo sem acionamento; repetir com revisão diferente, queda durante gravação e relógio retrocedido. Conferir desgaste/latência NVS em execução prolongada antes de campo.
