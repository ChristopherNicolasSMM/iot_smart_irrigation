# Entrega v0.8.0 — histórico, filtros e exportação

14/10/2026. P1-04 implementado em software; controles novos v0.6–v0.8 aguardam validação de uso. Última confirmação explícita da conversa: v0.5.

## Histórico no portal

- Consulta por dispositivo autorizado, período, sensor, conjunto, qualidade de leitura e tipo de evento.
- Período informado em **UTC**, com início inclusivo e fim exclusivo. Tabelas/gráficos exibem o fuso configurado do dispositivo. Campos vazios não restringem período. Exemplo: 06:00 em America/Sao_Paulo corresponde a 09:00 UTC na data do piloto.
- Sensor e conjunto são filtros combinados: ao escolher conjunto, leituras incluem seus sensores de umidade e temperatura associados na coleta. Sensor compartilhado aparece uma vez, com a lista de conjuntos.
- Qualidade aplica-se às leituras; tipo de evento aplica-se aos eventos. Sensor nos eventos exige referência explícita a componente/controlador no registro; eventos genéricos sem referência não são inferidos.
- Tabelas distinguem ausência/invalidez de zero e mostram instantes de coleta e leitura quando diferentes. Rede refere-se ao registro, não à conectividade atual.
- Um gráfico por sensor carregado, inclusive temperatura e sensores históricos removidos. Eixo horizontal usa tempo de coleta real do registro; umidade relativa usa 0–100, temperatura usa faixa dos valores carregados. Unidade desconhecida não é deduzida.
- Leituras inválidas/desabilitadas/ausentes não viram pontos válidos; intervalos acima de 22,5 minutos não são unidos (tolerância de 1,5 × cadência do piloto). Não há interpolação nem garantia de detecção universal de toda perda.

## Contexto histórico

Amostras novas do simulador persistem revisão aplicada, nomes/tipos/estado dos componentes e vínculos de conjuntos no instante da coleta. Sensor removido não gera novas leituras fictícias, e nomes/vínculos antigos não são reescritos após editar a configuração.

Telemetria HTTP com configuration_revision conhecida e aplicada recupera contexto da configuração imutável daquela revisão. O cliente independente já envia esse campo no envelope. Templates externos que omitem revisão precisam acrescentá-la para preservar contexto. Sem revisão comprovada, guarda a leitura e indica associação/unidade desconhecidas, sem atribuir configuração atual por adivinhação.

Registros legados permanecem disponíveis. Sem metadata histórica de grupo, ficam fora do filtro de conjunto e podem ser consultados com Todos os conjuntos ou por sensor. Catálogo inclui IDs atuais/anteriores de configurações e IDs encontrados no histórico; nomes de catálogo servem à seleção, enquanto cada linha conserva o nome capturado quando disponível. Não há reconstituição automática de vínculos ausentes.

## Paginação e consulta fixa

Filtro é aplicado no banco **antes** do limite; não consulta apenas os últimos 500 registros. Página padrão contém até 200 registros de coleta/evento (máximo API 500), podendo representar até oito leituras por coleta. Ordem por ID de recebimento decrescente; gráficos ordenam por instante de coleta, inclusive em retransmissão tardia.

Botões carregam páginas anteriores por before_id e snapshot_id. Novos registros recebidos após início não entram nas páginas seguintes. Consulta permanece fixa durante atualização automática da visão geral; **Consultar / atualizar** abre nova consulta. Alterar filtros exige esse botão para atualizar a tabela; exportação usa os filtros atualmente informados. Ao trocar dispositivo, seleções de sensor/conjunto/tipo de evento são limpas; período/qualidade podem ser mantidos.

Limite de exibição acumulada: 20000 leituras ou 5000 eventos. Ao exceder, solicita reduzir período/filtros. Snapshot é limite superior de ID, não isolamento transacional contra futuras políticas de exclusão/transferência; essas políticas ainda serão implementadas no backlog.

## Exportação

Selecionar leituras/eventos e CSV/JSON. Exporta todos os resultados filtrados, independentemente de quantas páginas foram abertas. Inicia nova consulta no instante do download e pode incluir chegadas posteriores à consulta fixa da tela. Máximo 5000 registros ou 20000 leituras; exceder retorna erro 400 e **não entrega arquivo parcial**. CSV usa UTF-8 com BOM, ponto e vírgula, datas de coleta/leitura normalizadas em UTC, números com ponto decimal, campos entre aspas e escape de aspas internas. Textos que podem ser interpretados como fórmulas recebem prefixo de texto; JSON preserva o texto original. Sem leitura válida, valor ausente permanece vazio/null, não zero.

Exportação exige sessão/vínculo, disponível para administrador ou operador autorizado. Usa colunas explícitas de histórico/evento; não inclui hashes, tokens, cabeçalhos de integração ou dados de autenticação. JSON inclui filtros, catálogo e metadados da consulta. CSV não inclui relatório de diagnóstico corrente.

## Atualização, compatibilidade e limites

Não há alteração de tabelas nem exclusão de registros. Campos novos são adicionados aos JSON de amostras futuras. APIs antigas /api/history e /api/events permanecem retornando arrays dos últimos 500 registros; portal usa endpoints paginados novos. APIs/clientes anteriores continuam aceitos. Histórico mantém cadência de 900 segundos e eventos imediatos.

Parar servidor e cliente, copiar bancos para backup, executar git pull --ff-only e reiniciar npm start com Node 24+. Reiniciar npm run client, se usado. Não apagar data/.

Consultas continuam usando JSON/SQLite sem índices temporais específicos; desempenho de grandes históricos, retenção e backup automatizado continuam em P1-07/P1-05/P1-06. Não declarar validação física ou equivalência de memória com ESP32.

## Verificações executadas

- npm test: **152 testes aprovados**, 131 anteriores e 21 novos.
- npm run check, git diff --check e conferência estática dos IDs HTML referenciados pelo JavaScript (sem ausências/duplicações).
- Testes: período antes do limite, fim exclusivo, UTC/offset e datas inválidas; sensor/conjunto/qualidade/tipo; contexto antigo/legado/HTTP; paginação/snapshot; exportação completa/limites; CSV real autenticado, UTF-8/escape de fórmula; gráfico com lacunas e eixo temporal; sensores removidos/compartilhados e isolamento por propriedade.
- Sem nova validação visual automatizada ou acionamento de hardware.

## Roteiro de validação de uso

1. Avançar simulador 15–30 minutos. Abrir Histórico e consultar: verificar umidade e temperatura, nomes e unidades nos gráficos/tabela.
2. Selecionar um sensor/conjunto e consultar; confirmar interseção. Injetar leitura inválida, gerar próxima amostra e conferir que não aparece como zero/ponto válido.
3. Selecionar período UTC que termina exatamente na coleta: essa coleta deve ficar fora da consulta. Limpar filtros para recuperar resultados.
4. Renomear conjunto/sensor, publicar/aplicar e gerar nova amostra; consultar ambas e confirmar nomes históricos preservados. Registros antigos sem contexto devem indicar isso.
5. Com mais de 200 registros, carregar páginas anteriores; nova coleta exige Consultar / atualizar para entrar na consulta fixa.
6. Exportar leituras e eventos em CSV/JSON; conferir filtros e quantidade total, além das páginas vistas. No caso de limite, reduzir período/sensores e repetir.
7. Como operador de outra propriedade, confirmar que não consulta/exporta GUID não autorizado.

Próxima etapa: P1-02, arquivamento/desativação e transferência de dispositivos, preservando GUID/histórico e tratando credenciais e isolamento por propriedade. Firmware continua após avaliação completa de hardware.
