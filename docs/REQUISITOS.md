# Requisitos funcionais e regras — v0.1

Data: 2026-10-13. Origem: requisitos desta conversa. “Confirmado” representa pedido explícito; “proposto” representa interpretação técnica inicial sujeita a revisão.

## 1. Escopo e limites

| ID | Requisito | Status |
|---|---|---|
| RF-01 | Cada ESP32 possui GUID estável e exclusivo, independente de IP/nome/área | Confirmado |
| RF-02 | Associar dispositivo a propriedade/área e definir sua função | Confirmado |
| RF-03 | Cadastrar 0–4 sensores de temperatura por dispositivo | Confirmado |
| RF-04 | Cadastrar 0–4 solenoides por dispositivo | Confirmado |
| RF-05 | Nome e identificador estável por sensor e atuador | Confirmado |
| RF-06 | Cadastrar 0–4 sensores de umidade por dispositivo e associá-los aos setores | Confirmado |
| RF-07 | Acionamento independente de umidade, automático ou manual | Confirmado |
| RF-08 | Modos dinâmico, estático e híbrido | Confirmado |
| RF-09 | Datas específicas ou dias da semana, horários e término por hora/duração | Confirmado |
| RF-10 | Configurar umidade esperada | Confirmado |
| RF-11 | Histórico de umidade em intervalos de 15 minutos | Confirmado |
| RF-12 | Registrar acionamentos, omissões justificadas e tentativas com falha | Confirmado |
| RF-13 | Testar acionamento no portal do ESP32 e no portal central | Confirmado |
| RF-14 | Administrador e mais uma conta com usuário/senha | Confirmado; perfil adicional adotado: operador |
| RF-15 | Configurar destino, requisição, corpo e confirmação de envio | Confirmado |
| RF-16 | Desenvolver portal e simulação antes do firmware | Confirmado |

Zero componentes é configuração válida. Com zero válvulas, o dispositivo serve para monitoramento. Sem sensores de umidade, estático/manual continuam disponíveis; dinâmico/híbrido exigem sensor válido associado ao setor. Temperatura é monitorada; participação em regras de controle fica fora do primeiro escopo.

## 2. Identidade e organização

- GUID no formato UUID, persistido no provisionamento. Reconexão, troca de Wi-Fi e renomeação não alteram identidade.
- GUID não é segredo nem autenticação. Credencial única de dispositivo é separada.
- Reset comum preserva identidade; reprovisionamento explícito deve revogar credencial antiga e tratar eventual colisão.
- Identificadores de componentes são únicos dentro do dispositivo; chave global é GUID + component_id.
- Nome é editável; identificador não é reutilizado para outro componente após arquivamento.
- Propriedade → áreas → dispositivos → setores → válvulas/sensores.
- Proposta inicial: um setor controla uma válvula; um dispositivo pode atender setores em áreas diferentes.
- Alterar área não reescreve o histórico: eventos guardam vínculo/contexto ocorrido na execução.
- Pinos e detalhes elétricos serão avaliados antes do firmware; cadastro lógico no simulador não comprova pinagem válida.

## 3. Modos de acionamento

| Modo | Quando inicia | Quando encerra | Dependência de umidade |
|---|---|---|---|
| Estático | Na ocorrência de data/dia e hora configurados | Hora final ou duração, respeitando limite de segurança | Nenhuma |
| Dinâmico | Abaixo do limite de início, após leitura válida e condições de segurança | Ao atingir alvo, limite máximo ou parada | Obrigatória |
| Híbrido | Dentro da janela permitida, se umidade estiver abaixo do limite | Alvo, término da janela, duração máxima ou parada | Obrigatória |
| Manual/teste | Comando autorizado local/remoto | Duração obrigatória, parada ou limite de segurança | Nenhuma |

### Estático

Exemplo: segunda/quarta/sexta, 06:00, por 10 minutos. Irriga mesmo que a umidade esteja acima do alvo. Umidade esperada pode ser registrada para comparar resultado, mas não muda a execução estática.

Suporta:
1. Data local específica + início + hora final.
2. Data local específica + início + duração.
3. Dias da semana + início + hora final.
4. Dias da semana + início + duração.

