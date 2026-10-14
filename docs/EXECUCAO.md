# Executar o piloto v0.2.0

## Requisitos

Node.js **24 ou superior** e Git. Sem pacotes externos; SQLite é integrado ao Node.

```bash
git clone https://github.com/ChristopherNicolasSMM/iot_smart_irrigation.git
cd iot_smart_irrigation
npm start
```

Portal central: http://127.0.0.1:8080
Portal local simulado: http://127.0.0.1:8080/local

O servidor escuta somente em 127.0.0.1 por padrão. PORT altera a porta; HOST altera interface. O piloto é local e não constitui implantação pública de produção.

## Contas iniciais de demonstração

| Usuário | Senha inicial | Perfil |
|---|---|---|
| admin | admin-demo-2026 | Configura e controla |
| operador | operador-demo-2026 | Monitora e controla |

Para escolher senhas, definir ADMIN_PASSWORD e OPERATOR_PASSWORD **antes da primeira execução**. As senhas são armazenadas com salt e scrypt, não em texto puro. As variáveis só inicializam contas inexistentes; não trocam uma senha já gravada. Gestão de contas/redefinição via UI fica para próxima entrega.

PowerShell, antes da primeira execução:

```powershell
$env:ADMIN_PASSWORD="sua-senha-admin"
$env:OPERATOR_PASSWORD="sua-senha-operador"
npm start
```

## Demonstração

1. Entrar como admin. Dispositivo Horta possui um sensor de umidade, um de temperatura, uma solenoide e setor híbrido.
2. Relógio inicia em segunda-feira, 12/10/2026, 05:59:50 (São Paulo), pausado.
3. Avançar 10 segundos: janela 06:00 abre e saída é acionada com solo inicialmente seco.
4. Avançar 15 minutos: controlador processa segundos intermediários, encerra ciclo e grava amostra.
5. Abrir Histórico: conferir leituras, início, conclusão e motivos.
6. Em Configuração, mudar setor para Estático, salvar com saída parada, configurar agenda e duração.
7. Em Acionamento, escolher Teste e duração 5 s. Solicitar; avançar 10 s para encerramento. Parada também é disponível.
8. Em Injeção, escolher sensor, valor/qualidade. Testar solo úmido, ausente ou inválido.
9. Alternar rede offline: teste web é rejeitado; portal local /local continua permitindo teste local.
10. Cadastrar novo dispositivo vazio. Acrescentar componentes, setor e agendas. Até quatro sensores de cada tipo e quatro válvulas por GUID.
11. Em Integrações, salvar URL/modelo e gerar prévia. Nenhum envio externo é realizado.

O relógio é global e único para este laboratório. Velocidades 1×, 10× e 60× são segundos simulados por segundo real. Avanço máximo por solicitação: um dia.

## Dados

Banco local data/pilot.sqlite. Reiniciar aplicação conserva GUIDs, configurações, histórico e comandos; saídas ativas são encerradas e reconciliadas na inicialização. Sessões expiram em 8 h e são invalidadas ao reiniciar servidor.

Para uma demonstração nova, parar a aplicação e mover a pasta data para um backup. Isso cria um banco novo na próxima execução; não realizar se houver dados a preservar.

## Verificação

```bash
npm run check
npm test
```

Testes usam bancos em memória/temporários, sem alterar data/pilot.sqlite. Ver relatório de validação em VALIDACAO_V02.md.

## Limites da entrega

