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