Hora final e duração são mutuamente exclusivas. Múltiplos horários usam múltiplas agendas. Não repetir a mesma ocorrência após reinício/reenvio. Proposta inicial: ocorrência perdida enquanto desligado é registrada como perdida quando identificável e não executada tardiamente.

### Dinâmico

Exemplo: iniciar abaixo de 30% relativos, parar ao atingir 45%. Exigir limite inicial menor que alvo, histerese e intervalo mínimo entre ciclos. Limite máximo de execução prevalece se o alvo não for atingido. Ler frequentemente durante a operação; não esperar a publicação de 15 minutos.

### Híbrido

Interpretação proposta: dinâmica restrita a janela(s) de data/dias e horários. Exemplo: segunda a sexta entre 06:00 e 06:30, iniciar abaixo de 30%, alvo 45%, cada ciclo limitado a 10 minutos.

- Avaliar na abertura da janela e nas leituras válidas dentro dela.
- Se já está no alvo, registrar ocorrência como não acionada por umidade suficiente.
- Se cair abaixo do início depois, pode acionar dentro da janela.
- Parar ao atingir alvo, ao fim da janela ou ao limite de duração — o que acontecer primeiro.
- Se não atingiu o alvo, registrar motivo e resultado observado.
- No primeiro piloto, no máximo um ciclo por janela/ocorrência (proposta para evitar repetições excessivas). Reciclos configuráveis ficam pendentes.

Agenda híbrida por duração define a extensão da janela, não garante que a válvula permaneça ligada durante todo o período. O limite por ciclo é separado.

## 4. Tempo, prioridades e conflitos

- Fuso configurável por propriedade/dispositivo, padrão inicial America/Sao_Paulo.
- API armazena instantes UTC; agenda mantém calendário local e fuso IANA.
- Semana padronizada ISO: segunda=1 até domingo=7.
- Fim exclusivo da janela. Intervalo 23:50–00:10 exige indicação explícita de dia seguinte; não inferir janela de 24 horas por horários iguais.
- Proposta inicial para mudanças de fuso/DST: horário inexistente é omitido com motivo; horário repetido executa uma única vez.
- Relógio não confiável bloqueia novas agendas e comandos remotos cuja validade não possa ser verificada. Parada e limite de duração usam relógio monotônico.
- Prioridade: parada/proteção > parada autorizada > comando manual/teste > automático.
- Proposta inicial: uma válvula simultânea por dispositivo; limite configurável dentro de 1–4 após validar capacidade hidráulica.
- Não mesclar solicitações sobre a mesma válvula. Conflito é rejeitado/omitido com motivo e ID da ocorrência.
- Teste exige setor ocioso e não interrompe automaticamente irrigação em curso.
- Ao terminar manual/teste, reavaliar automação em próxima avaliação; nunca prolongar automaticamente a duração solicitada.
- Configuração alterada durante operação não muda silenciosamente a execução já iniciada. Aplicar nova revisão após encerramento, exceto proteções mais restritivas/parada.

## 5. Sensores

Cadastro: component_id, nome, tipo, unidade, calibração, habilitado, área/setor e limite de idade da leitura.

- Leituras: valor, instante, qualidade (válida, inválida, ausente, antiga) e tipo de umidade.
- Umidade relativa calibrada e umidade volumétrica são conceitos diferentes; mostrar unidade/tipo corretamente.
- Sem leitura válida/recente, não iniciar dinâmico/híbrido; se falhar durante ciclo, parar e registrar.
- Estático/manual independem da leitura de umidade, mantendo todas as proteções elétricas e de duração.
- Proposta inicial: um sensor controlador por setor; demais são monitoramento. Agregação/média/quórum pendentes.
- Sensor removido não apaga leituras antigas; configuração com referência inexistente deve ser rejeitada.

## 6. Histórico, eventos e falhas