- Um único laboratório/propriedade compartilhado entre as duas contas. Multiempresa/RLS não implementados.
- API de demonstração /api/*, não o contrato futuro /api/v1 e /iot/v1.
- Portal local e central usam o mesmo controlador/banco; rede e ACK são simulados, sem transporte real ESP32.
- Revisão aplicada é síncrona no simulador. Não há revisão desejada pendente de entrega.
- Histórico local também registra períodos de nuvem offline; não reproduz fila de transferência remota.
- Sem MQTT, OTA, credenciais de máquina, envio externo, headers secretos ou motor de confirmação HTTP real.
- Retenção/limitação dos journals e IDs processados precisa ser definida antes de operação contínua.
- Hora de agenda usa fuso IANA; política detalhada para transições DST e agenda perdida após desligamento fica pendente.
- Falha elétrica/hidráulica não é modelada. “Saída acionada” não confirma água fluindo.
- Sensores geram modelo sintético determinístico, não agronômico; temperatura é constante até alteração manual.
- Nenhum firmware foi criado nesta fase.

## Atualização v0.3.0

Parar o processo com Ctrl+C. Preservar data/pilot.sqlite (backup recomendado antes de atualizar).

```bash
git pull --ff-only
npm run check
npm test
npm start
```

Migração automática, sem comandos SQL manuais e sem apagar data. Consulte VALIDACAO_V03.md. As limitações descritas acima referem-se à primeira entrega v0.2; envio e revisões agora seguem os fluxos abaixo.

### Aplicar configuração

Publicar não altera imediatamente as saídas. Confira indicador aplicada/desejada. Se offline, ligar conexão; se houver ciclo, parar ou aguardar encerramento. Avançar +10 segundos ou continuar relógio. Estado muda de pendente para aplicada/rejeitada; rejeição preserva revisão anterior.

### Envio real para servidor HTTPS

1. Selecionar GUID e abrir Integrações.
2. Preencher URL, método, modelo, códigos HTTP aceitos e regra de ACK. Perfil é individual por GUID.
3. Cabeçalhos opcionais em JSON, como Authorization. Campo em branco preserva os já configurados; enviar {} limpa os cabeçalhos.
4. Gerar prévia, salvar perfil e clicar Enviar teste real.
5. Observar fila: aguardando/enviando/confirmada/falha definitiva; histórico registra diagnóstico.
6. Habilitar envio automático para amostras de 15 minutos e eventos imediatos.

Modelos devem ter message_id: "{{message.message_id}}". Variável completa pode representar uma lista ou número; não é execução de script. Se exigir ACK, resposta deve conter true no campo configurado e o ID da mensagem no caminho configurado. Exemplo padrão:

```json
{"accepted":true,"message_id":"ID recebido","received_at":"2026-10-13T12:00:00Z"}
```

Sem ACK obrigatório, confirme que o servidor realmente persiste dados: sucesso HTTP sozinho não comprova gravação.

### Teste de ponta a ponta usando receptor local

Reiniciar com opção de destinos locais. PowerShell:

```powershell
$env:ALLOW_LOCAL_INTEGRATIONS="1"
npm start
```

Linux/macOS:

```bash
ALLOW_LOCAL_INTEGRATIONS=1 npm start
```

1. Em Integrações, selecionar GUID e Gerar / rotacionar token. Copiar o token exibido uma vez.
2. Clicar Modelo do receptor local; URL recebe http://127.0.0.1:8080/iot/v1/telemetry (ou origem em uso).
3. Informar cabeçalho JSON: {"Authorization":"Bearer TOKEN_COPIADO"}.
4. Salvar, depois Enviar teste real. A fila deve passar para Confirmada em poucos segundos reais, mesmo com relógio virtual pausado.
5. Avançar 15 minutos com automático habilitado para enfileirar amostras e eventos.
6. Simular offline: fila aguarda; reconectar retoma os envios, preservando IDs.
7. Para testar falha de autenticação, rotacionar token sem atualizar header antigo; observar falha 401. Atualizar header e solicitar novo teste.

ALLOW_LOCAL_INTEGRATIONS permite HTTP e destinos privados para laboratório/LAN. Padrão continua HTTPS público; redirecionamentos não são seguidos. DNS é resolvido e endereço validado/fixado para a requisição.

O intervalo de repetição usa relógio real, independente de avanço virtual. Retentativas não alteram message_id. Perfil do job é congelado no enqueue; alterar URL/headers não modifica jobs antigos.

DB_PATH permite escolher outro banco, sem alterar o existente. Não operar dois processos sobre o mesmo laboratório simultaneamente.

## Atualização v0.4.0 — propriedades e horários por dia

Mesmo processo de atualização: parar, fazer backup de data, git pull --ff-only, npm test e npm start. A migração preserva contas, GUIDs e agendas existentes.

### Dias da semana

1. Abrir Dispositivos e regras → Agendas.
2. Escolher Dias da semana.
3. Marcar Segunda-feira, Terça-feira etc. como checkboxes.
4. Em cada dia marcado, configurar início e término por duração ou hora final.
5. Adicionar horário neste dia permite uma segunda janela, por exemplo 06:00 e 18:00.
6. Desmarcar um dia impede agendamento nele.
7. Publicar e avançar relógio/aguardar dispositivo conectado e ocioso para aplicar.

Exemplo: segunda 06:00 por 600 s, terça 16:30 até 16:40, sexta 07:00 por 600 s. No híbrido, esses horários delimitam janelas de avaliação por umidade, sem garantir abertura se o solo estiver suficientemente úmido.

### Propriedades e áreas

1. Como admin, abrir Propriedades e áreas.
2. Preencher nome/fuso e Cadastrar nova propriedade; selecioná-la no filtro superior.
3. Cadastrar áreas e dispositivos. No editor de dispositivo, escolher área no seletor.
4. Área pertence à propriedade escolhida; associação a área externa é rejeitada pela API.
5. Atualizar área selecionada preserva ID e histórico.
6. Fuso da propriedade sugere o fuso para novos dispositivos; alterar propriedade não altera silenciosamente agendas já configuradas em dispositivos existentes.

### Usuários e vínculos

A conta admin original administra o sistema. Na propriedade selecionada:

- Criar usuário e vincular: nome, senha de pelo menos 10 caracteres, perfil administrador da propriedade ou operador.
- Vincular usuário existente / alterar perfil: concede acesso à propriedade selecionada ou atualiza perfil.
- Um usuário pode ter propriedades/perfis diferentes. O filtro mostra somente seus vínculos.
- Usuário administrador de propriedade configura seus dispositivos, mas não administra contas globais nem o relógio global do laboratório.
- Operador não pode publicar configurações, gerar tokens ou alterar integração.

Teste recomendado: criar propriedade B e usuário operador B. Entrar em janela separada com esse usuário; confirmar que dispositivos da propriedade principal não aparecem e seus GUIDs não podem ser consultados/acionados.

Gerenciamento de usuários requer conta admin do sistema. As duas contas iniciais continuam válidas na propriedade principal.

## Atualização v0.5.0 — conjuntos e cliente independente

Parar servidor, fazer backup de data, git pull --ff-only, npm test e npm start. A migração é automática. Consulte BACKLOG.md na raiz e VALIDACAO_V05.md.

### Conjunto por solenoide

No editor Conjuntos de irrigação e modos, cadastrar nome/ID, solenoide, umidade opcional, temperatura opcional e área. Escolher modo e limites. Na agenda, selecionar o conjunto/solenoide correspondente. Outra solenoide usa outro conjunto e pode ter dias/horários diferentes.

Conjuntos antigos mantêm IDs. Selecionar temperatura explicitamente quando antes não havia associação. Para remover um sensor, escolher Sem sensor nos conjuntos associados antes de publicar (dinâmico/híbrido precisam de umidade).

### Tempo no simulador interno

Como admin do sistema, controles do laboratório permitem ajuste de calendário UTC, confiabilidade do relógio e energia do dispositivo. Ajuste até sete dias. Duração é monotônica: adiantar/atrasar hora não estende manual/teste. Automação em curso é interrompida no ajuste; agendas puladas são omitidas com motivo.

Para energia: desligar antes do horário, avançar tempo e ligar. Agenda perdida deve constar no histórico sem acionamento tardio. Dias com horários inexistentes/repetidos seguem a política do relatório; transições raras continuam no backlog.

### Cliente HTTP separado

1. Deixar todas as saídas paradas. No editor, escolher Cliente HTTP independente e clicar Trocar controlador.
2. Gerar/rotacionar token individual em Integrações. Copiar GUID e token.
3. Em outro terminal, iniciar cliente:

```powershell
$env:DEVICE_URL="http://127.0.0.1:8080"
$env:DEVICE_GUID="GUID_DO_DISPOSITIVO"
$env:DEVICE_TOKEN="TOKEN_COPIADO"
npm run client
```

Linux/macOS:

```bash
DEVICE_URL="http://127.0.0.1:8080" DEVICE_GUID="GUID_DO_DISPOSITIVO" DEVICE_TOKEN="TOKEN_COPIADO" npm run client
```

4. O cliente consulta configuração, confirma hash/revisão e comunica estado. Aguardar conectado/aplicado no portal.
5. Solicitar teste de 5 s. Estado começa solicitado, depois recebido/iniciado e concluído mediante confirmação do cliente.
6. Relógio desse cliente usa segundos reais; não clicar +15 minutos esperando acelerar sua execução.
7. Publicar nova configuração: cliente aguarda ficar ocioso para aplicar.
8. Sem servidor, cliente mantém configuração local e fila. Ao reconectar, retoma comunicação; comando vencido não inicia irrigação.
9. Antes de voltar ao controlador interno, parar e encerrar cliente independente. Não operar dois controladores para o mesmo GUID.

CLIENT_DB_PATH altera banco do cliente. Padrão data/client-GUID.sqlite, separado do banco do servidor. Não apagar banco para contornar erro de protocolo; ele preserva deduplicação e fila. Token e GUID devem corresponder. O cliente simula sensores/saídas em Node, sem GPIO físico.

Desconexão de cliente HTTP é detectada por ausência de heartbeat em 90 s. Estado anterior de saída não é confirmação atual. Comando recebido sem ACK posterior pode ficar não confirmado e ser reconciliado; isso não autoriza repetição do acionamento.

## Atualização para v0.6

Com servidor parado, copiar o banco para backup, executar git pull --ff-only e npm start. Migração automática preserva dados. Não apagar data/. Ver roteiro em VALIDACAO_V06.md. Minha senha exige senha atual e novo login; Propriedades e áreas permite gestão de contas e revogação de vínculo pelo administrador do sistema.

## Atualização para v0.7

Parar servidor e cliente independente, copiar bancos para backup e atualizar com git pull --ff-only. Reiniciar npm start e npm run client com os parâmetros existentes. Não apagar data/; não há migração de tabelas nesta versão. Visão geral apresenta diagnóstico e próxima agenda por conjunto. Sem cliente atualizado, condições do controlador HTTP ficam desconhecidas; runtime antigo continua aceito. Roteiro em VALIDACAO_V07.md.

## Atualização para v0.8

Com servidor/cliente parados, copiar bancos para backup e executar git pull --ff-only; reiniciar npm start e cliente independente, se usado. Não apagar data/. Sem migração de tabelas. Em Histórico, usar Consultar / atualizar para aplicar filtros e incluir novos registros; período em UTC, exibição no fuso do dispositivo. Exportação CSV/JSON usa filtros informados e todos os resultados até o limite documentado. Roteiro em VALIDACAO_V08.md.

## Atualização para v0.9

Parar servidor/cliente e copiar bancos para backup antes de git pull --ff-only; reiniciar npm start. Migração adiciona propriedade aos registros e tabelas de ciclo de vida sem apagar IDs/dados. Gerenciar dispositivo oferece arquivo/restauração/transferência. No HTTP, desabilitar/publicar saídas/agendas e confirmar ociosidade antes de arquivar. Após transferência, restaurar no destino e provisionar novo token e CLIENT_DB_PATH, preservando banco antigo; não reenviar fila anterior com credencial nova. Histórico possui seletor próprio para GUIDs transferidos. Roteiro em VALIDACAO_V09.md.

## Atualização para v0.10

Parar servidor/cliente, copiar bancos para backup, git pull --ff-only e npm start. Reiniciar cliente independente, se usado: ambos migram contadores de Outbox. Não apagar data/. Migração preserva IDs/conteúdo/tentativas e não executa retenção. Fila de envio oferece consulta, cancelamento, reprocessamento e prévia de retenção manual; aplicação remove definitivamente apenas conteúdo das mensagens encerradas selecionadas. Roteiro em VALIDACAO_V010.md.

## Atualização v0.12 — entrega consolidada 1–3

Pare portal/cliente, preserve bancos e faça backup antes de git pull --ff-only. Execute npm test e npm run check. Bancos legados continuam compatíveis sem chave, mas isso é modo de demonstração sem proteção de segredos em repouso. Para proteger, gere chave externa e migre para banco novo, conforme [OPERACAO_SEGURA.md](OPERACAO_SEGURA.md); apontar chave para banco legado sem migração é recusado. Nunca apagar data/ para resolver esse erro.

Cliente deve ser atualizado para suportar recuperação em quarentena. Encaminhamento HTTP usa a integração por dispositivo já configurada; ACK de ingestão confirma só histórico local. Ver [VALIDACAO_V012.md](VALIDACAO_V012.md) para APIs, migrações, métricas e limites. A próxima entrega consolida etapas 4–6, antes da avaliação do ESP32.

## Correção v0.12.1 — Windows

A v0.12.1 corrige limpeza de arquivos SQLite nos testes e espera da sincronização do cliente. Com portal/cliente parados, git pull --ff-only, npm test e npm run check. Reinicie os processos após atualizar. Sem migração adicional nem alteração de bancos/chaves. Ver CORRECAO_V0121.md para o diagnóstico e evidências; confirmação no Windows permanece a cargo do ambiente do usuário.
