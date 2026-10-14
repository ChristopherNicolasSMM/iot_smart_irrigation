# Entrega consolidada v0.12 — etapas 1 a 3

14/10/2026. Escopo aprovado: concluir proteção/recuperação P1-06, ensaio de estabilidade/desempenho P1-07 e encaminhamento HTTP P1-11. A próxima entrega reúne contrato P1-12, testes temporais P1-10 e roteiro/validação de uso P0-01. Só depois será avaliado o hardware e preparado o firmware do ESP32.

## 1. Proteção e recuperação

Chave externa AES-256-GCM protege estado do portal/cliente, perfis/corpos/cabeçalhos da fila e cabeçalhos atuais. Modo obrigatório via VAULT_REQUIRED, geração exclusiva de chave, migração e rotação para banco novo, preservando origem, GUIDs, registros e tentativas. Autenticação vincula ciphertext à tabela/linha/coluna. Banco protegido sem chave correta é recusado antes de iniciar workers. Banco legado sem proteção continua compatível em modo demonstração; não há migração silenciosa ao definir uma chave.

Backup IRRBACK1 atende portal e cliente, identificando o tipo, com senha separada e limite de 64 MiB. Backup de banco protegido requer também a chave externa correspondente. Recuperação do portal conserva quarentena v0.11. Cliente recuperado exige configuração completamente desabilitada e ACK confirmado antes de liberar controle; falha do ACK mantém recovery_hold persistido. Saída ativa do snapshot não reinicia. Token permanece externo e o cliente recusa banco de outro GUID.

Procedimentos Bash/PowerShell, migração, rotação, recuperação e limites: [OPERACAO_SEGURA.md](OPERACAO_SEGURA.md). Não é cifragem integral do disco/SQLite, nem KMS ou prova de parada física. Chaves antigas precisam ser mantidas para recuperar backups antigos. Cópias legadas em claro não são apagadas. A escolha de KMS/hospedagem fica em EX-03, sem bloquear estas ferramentas do piloto.

## 2. Estabilidade, desempenho e refatoração

Novos módulos secrets, secret-management e forwarding isolam proteção/migração/encaminhamento. Backup foi separado em funções de validação, cifragem, decifragem, quarentena e publicação. Outbox ganhou índice por dispositivo/estado e seleciona metadados antes de carregar o conteúdo de uma mensagem elegível; mensagens offline não têm seus payloads carregados/decifrados. Fechamento de portal/cliente é idempotente.

Teste de controle local com portal indisponível confirma término por duração e ACK após reconexão/reinício. Outros testes cobrem saturação, retries limitados, integridade e seleção entre filas offline/online.

Ensaio reproduzível:

```bash
npm run soak
# Para repetir o ensaio desta entrega em Bash:
SOAK_SECONDS=300 SOAK_MESSAGES=6000 npm run soak
```

O script só usa arquivos temporários e remove seus bancos/chaves. SOAK_SECONDS aceita 5..3600, padrão 120; SOAK_MESSAGES aceita 100..100000, padrão 6000. É ferramenta de teste, não automação agendada. Portal real em loopback, transporte externo injetado, banco cifrado, quatro dispositivos, duas leituras por pacote. Carga distribuída durante cinco minutos, duplicatas, timeout no destino entre 15% e 40% do ensaio, capacidade de 25 mensagens por dispositivo, até duas tentativas e reinício do portal na metade. Verificou autenticação após reinício e deduplicação da última mensagem. Ao final confere contagens/integridade, drena a fila elegível e executa sete dias acelerados de controle estático em quatro dispositivos.

Medições do ambiente Node v24.19.0 / AMD EPYC 9V74:

| Medida | Resultado |
|---|---:|
| Carga em tempo real | 300 s |
| Execução total, incluindo controle acelerado | 322,09 s |
| Mensagens únicas preservadas no histórico local | 6000 |
| Repetições deduplicadas | 301 |
| Entregas externas confirmadas | 4369 |
| Envios externos encerrados em falha | 731 |
| Envios descartados por saturação | 900 |
| Pendências ao término | 0 |
| Latência HTTP p95 | 8,646 ms |
| Heap máximo observado após GC explícito | 10,97 MiB |
| Heap final observado após GC explícito | 10,94 MiB |
| RSS máximo amostrado | 134,27 MiB |
| Atraso do event loop p95, fase real | 21,365 ms |
| Banco / WAL ao medir | 23945216 / 4231272 bytes |
| Controle acelerado | 7 dias × 4 dispositivos |
| Amostras nesse controle | 2688 (a cada 900 s) |
| Tempo da fase acelerada | 22 s |

Arquivo bruto sem segredos: [benchmarks/V012_SOAK.json](benchmarks/V012_SOAK.json). A falha do destino não removeu nenhuma das 6000 mensagens locais: os 900 descartes e 731 falhas referem-se à fila de encaminhamento, com logs explícitos. Limite de fila foi respeitado em todos os dispositivos. O objetivo foi verificar pressão/falhas; não afirmar ausência de perdas externas.

