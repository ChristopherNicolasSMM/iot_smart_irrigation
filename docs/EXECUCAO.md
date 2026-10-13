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
