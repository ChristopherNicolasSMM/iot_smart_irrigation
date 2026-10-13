# Critérios de aceitação — fase portal/simulação

Cada cenário automatizado registra configuração, seed, tempo inicial, ações e resultado. Não considerar testes existentes até implementação.

| ID | Cenário | Resultado esperado |
|---|---|---|
| AT-01 | Cadastrar 0 temperaturas, 0 umidades e 0 válvulas | Salvar; mostrar vazio válido |
| AT-02 | Cadastrar 4 temperaturas, 4 umidades e 4 válvulas; tentar quinto componente de cada categoria | Até quatro por categoria aceitos; quinto rejeitado por API e UI |
| AT-03 | Repetir component_id no mesmo GUID | Rejeitar; em outro GUID permitido |
| AT-04 | Renomear/mudar área e reconectar | GUID preservado e histórico mantém contexto |
| AT-05 | Estático sem umidade | Inicia/encerra na agenda |
| AT-06 | Estático com umidade acima do alvo | Executa mesmo assim |
| AT-07 | Data específica e avanço de uma semana | Executa só na data configurada |
| AT-08 | Segunda/quarta às 06:00 por 600 s | Executa só nas ocorrências previstas |
| AT-09 | Início/fim explícitos e virada de dia | Para no instante correto; exige crosses_midnight |
| AT-10 | Hora final e duração juntas, horários iguais ou duração inválida | Rejeitar com mensagem por campo |
| AT-11 | Dinâmico: 29%, início 30%, alvo 45% | Inicia; para em 45%; respeita histerese/cooldown |
| AT-12 | Dinâmico/híbrido sem sensor válido | Bloquear com motivo |
| AT-13 | Híbrido úmido/seco dentro/fora de janela | Omitir úmido; iniciar seco dentro; impedir fora |
| AT-14 | Híbrido não atinge alvo no fim | Parar e registrar alvo não atingido/fim de janela |
| AT-15 | Falha/obsolescência de sensor em dinâmica | Parar e registrar; valor inválido não vira zero |
| AT-16 | Teste local e remoto com umidade alta/sem sensor | Acionar por duração autorizada |
| AT-17 | Teste offline ou expirado | Não executar quando reconectar |
| AT-18 | Comando reenviado por perda de ACK | Uma execução; temporizador não reinicia |
| AT-19 | Parada repetida | Idempotente e prioritária |
| AT-20 | Solicitações concorrentes/saída ocupada | Rejeitar/omitir com motivo, sem acionamento oculto |
| AT-21 | Limite máximo com rede indisponível | Controlador local para no prazo |
| AT-22 | Reinício durante execução | Estado seguro; execução interrompida reconciliada |
| AT-23 | Avançar relógio virtual por um dia | Processar agendas/paradas intermediárias |
| AT-24 | Telemetria de 15 min e teste entre amostras | Amostras em 900 s e eventos imediatos |
| AT-25 | Agenda omitida por solo úmido | Histórico mostra não acionou + motivo |
| AT-26 | Mensagem duplicada/atrasada | Sem duplicação nem regressão de estado atual |
| AT-27 | Falha de persistência no servidor | Não emitir ACK de gravação |
| AT-28 | Integração com resposta inválida/fila cheia | Diagnóstico, repetição limitada e perda explícita |
| AT-29 | Operador tenta alterar credenciais/limites | API rejeita; ação auditada |
| AT-30 | Usuário de outra propriedade consulta/comanda GUID | Negar sem expor dados |
| AT-31 | Configuração inválida ou referência removida | Não aplicar parcialmente; preservar revisão válida |
| AT-32 | Configuração publicada sem ACK | Mostrar desejada diferente de aplicada |
| AT-33 | Erro de API no portal | Mostrar erro; nunca substituir por mock invisível |
| AT-34 | Relógio inválido/DST repetido | Aplicar política temporal e registrar decisão |
| AT-35 | Exportar configuração/ver logs | Não expor hashes, senhas ou tokens |
| AT-36 | Repetir cenário com seed/entradas iguais | Mesmo resultado |
| AT-37 | UI mostra saída elétrica ligada sem sensor de vazão | Não afirmar irrigação física confirmada |
| AT-38 | Remover componente com histórico | Arquivar/preservar dados e validar vínculos |
| AT-39 | Teste excede duração permitida | Rejeitar na API e controlador |
| AT-40 | Limite de umidade inicial >= alvo | Rejeitar configuração dinâmica/híbrida |

## Gate da fase 1

Build do portal/API, testes do controlador e contrato, testes de isolamento/autorização e demonstração dos fluxos principal/local. Documentar checks executados e limitações. Não exige ESP32 nessa fase.

## Gate futuro de hardware

Compilação para placa escolhida + verificação elétrica e física de boot, saídas, duração máxima, sensores, quedas de rede/energia e retomada. Simulador não substitui estes ensaios.
