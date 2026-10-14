# Entrega v0.7.0 — diagnóstico e próxima agenda por conjunto

13/10/2026. Implementa P1-03 em software. Controles da v0.6/v0.7 aguardam validação de uso; a última confirmação explícita de validação nesta conversa refere-se à v0.5.

## Portal

Visão geral mostra última comunicação, idade, fonte e qualidade das leituras, estado conhecido/desconhecido do controlador e qualidade do relógio. Cada conjunto apresenta saída observada, motivos de espera, pausa restante e próxima agenda da revisão aplicada. A revisão desejada aparece separada para evitar prever regras ainda não aplicadas.

- Estático: próxima abertura de agenda; umidade não bloqueia nem determina a previsão.
- Híbrido: próxima janela permitida; pode mostrar janela já aberta se ainda não consumida. Não promete execução, que depende de umidade e proteções. Ocorrência já consumida não é anunciada como oportunidade nova.
- Dinâmico: depende da leitura, sem inventar horário fixo.
- Temperatura é monitoramento; não bloqueia irrigação.
- Motivos atuais incluem saída em execução/desabilitada, capacidade ocupada, energia, relógio, sensor ausente/inválido/antigo/desabilitado, umidade acima do início, pausa, espera de agenda e ausência de estado confiável.

## Tempo e previsão

Simulador interno usa calendário virtual para agenda e contador monotônico para idade/pausa. Perder comunicação com a nuvem não bloqueia automação local. Última comunicação simulada é atualizada nos ticks com energia/conexão e não comprova conexão de hardware.

HTTP usa UTC real do servidor para envelhecer comunicação/relatórios. Previsão usa calendário informado pelo controlador quando diagnóstico atual está disponível, acrescido do tempo decorrido desde recebimento; sem relatório atual usa UTC do servidor como previsão indicativa. Diferença de relógio aparece em segundos; não é ajuste automático ou confirmação de sincronização NTP.

Semanais são buscadas até 14 dias após a data local, incluindo dia anterior para janelas que atravessam meia-noite. Datas específicas futuras são examinadas mesmo além desse horizonte. Horários inexistentes em DST são omitidos pelo resolvedor compartilhado; repetidos usam a primeira ocorrência. Transições raras continuam em P1-10. Previsão usa revisão aplicada, considera fim exclusivo e ocorrências consumidas conhecidas. Conflitos futuros/capacidade no instante futuro não são simulados nesta previsão: avisos prévios continuam em Verificar agendas.

Próxima agenda não é autorização de acionamento nem promessa de duração. Motivos descrevem condições atuais e podem mudar até a abertura. Saída elétrica observada não comprova vazão ou abertura física da válvula.

## Cliente HTTP e compatibilidade

O cliente independente passa a enviar controller_status opcional em POST /iot/v1/runtime: revisão, relógio/tempo monotônico, sensores atuais, últimas pausas dos conjuntos vigentes e IDs de janelas abertas já consumidas. Limites do piloto: 8 sensores, 4 conjuntos, 512 IDs de ocorrência até 300 caracteres e corpo total de 128 KB. Não transportar esses limites ao ESP32 sem medir memória (HW-02).

Servidor valida o relatório e persiste apenas estado de diagnóstico, sem criar amostra histórica/evento de acionamento por atualização. Histórico mantém a cadência de 900 segundos; eventos de controle continuam independentes. Telemetria autenticada de dispositivo HTTP também atualiza última comunicação. Dados retransmitidos de fila mantêm a idade original da leitura.

Relatório considerado atual: revisão corresponde à aplicada, conexão disponível e recebimento há no máximo 90 segundos. Idade das leituras acrescenta tempo decorrido desde recebimento. Limite de atualidade de leitura no controle é 60 segundos, igual ao controlador do piloto; qualidade informada e desabilitação são preservadas. Leitura antiga/inválida não é exibida como atual válida; último valor fica identificado separadamente. Horário futuro/idade negativa é sinalizado.

Clientes antigos continuam podendo enviar runtime sem controller_status. Nesse caso, saída relatada em runtime recente pode ser conhecida, mas condições de controle são desconhecidas. A telemetria antiga é mostrada como cache, sem afirmar que o controlador local está bloqueado por ela. Um heartbeat recente sozinho não renova a atualidade de diagnóstico/sensores/saídas. Cliente offline com saída antiga no cache mostra estado sem confirmação.

## API e persistência

GET /api/diagnostics?guid=UUID retorna diagnóstico do dispositivo autorizado para administrador ou operador vinculado. GET /api/state acrescenta diagnostics em cada dispositivo permitido. Campos documentados em CONTRATO_API.md. Nenhum endpoint humano ignora isolamento por propriedade.

Não há alteração de tabelas nesta versão: campos opcionais são persistidos no JSON de estado. GUIDs, agendas, usuários, históricos e credenciais são preservados. Bancos antigos mostram última comunicação simulada desconhecida até o primeiro tick conectado. Troca de transporte limpa relatório de controlador anterior.

## Verificações

- npm test: 131 testes aprovados (112 anteriores e 19 novos).
- npm run check, conferência estática de IDs HTML/JavaScript e git diff --check.
- Testes cobrem revisão aplicada, estático sem umidade válida, híbrido consumido, dinâmico/pausa, idade monotônica, energia/relógio, offline, relatório HTTP antigo/novo, meia-noite, horários por dia, datas distantes, DST, validação/rollback, runtime legado, cliente independente, cadência histórica e isolamento por propriedade.
- Sem nova validação visual automatizada ou execução em ESP32 físico.

## Atualizar e validar

1. Parar servidor e cliente, copiar os bancos para backup, git pull --ff-only e npm start com Node 24+. Não apagar data/. Reiniciar também npm run client para receber o diagnóstico novo.
2. Visão geral: conferir idade, fonte das leituras e próxima agenda por conjunto. Editar/publicar horário com simulador pausado: previsão deve manter revisão aplicada até avançar relógio e aplicar.
3. Estático: injetar umidade inválida e confirmar que ela não aparece como bloqueio desse modo. Dinâmico/híbrido: qualidade inválida deve aparecer como motivo de espera; restaurar leitura válida.
4. Híbrido: manter umidade alta durante janela para ver condição; executar um ciclo e conferir que janela consumida não aparece como oportunidade nova.
5. Desligar energia ou invalidar relógio no simulador e conferir motivos. Alternar apenas conexão permite controle local.
6. Com cliente HTTP novo, observar controlador conhecido e leituras atuais. Parar cliente: após 90 segundos, confirmar estado desconhecido e saída sem confirmação. Relógio acelerado do laboratório não acelera esse prazo.
7. Confirmar que operadores consultam somente GUIDs das propriedades autorizadas.

Próxima entrega: P1-04, histórico com filtros por período/sensor/conjunto, todos os sensores e exportação. Arquivamento/transferência, retenção, backup e validação física continuam no BACKLOG.md.
