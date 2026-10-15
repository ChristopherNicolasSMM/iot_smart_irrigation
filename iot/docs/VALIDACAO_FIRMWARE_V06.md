# Firmware experimental 0.6.0 — recuperação, fila e fusos

15/10/2026. Desenvolvimento/publicação autorizados. Nenhuma placa conectada, gravação ou validação elétrica realizada.

## Comportamento entregue

- Backup externo AES-256-GCM e restauração USB do perfil/configuração/calibração no mesmo GUID. Manifesto persistente retomável, validação de revisões e quarentena; token do backup não libera integração central. Guia: [RECUPERACAO_E_PRODUCAO_ESP32.md](RECUPERACAO_E_PRODUCAO_ESP32.md).
- Migração aditiva da fila, cancelamento administrativo individual com digest/motivo e oito registros recentes no mesmo commit que o avanço. Mantém a exigência de fila vazia para trocar destino; não oferece exclusão em massa.
- Eventos novos recebem ID persistido antes do envio. Reenvio automático/manual do anel retido mantém identidade, horário e revisão originais; central deduplica por event_id. Cursor/perdas são persistidos juntos. Não reconstrói amostras, eventos já rotacionados ou registros legados sem identidade/horário. Anel continua de 64 e fila de 16, com uma vaga reservada no backfill.
- 597 nomes/aliases IANA 2026e com transições e regras recorrentes futuras em flash; calendário 2020–9999 sem corte em 2037. Gap é omitido, fold usa primeiro instante. Legislação futura exige atualizar base; não promete SNTP ilimitado. Reprodução/licença: [timezones](../firmware/timezones/README.md).
- Perfil hardened verifica as proteções de boot/NVS antes de iniciar; não prepara bootloader/chaves/eFuses automaticamente. Produção permanece parcial, incluindo proteção do acesso HTTP local e fluxo de provisionamento/recuperação com SDK seguro.

GUID, tabela de partições, envelopes remotos e histórico existentes são preservados. `queue_v2`, `replay_v1`, manifesto/hold de recuperação e campo opcional `audit_origin_floor` são aditivos. Downgrade direto para 0.5.x após migrar a fila não é suportado. Factory.bin completo continua inadequado para atualização preservando NVS; usar upload normal em 0x50000.

## Evidências de software

- 14 executáveis C++ com UBSan; cinco fixtures de bytes/hash da configuração.
- Adaptadores ArduinoJson reais no host: provisionamento/redação/patch/NUL e roundtrip/recusa de metadados inválidos da fila.
- Sete testes Python já existentes de USB/layout; sete novos testes de backup (senha incorreta, adulteração, limite, aleatoriedade, exclusividade/permissões do arquivo, resposta USB fragmentada/UTF-8 e limite de resposta).
- Falha injetada em cada uma das 13 operações da aplicação da recuperação: manifesto retido até concluir, repetição idempotente, GUID/fila/ocorrências preservados, quarentena mantida. Não equivale a cortar alimentação em NVS real.
- 163.755 instantes comparados com ZoneInfo lendo o mesmo catálogo IANA: 597 nomes, ±1 segundo de transições explícitas, meses além de 2038 e limites futuros. Casos C++ de horário inexistente/repetido, Lord Howe, Chatham, Kathmandu e ano 9999.
- 285 testes Node, incluindo reenvio do mesmo event_id em outra mensagem/boot sem duplicar o histórico central; sintaxe e 26 fixtures/consistência OpenAPI.
- Chromium 149: 22 cenários da página embutida em API HTTP sintética; desktop e móvel de 390 px sem transbordamento, zero erros JS. Catálogo IANA, reenvio com ID/cursor, cancelamento obsoleto recusado, motivo/digest e texto sem HTML, quarentena/liberação explícita, permissões, formulários e rascunhos. Inspeção visual realizada. Relatório: LOCAL_PORTAL_UI_V06.json. Não executa handlers Arduino ou autenticação real.

Sete builds alvo aprovados: matriz/bancada/nó padrão e sintético, mais perfil hardened. Argumentos/imagens auxiliares conferidos fora das partições de dados; aplicativo combinado conferido byte a byte em 0x50000. Recursos estáticos em BUILD_FIRMWARE_V06.json. Builds em cópia temporária, sem upload/binário de entrega. Avisos existentes de OneWire/GPIO e nome legado da partição reservada permanecem, sem serem silenciados. O hardened compilado com SDK padrão deve bloquear no boot, sem ser tratado como imagem de produção cifrada.

## Próxima validação e pendências reais

O primeiro ensaio continua [PRIMEIRO_ACESSO_ESP32.md](PRIMEIRO_ACESSO_ESP32.md): build/upload, cadastro USB, Wi-Fi 2,4 GHz e portal, com saídas desabilitadas. Depois medir heap/pilha/latência durante TLS, catálogo de fusos, backup/restauração e lotação NVS; testar quedas durante manifesto/fila, rotação/revogação e perda/reconexão real. Estabilidade, sensores, driver/solenoide, RS485/100 m e migração física não foram validados.

FW-06 de produção ainda exige integração do SDK/bootloader/partição de chaves e fluxo seguro de instalação/recuperação em hardware adequado. Esse desenvolvimento não foi encerrado pelo simples bloqueio hardened. FW-04/FW-05/HW-01/HW-02 continuam dependentes de bancada/campo. Não há pendência de compilar a página separadamente ou enviar uploadfs: o portal é embutido na flash pelo build normal.