O ensaio longo terminou com código 0 e todas as asserções aprovadas, mas stderr apresentou `fatal library error, lookup self` ao encerramento. Origem não isolada; o ensaio curto de 5 s mais sete dias acelerados e testes separados de GC/CPU não reproduziram o alerta. Registra-se a ocorrência para repetição no ambiente de destino; não declarar o ensaio livre de alertas. A suíte funcional/sintaxe não apresentou essa mensagem.

Limites: cinco minutos reais não equivalem a 24 h contínuas; sete dias são tempo virtual. GC foi explícito nas medições de heap, RSS foi amostrado e os valores não são garantia de ausência de vazamento/capacidade máxima. História/fila terminal/inbox continuam crescendo e não têm limpeza automática. O ensaio não inclui Internet/TLS real, execução elétrica ou os máximos de sensores/solenoides por ESP32. Medições Node não se aplicam a RAM/flash do ESP32. Ensaio de 24 h na hospedagem escolhida e bancada/campo permanecem validações operacionais posteriores.

## 3. Encaminhamento de telemetria HTTP

`POST /iot/v1/telemetry` mantém autenticação/deduplicação/histórico e acrescenta `forwarding` no ACK:

```json
{"accepted":true,"message_id":"id-do-cliente","duplicate":false,"forwarding":{"status":"pending","message_id":"forward-..."}}
```

ACK accepted confirma armazenamento local, não recebimento por terceiros. Status: pending, disabled, duplicate, dropped, loop_suppressed, local_outbox_suppressed ou integration_events_suppressed. Falha real de escrita faz rollback de ingestão/fila e retorna erro sem accepted. Perfil desabilitado não envia; fila cheia ou template inválido mantém histórico e registra perda do envio.

Usa o perfil/cabeçalhos de integração já configurados por GUID, com snapshot cifrado quando o banco é protegido. Token do cliente nunca é reaproveitado como autenticação de terceiros. Leituras e timestamps vêm do pacote recebido; variáveis sensors usam essas leituras, nunca um valor sintético do portal. Sensor ausente no pacote tem valor null/qualidade missing. O exemplo padrão inclui readings/events/outputs; outputs ausente é normalizado para lista vazia.

ID externo é `forward-` + SHA-256 de gateway persistente, propriedade, GUID e ID recebido. Isso evita colisão entre dispositivos/propriedades com IDs iguais e permanece estável em reenvios/reinícios. ID original permanece em source_message_id. Receptor externo deve confirmar o ID externo conforme seu modelo de resposta e deduplicá-lo.

Encaminhamento força **integration_trace** com o identificador do portal, mesmo se o template não o incluir, e source_message_id no payload. Aceita no máximo quatro saltos, sem IDs repetidos; seu próprio identificador ou teto de saltos suprime novo envio. Mensagem já pertencente à Outbox local e pacotes compostos só por eventos de integração também não são encaminhados. Sistemas intermediários precisam preservar esses campos para a proteção contra ciclos; não é defesa contra um intermediário que os remove deliberadamente.

Fila originada na ingestão HTTP pode enviar mesmo se o dispositivo ficar offline: envio externo é trabalho do portal. Arquivo/transferência e propriedade continuam bloqueando mensagens incompatíveis. O canal de controle não depende da resposta externa. HTTPS, DNS fixado, bloqueio de destinos privados, limites de resposta/payload e ACK da integração permanecem vigentes; HTTP privado exige ALLOW_LOCAL_INTEGRATIONS explícito para laboratório. Só pacotes novos recebidos após habilitar o perfil são encaminhados; não há reprocessamento automático do histórico anterior.

## Migrações e testes

Outbox ganha source_kind com default local e índice outbox_device_status; linhas antigas preservadas. Banco protegido ganha secrets_meta (ID/fingerprint da chave, sem chave). Restauração registra tipo na recovery_audit e recovery_hold apenas no estado do cliente. Não modifica schema de agendas nem requer apagar data/.

215 testes aprovados, npm run check e git diff --check. Também foi executado o ciclo CLI keygen → migrate → backup → restore → rotate → abertura com chave obrigatória em bancos temporários. Incluem cifragem/autenticação/contexto, chave errada/missing, migração/rotação com origem intacta, proteção de segredos legados no arquivo final, backup protegido, reprocessamento/retenção, recuperação bloqueada/ACK falho/execução ativa, término offline/reconexão, encaminhamento HTTP local real, repetição, loops, template inválido, saturação e rollback. Sem nova validação visual pelo usuário ou hardware.

## Próxima entrega consolidada — etapas 4 a 6

1. P1-12: schemas/OpenAPI e fixtures compartilhadas do contrato.
2. P1-10: transições temporais raras e recuperação após longas paradas.
3. P0-01: conferência do uso do portal e roteiro para validação do usuário.

Depois das duas entregas, avaliar com o usuário e iniciar HW-01/HW-02. Firmware, sensores reais, pinagem e dimensionamento hidráulico continuam fora desta entrega.
