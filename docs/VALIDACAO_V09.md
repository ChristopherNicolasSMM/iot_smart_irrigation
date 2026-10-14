# Entrega v0.9.0 — ciclo de vida e transferência de dispositivos

14/10/2026. Implementa P1-02 em software. Controles v0.6–v0.9 aguardam validação de uso; última confirmação explícita da conversa refere-se à v0.5.

## Arquivamento e restauração

Nova aba **Gerenciar dispositivo**, para administradores da propriedade. Dispositivo arquivado permanece listado/identificado e seu histórico pode ser consultado/exportado. Não há exclusão de dispositivo ou histórico.

Arquivar exige saídas ociosas, configuração aplicada, ausência de comando solicitado/recebido/em execução/sem confirmação e ausência de envio externo em andamento. Não para silenciosamente um ciclo ativo: parar/reconciliar antes. Revoga credencial de máquina, desabilita integração, marca mensagens pendentes como cancelled/device_archived sem apagar seus registros e bloqueia operações humanas e APIs de máquina. Simulador interno arquivado deixa de executar regras/coletar novas amostras. O arquivamento não encerra o processo do cliente HTTP; ele conserva sua configuração local desabilitada e pode continuar coletando offline, sem permissão de envio pela credencial revogada.

No cliente HTTP, também exige runtime ocioso recebido nos últimos 90 s e configuração aplicada com **todas as solenoides e agendas desabilitadas**. Fluxo: parar saídas → desabilitar/publicar → aguardar aplicação e runtime ocioso → arquivar. Revogar token sozinho não desliga automação local; por isso não há arquivamento remoto de cliente offline ou sem prova recente de ociosidade. Confirmação de saída elétrica não comprova fechamento hidráulico.

Restaurar mantém o mesmo GUID/IDs e valores de configuração, mas agendas e solenoides ficam desabilitadas. No simulador a revisão segura é aplicada localmente; no HTTP fica pendente até nova credencial/ACK. Integração continua desabilitada e mensagens canceladas não voltam à fila. Habilitar acionamentos exige editar/publicar explicitamente depois. Coleta simulada recomeça com intervalo de 900 s; não retoma ciclo interrompido.

## Transferência

Somente administrador do sistema, também administrador nas propriedades de origem/destino. Exige dispositivo arquivado e condições ociosas. Escolher outra propriedade autorizada e área pertencente ao destino. Todos os conjuntos recebem essa área; reconfigurar áreas individuais posteriormente, se necessário. Fuso do dispositivo é preservado.

Preserva GUID, IDs/nome/tipos dos componentes, nomes/modos/horários/limites dos conjuntos e agendas; mantém saídas/agendas desabilitadas. Cria revisão nova para o destino. Dispositivo chega **arquivado**, devendo ser restaurado e provisionado. Credencial anterior é revogada; perfil/cabeçalhos de integração e caches de leituras/execução da origem não são levados ao novo vínculo. Filas/comandos antigos continuam armazenados e pertencem à origem.

Depois da transferência, telemetria deve informar configuration_revision do vínculo atual; pacotes/eventos de revisão anterior são rejeitados. IDs de comando da origem não podem ser reutilizados para obter estado ou confirmar comandos no destino. Para reprovisionar cliente independente, guardar o banco antigo como evidência/backup e iniciar com CLIENT_DB_PATH novo e nova credencial. Não reenviar fila antiga com o token novo.

## Histórico e acesso

Propriedade de cada registro é persistida separadamente da propriedade atual do dispositivo. Transferência não move amostras/eventos/revisões/filas/comandos antigos ao destino. Novos registros pertencem ao destino, mesmo quando o relógio do simulador difere da data real da transferência.

A origem continua consultando/exportando seu histórico por GUID, incluindo pelos endpoints antigos. Em Histórico, seletor próprio **Dispositivo atual ou histórico da propriedade** permite escolher dispositivo transferido em modo histórico; não expõe diagnóstico/configuração atual/acionamento do destino. Catálogos de sensores/conjuntos/tipos também são limitados à propriedade da consulta. O destino não recebe histórico anterior, mas recebe identidades/configuração do dispositivo conforme a transferência autorizada.

