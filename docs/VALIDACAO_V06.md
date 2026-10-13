# Entrega v0.6.0 — avisos de agenda e gestão de acesso

13/10/2026. Usuário confirmou validação da v0.5 nesta conversa. As telas novas desta versão aguardam validação de uso.

## O que mudou

- Botão **Verificar agendas** analisa o rascunho antes da publicação, sem criar revisão ou alterar dispositivos.
- Detecta janelas sobrepostas na mesma solenoide/conjunto e conjuntos que podem ultrapassar a simultaneidade configurada. Aceita agendas semanais antigas, horários próprios por dia, datas específicas e passagem da meia-noite. Fim exclusivo: 06:00–06:10 e 06:10–06:20 não conflitam.
- Publicação também retorna os avisos. Eles são informativos, não impedem publicar. Proteções de execução continuam aplicando conflito, simultaneidade e duração máxima.
- **Minha senha** permite alteração mediante senha atual. Administrador do sistema pode redefinir senha de outros usuários, desativar/reativar contas e revogar vínculo com a propriedade selecionada.
- Ações de conta/vínculo bem-sucedidas são registradas em account_audit, com ator, alvo, instante UTC real, ação e propriedade quando aplicável; sem senha/hash/token.

## Escopo da análise

Semanais: data local do relógio do laboratório até sete dias depois, incluindo o dia anterior para identificar intervalos que atravessam meia-noite. Datas específicas: todas as configuradas, incluindo vizinhança para comparar com recorrências semanais. Usa o fuso do dispositivo e o mesmo resolvedor de tempo do controlador.

Conta conjuntos distintos para simultaneidade. Exclui agendas desabilitadas, saídas desabilitadas e agendas ignoradas no modo dinâmico. No híbrido, sobreposição de janelas indica possibilidade, não certeza de acionamento: depende de umidade, pausa e limite por ciclo. Dinâmicos concorrentes geram aviso condicional de capacidade. Janelas não são reduzidas pelo limite por ciclo, portanto um aviso pode ser conservador.

Até 50 avisos exibidos, com truncated=true quando houver mais. Datas/horários de exemplo são locais. Ausência de aviso não certifica todo o futuro: outras transições de fuso, comandos manuais, capacidade entre dispositivos e dimensionamento hidráulico não são cobertos. Vazão, pressão e bomba continuam no backlog de hardware.

## Acesso e migração

Migração automática adiciona users.enabled (padrão 1), users.auth_version (padrão 0) e account_audit. Preserva hashes existentes, GUIDs, configurações e históricos. Não apagar banco para atualizar. Fazer backup com servidor parado antes de atualizar; o fluxo automatizado de backup/restauração continua pendente em P1-06.

Cada requisição humana verifica conta ativa e versão da sessão. Alteração/redefinição de senha, desativação, reativação e revogação de vínculo incrementam essa versão. Sessões antigas passam a receber 401, inclusive nas outras propriedades; usuário pode entrar novamente se conta ativa e acessar somente vínculos restantes. Alterar perfil mantém sessão e aplica novas permissões na próxima requisição. Usuário sem vínculos não acessa dados de propriedades, mas pode trocar a própria senha após login.

Desativação é global; revogação afeta apenas a propriedade selecionada. Reativar conta não restaura vínculo removido nem sessões antigas. Vincular novamente usa o botão existente. Administrador do sistema não pode se desativar/remover o próprio vínculo nem usar redefinição administrativa para sua senha; deve informar senha atual em Minha senha.

Correção importante: vínculos iniciais admin/operador são criados apenas na instalação/migração de banco sem memberships. Não são recriados em cada reinício. Credenciais humanas e de dispositivos são separadas: encerrar acesso humano não interrompe ciclos locais nem revoga token do ESP32/cliente.

## Verificações executadas

- npm run check: sintaxe dos módulos do servidor, cliente e navegador.
- npm test: 112 testes aprovados, sem falhas; 18 novos sobre contas, análise de agendas e APIs, além das 94 verificações anteriores.
- Conferência estática de IDs do HTML referenciados pelo JavaScript e git diff --check.
- Não houve nova validação visual automatizada ou teste físico de ESP32. Validação prévia do usuário não cobre automaticamente os controles novos.

## Roteiro de validação

1. Atualizar com git pull --ff-only e reiniciar npm start usando Node 24+ e o banco existente.
2. No editor, criar duas agendas da mesma solenoide: segunda 06:00 por 10 min e segunda 06:05 por 10 min. Verificar agendas: aviso. Mudar segunda agenda para 06:10: aviso desaparece.
3. Usar horários próprios por checkbox e janela domingo 23:55 por 20 min com segunda 00:05. Conferir aviso e referência ao conjunto correto.
4. Em dois conjuntos, configurar mesmo horário e simultaneidade 1: aviso de capacidade. Com limite 2, remover o aviso de capacidade, respeitando capacidade física futura.
5. Entrar como operador em outro navegador. Como admin, redefinir sua senha: sessão anterior deve retornar ao login. Testar nova senha.
6. Desativar/reativar operador; sessão antiga não volta. Revogar vínculo, reiniciar portal e confirmar que vínculo permanece removido. Para restaurar, vincular novamente.
7. Testar Minha senha e confirmar que exige senha atual, confirmação igual e novo login.

Próxima etapa: diagnóstico de comunicação/leituras e próxima execução por conjunto (P1-03), seguido de filtros/exportação do histórico (P1-04). Arquivamento/transferência de dispositivos, filas, backup, estabilidade e contrato definitivo permanecem no BACKLOG.md. Firmware permanece após avaliação completa de hardware.