- Persistir/publicar amostra de umidade a cada 900 segundos, com temperatura e estado das saídas quando disponíveis.
- Portal mostra última leitura e seu horário; não substitui ausência por zero.
- Leituras internas para controle têm intervalo menor configurável; proposta: 10 segundos, sujeito a sensor/hardware.
- Eventos de início/fim/falha/comando são imediatos e independentes dos 900 segundos.
- Registrar decisões por ocorrência/transição, sem gerar logs a cada avaliação sem alteração.
- Categorias: solicitado, rejeitado, recebido, iniciado, concluído, interrompido, falhou, expirado, omitido, offline.
- Motivos: umidade suficiente, sensor ausente/antigo/inválido, agenda perdida, limite de simultaneidade, conflito, dispositivo offline, timeout, credencial inválida, configuração inválida e parada de segurança.
- Uma ocorrência omitida deve constar no histórico mesmo quando não houve acionamento.
- Cada registro inclui IDs, origem, ator quando humano, configuração vigente, horários, motivo e medições relevantes.
- Estado da saída elétrica não comprova abertura física/vazão. Exibir “saída acionada”; confirmação hidráulica exige sensor adicional.
- Queda de energia pode impedir log instantâneo: após retorno, reconciliar sessão interrompida. Sem confirmação, mostrar estado desconhecido.

## 7. Acionamento de teste

Local e web têm selecionar válvula, duração, iniciar e parar.
Proposta: padrão 5 segundos, limite de teste 30 segundos; nunca maior que limite de segurança do setor.
Modo estático/manual/teste não exige umidade. Todo teste exige autorização, saída habilitada, ausência de conflito e comando idempotente.
No portal web, não apresentar sucesso antes do retorno do dispositivo; distinguir solicitado, recebido, saída acionada e finalizado.
Dispositivo offline não recebe teste pendente para executar futuramente: rejeitar ou expirar.
Parar é idempotente e tem prioridade, inclusive quando não há execução ativa.

## 8. Acesso e integrações

- Administrador: usuários, componentes, calibração, agendas, modos, credenciais e integrações.
- Operador: monitorar, iniciar teste/manual e parar; sem edição de segurança/credenciais.
- Conta humana não é usada como identidade do ESP32.
- Configuração nunca devolve senha, hash ou token bruto. Exportação comum remove segredos.
- Requisição configurável: HTTPS, método, cabeçalhos, mapeamento JSON, intervalo, timeout e limites.
- Confirmação: códigos permitidos + campo JSON simples + identificação da mensagem aceita.
- Repetir somente com IDs preservados; servidor deduplica.
- Templates não executam código arbitrário. Limites de endpoints/payload/resposta/fila serão medidos antes do firmware.
- Enviar para bancos online preferencialmente via API; credencial administrativa do banco não reside no dispositivo.
- Fila limitada com política de descarte e indicação de perda; não prometer histórico infinito offline.

## 9. Requisitos não funcionais

Portal: responsivo, acessível, português brasileiro, estados vazios e erros reais, persistência e isolamento por propriedade.
Simulador: determinístico, tempo virtual, falhas reproduzíveis e marcação explícita.
Firmware futuro: C++, controle não bloqueante, watchdog, TLS validado, armazenamento atômico, estado seguro no boot, memória/fila limitadas.
Operação local deve continuar com configuração válida quando a nuvem cair. Offline não remove limite de duração nem exige autorização remota para parar.

## Adendo confirmado em 13/10/2026

Seleção semanal com checkboxes nomeados de segunda a domingo. Cada dia marcado permite horários próprios, com hora final ou duração, e múltiplos horários naquele dia. A v0.4 implementa day_times; agendas antigas continuam aceitas. Propriedades/áreas e vínculos humanos são implementados no portal; ver VALIDACAO_V04.md.

## Conjuntos por solenoide — adendo v0.5

Agrupar solenoide, sensor de umidade e temperatura como conjunto nomeado/identificado, com área, modo e agenda próprios. Uma solenoide por conjunto, até quatro conjuntos; sensores opcionais preservam os limites de 0–4 por tipo. Campos zones/zone_id permanecem compatíveis; sensor_id é umidade e temperature_sensor_id é temperatura. Temperatura não altera acionamento nesta fase. Status e pendências em BACKLOG.md.

## Agenda e acesso — adendo v0.6

Avisos antecipados de sobreposição por conjunto e disputa de simultaneidade são informativos e não substituem proteções durante execução. Escopo temporal e limites em VALIDACAO_V06.md. Senha própria exige senha atual; administrador do sistema gerencia senhas de outros usuários, estado global da conta e vínculos por propriedade. Revogação encerra sessões abertas e permanece após reinício. Não implica parada automática de dispositivo.

