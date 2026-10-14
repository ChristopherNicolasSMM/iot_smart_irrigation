# Entrega v0.10.0 — gestão da fila e retenção auditada

14/10/2026. P1-05 entregue no portal. Última confirmação explícita do usuário: v0.5; telas v0.6–v0.10 aguardam validação de uso.

## Fila de envio

Nova aba **Fila de envio**, com seleção de dispositivo atual/histórico, filtro de estado, contagem total, paginação, total de tentativas, tentativas do ciclo, reprocessamentos e auditoria. Administrador da propriedade executa ações; operador consulta. Resultados não incluem payload, perfil nem cabeçalhos secretos. Integrações mantém sua consulta resumida anterior.

A fila é de mensagens HTTP de dados do portal. Comandos de irrigação e a fila local do cliente independente são separados; essas ações não reexecutam comandos no motor de irrigação. Cliente independente recebe a migração de contadores da classe Outbox, mas sua gestão/retencão local pela interface continua fora desta entrega.

Página de 100 registros por ordem de inserção decrescente, filtro antes do limite; carregar anteriores até 5000 itens na tela. Contagens consideram todos os registros da propriedade. Estados podem mudar enquanto o worker envia: **Atualizar fila** abre nova consulta. Não há promessa de snapshot imutável de status entre páginas.

## Cancelamento

Somente mensagens pending podem ser canceladas. Atualiza estado/instante/motivo e registra ator/auditoria; conserva ID, conteúdo e contadores. Não cancela envio sending em andamento nem mensagem entregue. Cancelar um retry depois de timeout impede próximas tentativas, mas não desfaz processamento que o receptor já possa ter feito sem ACK.

O arquivamento de dispositivo continua cancelando pendências de envio, agora registrando também instante de encerramento. Não reativa mensagens canceladas automaticamente.

## Reprocessamento

Somente dead/cancelled com conteúdo disponível, dispositivo ativo e vínculo atual comprovado. Mantém **mesmo ID, payload e perfil original**. Reinicia budget do ciclo, conserva total de tentativas e incrementa reprocess_count. Cada ciclo respeita max_attempts do perfil capturado (máximo 5), backoff e confirmação anteriores. Reprocessamento offline pode ficar aguardando comunicação; respeita capacidade atual da fila.

Por padrão, usa cabeçalhos capturados originalmente. Opção explícita **Usar cabeçalhos atuais** permite corrigir autenticação apenas se a URL original for exatamente igual à URL do perfil atual. Não envia credenciais novas para outro destino. Auditoria registra mudança de autenticação, nunca seu valor.

Receptor precisa deduplicar por message_id: timeout/ACK perdido pode significar processamento já ocorrido. Preservar ID permite deduplicação, não garante execução exatamente uma vez em receptor sem esse contrato. Perfil/modelo alterado posteriormente não modifica o conteúdo capturado da mensagem antiga.

Transferência mantém propriedade dos registros e invalida reprocessamento de mensagens de vínculos anteriores, inclusive se GUID voltar à mesma propriedade. Mensagens v0.9 sem prova da revisão do vínculo, em dispositivo já transferido, não recebem autorização de reprocessamento por adivinhação. Pendências legítimas existentes continuam sendo processadas segundo sua propriedade; worker não envia registros de outra propriedade quando dispositivo muda de vínculo.

## Retenção manual com prévia

Não há limpeza automática nesta versão. Prazos da fila/retencão usam UTC real e dias de 24 horas; acelerar o simulador não envelhece as mensagens. Administrador seleciona dispositivo/propriedade e idade mínima (1–3650 dias), solicita prévia e decide aplicar pela interface. A prévia é válida por 5 minutos e vinculada ao ator, propriedade, GUID e registros candidatos. Até 100 prévias em memória; reiniciar servidor exige gerar nova prévia.

Elegíveis: delivered/dead/cancelled ainda com conteúdo, encerrados antes do corte. Usa finished_at; na ausência, delivered_at e depois created_at. Para legado sem término conhecido, a prévia informa quantos usam criação como referência. Avaliar esses registros antes de confirmar; data de criação não comprova idade desde encerramento.