Se dispositivo voltar a propriedade anterior, esta vê seus registros anteriores e novos daquele vínculo, nunca os registros gerados nas outras propriedades. Troca de propriedade/usuário limpa a consulta fixa no navegador para evitar reaproveitar dados da seleção anterior.

Auditoria registra ator, instante UTC real, propriedade, ação e detalhe, sem segredos. A origem registra arquivamento/transferência de saída; destino registra entrada/restauração. API limita consulta à propriedade autorizada. A aba mostra as últimas 100 ações do dispositivo atual; histórico da origem também registra evento de transferência e a API permite consultar sua auditoria.

## Migração incremental

Adiciona property_id e índice por propriedade/dispositivo em events, samples, configuration_versions, outbox e machine_commands. Registros anteriores da v0.8 são atribuídos à propriedade atual comprovada do GUID nessa migração (v0.8 ainda não tinha transferências). IDs, corpos JSON, timestamps e valores permanecem preservados.

Tabelas novas: device_registry (propriedade atual), device_transfers (origem/destino e configuração de origem congelada), device_audit. Triggers atribuem propriedade às inserções a partir do registro atual. Metadados de ciclo de vida ficam no estado JSON. A migração é repetível e não reatribui registros já marcados. Registros órfãos sem dispositivo correspondente permanecem sem atribuição/acesso pela interface; recuperação exige evidência, não adivinhação.

**Antes de atualizar:** parar servidor e cliente, copiar os bancos para backup, executar git pull --ff-only e reiniciar npm start com Node 24+. Não apagar data/. Backup/restauração automatizados e encriptação em repouso continuam em P1-06; retenção/reprocessamento geral da fila em P1-05. O banco preserva mensagens antigas e seus segredos para a origem; a transferência remove cabeçalhos atuais, não reescreve histórico de envio.

## Verificações

- npm test: **164 testes aprovados**, 152 anteriores e 12 novos de ciclo de vida.
- npm run check, git diff --check e conferência estática de IDs HTML/JS.
- Cobertura: permissões, saída ativa, configuração pendente, envio em andamento, comando desconhecido, arquivo/restauração, runtime HTTP real, token revogado, fila antiga rejeitada, origem/destino, catálogo/exportação, segredos, auditoria, migração/reinício e IDs de comando antigos.
- Sem nova validação visual automatizada ou acionamento físico de ESP32.

## Roteiro de uso

1. Gerenciar dispositivo: arquivar simulador ocioso, avançar 15 minutos e confirmar que não surgem amostras/acionamentos. Tentar acionar: operação bloqueada.
2. Restaurar e verificar agendas/solenoides desabilitadas; GUID/identificadores preservados. Editar/publicar somente quando quiser retomar operação.
3. Cadastrar segunda propriedade/área. Gerar amostras na origem, arquivar e transferir. Na origem, selecionar GUID no Histórico e conferir registros anteriores.
4. Selecionar destino: dispositivo está arquivado; restaurar, verificar área dos conjuntos e integração desabilitada. Histórico do destino não deve conter amostras anteriores.
5. Gerar novas amostras no destino, voltar à origem e conferir que esses registros não são visíveis nem exportados lá.
6. No HTTP, aplicar configuração com saídas/agendas desabilitadas e aguardar runtime ocioso. Arquivar. Token antigo deve retornar 401. Depois de restaurar/provisionar, usar novo banco de cliente e token.
7. Operador pode consultar histórico, mas não arquivar/restaurar/transferir. Administrador de propriedade pode arquivar/restaurar; transferência exige administrador do sistema e acesso administrativo ao destino.

Próxima etapa: P1-05, gestão da fila com cancelamento/reprocessamento e retenção auditada. Firmware continua após fechamento do piloto e avaliação completa de hardware.