## Diagnóstico — adendo v0.7

Portal deve distinguir idade/qualidade/fonte de leitura e condições conhecidas/desconhecidas do controle local. Previsão por conjunto usa revisão aplicada: estático prevê agenda, híbrido janela condicional e dinâmico não tem horário fixo. Relatório corrente do cliente HTTP é separado das amostras históricas de 15 minutos. Comunicação recente não torna atual uma leitura ou saída antiga. Escopo temporal e compatibilidade em VALIDACAO_V07.md.

## Histórico — adendo v0.8

Consulta por período UTC (início inclusivo/fim exclusivo), sensor, conjunto, qualidade e evento; amostras conservam contexto da revisão aplicada quando comprovado. Gráficos individuais de umidade/temperatura não convertem ausência em zero nem ligam grandes lacunas. Consulta paginada e fixa, exportação CSV/JSON dos resultados filtrados com limite explícito. Contexto legado desconhecido permanece indicado; API antiga preservada. Ver VALIDACAO_V08.md.

## Ciclo de vida — adendo v0.9

Arquivar preserva GUID/histórico, bloqueia operação e revoga credencial; exige ociosidade e, no HTTP, configuração aplicada com acionamentos desabilitados e runtime recente. Restaurar mantém agendas/solenoides desabilitadas. Transferência exige administrador do sistema com acesso administrativo nas duas propriedades, mantém histórico anterior na origem e limpa segredos/caches atuais. Destino chega arquivado para novo provisionamento. Registro histórico tem propriedade imutável, independente da propriedade atual do GUID. Ver VALIDACAO_V09.md.

## Gestão da fila — adendo v0.10

Administrador pode cancelar mensagem pending e reprocessar dead/cancelled com conteúdo disponível do vínculo atual, preservando ID/payload e total de tentativas. Atualizar autenticação exige opção explícita e mesmo destino. Retenção manual usa prévia vinculada/expirável, conserva IDs/auditoria e não remove pendências ou histórico de irrigação. Receptor precisa deduplicar IDs; não prometer exatamente uma entrega sem esse contrato. Escopo/migração em VALIDACAO_V010.md.

## Recuperação — adendo v0.11

Backup do portal cifrado com senha externa e recuperação exclusiva em banco novo, preservando identidade/histórico e colocando dispositivos em quarentena. Sem retomada automática de credenciais, comandos ou envios. Não equivale a parada física; banco ativo ainda tem segredos em claro. P1-06 parcial e procedimentos em VALIDACAO_V011.md.

## Consolidação — adendo v0.12

Entrega 1–3 aprovada: proteção de estado/segredos da fila por chave externa com migração/rotação em novo banco; backup do portal/cliente e recuperação bloqueada até configuração desabilitada confirmada no cliente; estabilidade/desempenho medidos no piloto; telemetria recebida pode usar perfil externo por GUID, sem dependência do canal de controle. ACK local não confirma destino externo. Próxima entrega reúne contrato, testes temporais raros e validação de uso (etapas 4–6); hardware/firmware após avaliação conjunta. Ver OPERACAO_SEGURA.md e VALIDACAO_V012.md.

## Contrato, tempo e conferência — adendo v0.13

Etapas 4–6 entregues em software: schemas/OpenAPI e fixtures do protocolo HTTP v1, validação/hash compartilhados, cenários temporais raros e recuperação de datas específicas antigas sem execução tardia, conferência do portal em navegador desktop/móvel e roteiro de validação do usuário. Recuperação semanal continua delimitada com resumo explícito; não inferir meses de ocorrências ou revisão antiga sem prova. Firmware/hardware continuam posteriores à revisão e aprovação HW-01/HW-02. Evidências e limites em VALIDACAO_V013.md.

## Firmware experimental — adendo 14/10/2026

Usuário autorizou iniciar firmware antes do fechamento físico. Base HD-38/DS18B20, sensores opcionais por conjunto, matriz com energia e dados até pontos remotos de 100 m. Firmware 0.2 implementa primeiras rotinas com limites explícitos, sem substituir requisitos completos do piloto. Ver iot/docs/VALIDACAO_FIRMWARE_V02.md para divergências/pendências; hardware e fonte continuam em avaliação.
