# Validação de software — firmware experimental 0.4.0

15/10/2026. Entrega FW-08: portal local incorporado à flash com fontes reproduzíveis. Não há ESP32 conectado ou validação física nesta entrega.

## Alterações

Editor visual para componentes/conjuntos e agendas semanais por dia/horário ou data, sensores opcionais, duração/término e meia-noite. Importação/exportação e JSON avançado preservam IDs; migração visual de agenda semanal legada não altera a origem. Novos IDs são aleatórios, evitando reutilização sequencial automática. Remoção de referências em uso é bloqueada pelo editor.

POST /api/configuration/draft recebe `{expected_revision, configuration}`. Exige admin, CSRF, autoridade local e ociosidade; rejeita revisão obsoleta/exaurida. Calcula SHA-256 sobre os bytes originais da configuração, cria envelope da revisão seguinte e reutiliza applyEnvelope, validação/persistência existentes. O documento de rascunho é liberado antes da aplicação para reduzir pico de heap. O browser não usa crypto.subtle, que dependeria de contexto seguro em HTTP LAN. Falhas preservam o rascunho; atualização automática de estado não sobrescreve edição.

GET /api/state informa perfil/permissões; GET /api/sensors acrescenta calibração/ROM salva por slot. Matriz não calibra sensor local; nó aceita somente slot 0. Eventos locais retornam até dez por página em ordem decrescente, next_before decimal como string/null e limite retido de 64. Rotação pode remover registros entre páginas. Consumidores antigos precisam implementar paginação. Contrato remoto v1, GUID, NVS, partições e envelopes anteriores preservados.

Fontes HTML/CSS/JS sem dependências externas geram LocalPortalPage.h pelo pre-script PlatformIO. Snapshot de aproximadamente 33 KiB na flash, sem filesystem/uploadfs. Isso não mede tamanho do binário nem pico real de RAM.

## Evidências executadas

- `python iot/firmware/scripts/test-native.py`: nove executáveis C++ com UBSan e cinco fixtures de bytes/hash. Inclui política de autoridade, papel, CSRF, atividade e revisão.
- `npm test`: 280 testes aprovados, incluindo nove testes novos do editor (compatibilidade central, preservação de IDs, referências, data 9999/leap, dias independentes e limites).
- `npm run check` e `npm run contracts:check`: sintaxe e 26 fixtures/consistência OpenAPI aprovadas.
- `npm run test:ui:local` com Playwright e Chromium 149: treze cenários aprovados; relatório em VALIDACAO_PORTAL_LOCAL_V04.json. Mock HTTP local reproduz estado/configuração/sensores/eventos/fila. Inclui CSRF enviado, revisão/conflito, rascunho preservado, calibração, paginação/XSS textual, reprocessamento, teste/parada, relógio inválido, operador, autoridade central, nó, offline e largura de celular. Inspeção das capturas desktop 1366×900 e mobile 390×844 realizada; zero erros JS e sem rolagem horizontal no mobile.

O teste opcional de navegador requer Playwright instalado; aceita UI_PLAYWRIGHT_MODULE, UI_BROWSER_EXECUTABLE, UI_BROWSER_ARGS (array JSON) e UI_REPORT. Nenhum navegador/binário externo é versionado. CI executa o modelo do editor e a verificação do snapshot, além dos testes nativos e builds previstos.

## Limites e próximos passos

As rotas Arduino não foram executadas no teste de navegador. O mock verifica a interface e seus pedidos HTTP, não autenticação Basic real, heap/NVS/TLS/latência ou acionamento. Compilação ESP32 não confirmada nesta versão: PlatformIO ausente no runtime atual; em tentativas anteriores o download do core foi bloqueado por validação de certificado. Não foi desativada validação TLS.

Saídas físicas permanecem desabilitadas por padrão. Wi-Fi, segredos, GPIO, bindings e presença física dos sensores ainda exigem provisionamento privado; o editor lógico não os modifica. Acesso HTTP Basic é restrito à LAN confiável e não oferece logout confiável ou cifragem. Outbox16/auditoria64 e perda contabilizada continuam limitadas. A memória da página é flash, mas parsing/configuração e TLS usam heap: medir no alvo antes de aceitar limites em produção.

Próxima etapa de desenvolvimento FW-06: provisionamento/recuperação de segredos sem recompilar e migrações. Build dos três perfis/padrão+provisionado, testes de bancada e campo continuam pendentes. UTC/BRT fixo permanece; o antigo limite de 2037 já foi removido na 0.3.0 com base de tempo de 64 bits.