Prévia lista contagem por estado, pendências protegidas, tamanho estimado e amostra de IDs. Lote limitado a 5000 mensagens e **16 MiB** de conteúdo/perfis/cabeçalhos; havendo mais, repetir prévia após aplicar. Uma alteração relevante na fila invalida a prévia antes de limpar qualquer conteúdo. Execução é transacional: falha no banco reverte conteúdo e auditoria.

Aplicar remove body/profile/headers dos candidatos e marca retained=1. Mantém registro, ID, propriedade, status, timestamps, contadores e auditoria, incluindo SHA-256 do payload original. Não apaga eventos/amostras de irrigação, revisões, contas, IDs de inbox/deduplicação ou mensagens pending/sending. Conteúdo removido não pode ser reprocessado. Remoção lógica não garante eliminação física de bytes antigos em páginas SQLite, WAL ou backups. Trata-se de remoção de conteúdo, não eliminação de tombstones: crescimento de registros/backup/compactação será medido em P1-07/P1-06.

Mensagens antigas de dispositivo transferido podem ter conteúdo retido pela propriedade de origem, sem acesso às mensagens do destino. Isso não é autorização para reenviá-las no vínculo novo. Retenção não remove os cabeçalhos atualmente configurados do dispositivo; afeta somente snapshots das mensagens selecionadas.

## Migração e atualização

Migração incremental acrescenta cycle_attempts, reprocess_count, finished_at, retained e ownership_revision à outbox, além de queue_audit. Inicializa ciclo com tentativas já consumidas; não zera tentativas, modifica IDs/payload/credenciais ou aplica retenção durante atualização. Propriedade permanece intacta. Novas mensagens do portal guardam revisão inicial do vínculo; registros anteriores sem prova permanecem desconhecidos para decisões de reprocessamento depois de transferência.

Parar servidor e cliente, copiar bancos para backup, executar git pull --ff-only e reiniciar npm start com Node 24+. Reiniciar npm run client, se usado, para migrar também os contadores de seu banco local. Não apagar data/. Segredos em repouso e backup/restauração automatizados continuam em P1-06.

## Verificações executadas

- npm test: **184 testes aprovados**, 164 anteriores e 20 novos.
- npm run check, git diff --check e conferência estática dos IDs HTML/JavaScript.
- Testes: cancelamento/inflight, retry com budget e ID preservados, deduplicação após ACK perdido, cabeçalhos/destino, capacidade/vínculo, prévia/expiração/ator/propriedade, limites de bytes, rollback transacional, proteção de pendências, isolamento/API/perfis, paginação, migração e persistência após reinício.
- Sem validação visual automatizada nova ou acionamento físico de ESP32.

## Roteiro de validação

1. Configurar integração de teste e gerar mensagens; abrir Fila de envio e filtrar estados. Confirmar contadores totais e carregar páginas anteriores quando necessário.
2. Com comunicação pausada/offline, cancelar uma mensagem aguardando; ao reconectar, confirmar que não foi reenviada.
3. Produzir falha definitiva em receptor de teste, corrigir a causa e reprocessar. Conferir mesmo ID/conteúdo, contador total preservado e novo ciclo. Receptor deve deduplicar IDs.
4. Se corrigiu cabeçalho de autenticação, marcar opção correspondente com mesmo destino. URL diferente deve impedir atualização de cabeçalhos no reprocessamento.
5. Gerar prévia de retenção de registros antigos e conferir estados/datas/legado. Em fila recente, zero candidatos é esperado. Confirmar somente se quiser remover definitivamente o conteúdo; depois, IDs/auditoria continuam e reprocessar fica indisponível.
6. Alterar uma mensagem candidata após prévia: aplicar deve solicitar nova prévia, sem remoção parcial. Verificar também expiração após 5 minutos.
7. Operador consulta, mas não possui ações de gestão. Usuário de outra propriedade não consulta/altera os IDs.

Próxima etapa: P1-06, backup/restauração e proteção de segredos, seguida de estabilidade/desempenho P1-07 e contrato definitivo P1-12. Firmware continua após fechamento do piloto e avaliação completa de hardware.
