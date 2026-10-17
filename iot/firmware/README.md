# Firmware experimental 0.6.0

ESP32 clássico / DevKitC V4 com WROOM-32E, flash 4 MB. Desenvolvimento explicitamente autorizado pelo usuário com hardware ainda em avaliação. Compilação não comprova funcionamento físico.

**[Manual ilustrado do portal local: telas, configuração e operação](../docs/manual-esp32/README.md)**

## Conteúdo

- Controlador estático, dinâmico e híbrido, sensores opcionais, duração monotônica, histerese, pausa e simultaneidade 1–4.
- Agendas por data ou dia/horário, duração ou fim, inclusive virada de dia; ocorrências persistidas antes do acionamento.
- GPIO para módulo de relé em bancada; matriz com comandos RS485 confirmados por nó remoto e lease local.
- Portal local embutido: administrador/operador, CSRF, teste/parada, editor visual de componentes/conjuntos/agendas, calibração, eventos paginados e fila.
- Cliente HTTPS/Bearer, CA obrigatória, configuração/revisão/hash, ACK de comando, runtime, amostra a cada 900 s e eventos.
- GUID NVS, fila persistente limitada, registro de comandos e auditoria local. Nenhum segredo de exemplo é credencial de produção.

## Compilar

Abra esta pasta no VS Code/PlatformIO ou execute:

```sh
pio run -e esp32_matrix
pio run -e esp32_sensor_bench
pio run -e esp32_remote_node
```

Todos usam Arduino-ESP32 3.3.8 / ESP-IDF 5.5.4 via plataforma comunitária pioarduino 55.03.38-1 fixada no platformio.ini, com ArduinoJson 6.21.5. Bancada/nó incluem OneWire 2.3.8 e DallasTemperature 3.11.0. PlatformIO Core requerido/usado: 6.1.19. Bibliotecas não têm suas licenças substituídas pela licença do projeto.

`esp32_matrix`: uma UART RS485 para barramento linear ou hub ativo a definir. NÃO suporta estrela passiva de quatro cabos longos. `esp32_sensor_bench`: quatro sensores de cada tipo/relés por cabos curtos. `esp32_remote_node`: referência ESP32 para um conjunto local; não é promessa de menor custo do nó final. Umidade e temperatura opcionais; um relé local no GPIO25.

Partições: NVS 256 KiB e aplicativo único, sem OTA. Alteração em relação à estrutura 0.1: não aplicar em placa existente sem backup de NVS/GUID e avaliação da transição. Não usar erase_flash como atualização comum; migração física da tabela ainda não validada.

## Provisionar

**Primeiro acesso 0.5.0:** é possível compilar os padrões, gravar e cadastrar Wi-Fi/senhas por USB, sem criar provision.h. Depois, usar a aba Wi-Fi e acesso. Siga [PRIMEIRO_ACESSO_ESP32.md](../docs/PRIMEIRO_ACESSO_ESP32.md). Os passos abaixo continuam válidos para provisionamento compilado; um perfil prov_v1 persistido prevalece sobre credenciais e bindings compilados.

1. Copie include/provision.example.h para include/provision.h (gitignored).
2. Defina SSID/senha, senhas distintas de pelo menos 12 caracteres para admin e operador. Não envie segredos no chat ou commit.
3. Configure URL HTTPS do portal, token individual e CA PEM. O piloto Node HTTP precisa de proxy TLS na rede; `localhost` do computador não é o endereço do ESP32. HTTP sem TLS não é aceito por este firmware.
4. Se existir dispositivo no portal, use seu GUID em IOT_DEVICE_GUID antes do primeiro boot. GUID divergente do NVS bloqueia; não cria outra identidade silenciosamente. Dispositivo novo pode gerar UUID e ser cadastrado com o mesmo valor informado no Serial.
5. Preencha IOT_ACTUATOR_ID_1..4, IOT_MOISTURE_ID_1..4 e IOT_TEMPERATURE_ID_1..4: bindings de IDs estáveis a GPIO/canal ou endereço RS485 1..4. Ordem dos componentes no JSON não define porta física.
6. Na bancada, deixe saídas desabilitadas primeiro. Após verificar driver, polaridade e proteção, IOT_PHYSICAL_OUTPUTS=1 e IOT_RELAY_MASK habilitam apenas canais instalados. GPIO é entrada de módulo com driver, nunca alimentação de bobina.
7. Na matriz/nó, configure transceptor 3,3 V e RX16/TX17/DE19, IOT_RS485_ENABLED=1, endereço único do nó 1..4. Saídas desligadas exigem bias externo; firmware não controla nível antes de executar setup.
8. Para nó remoto, hardware::moistureMask bit0 e temperatureEnabled definem presença local. Calibração/ROM são por slot físico 0. Sensores não instalados continuam desabilitados.

Nenhum perfil de fábrica ativa relés ou Wi-Fi. Perfil padrão é matriz. Não instalar esp32_sensor_bench para ligar sensores diretamente a 100 m.

## Configuração e operação

Com portal central configurado, configuração vem de /iot/v1/configuration. Aplica somente ocioso, confere GUID, revisão, bytes SHA-256 e limites locais; configuração inválida recebe rejeição. Enquanto há execução, a revisão permanece pendente. Persistência deve concluir antes de ACK applied. Configuração do portal não contém os segredos de provisionamento.

Para bancada independente, sem IOT_PORTAL_URL, use a aba Configuração do portal local como administrador. O dispositivo calcula o hash e cria a próxima revisão, recusando rascunhos baseados em revisão antiga. A alternativa por envelope continua disponível via POST /api/configuration. Geração do envelope (Node 24, na raiz do projeto):

```sh
node iot/firmware/scripts/pack-configuration.mjs examples/device-config.json GUID_DO_DISPOSITIVO 1 > configuration-envelope.json
```

Não publique arquivo com credenciais; configuração lógica é separada. O helper usa validateConfig do projeto e JSON.stringify/SHA-256 normativos.

Após Wi-Fi/SNTP, Serial informa GUID; acessar http://IP_DA_PLACA/. Portal local usa HTTP Basic com comparação explícita e CSRF: apenas LAN confiável, sem exposição direta à Internet. Transporte local não cifra senha; produção exige solução TLS/rede protegida e provisionamento mais completo. Nenhuma senha padrão compartilhada é ativada.

| API local | Permissão | Uso |
|---|---|---|
| GET /, /api/state | admin/operador | Monitorar e obter token CSRF da sessão de boot |
| POST /api/command | admin/operador + X-CSRF-Token | zone_id, action=start/stop, source=test/manual e duration_s; resposta informa command_status |
| GET /api/provisioning | admin | Metadados/revisão/IP/bindings; segredos nunca retornados |
| POST /api/provisioning | admin + CSRF, ocioso | expected_revision + settings parcial; persistir e reiniciar; sem mudar GUID/pinos |
| GET /api/timezones | admin | Catálogo IANA embutido e versão |
| GET /api/configuration | admin | Configuração sem segredos |
| POST /api/configuration | admin + CSRF | Somente bancada independente; envelope completo |
| POST /api/configuration/draft | admin + CSRF, ocioso | expected_revision + configuration; hash na placa, revisão seguinte; central/nó recusam edição |
| GET /api/sensors | admin | ADC bruto, ROMs e calibração salva por slot; matriz não lê sensores locais |
| POST /api/calibration | admin + CSRF, ocioso | slot 0..3 + dry/wet, ou slot + rom hexadecimal de 16 caracteres |
| GET /api/events?before=CURSOR | admin/operador | Até 10 registros por página, mais recentes primeiro; anel de 64, next_before string/null; horário desconhecido null |
| GET /api/outbox | admin | Contagem/tentativas sem payload ou segredos |
| POST /api/outbox/cancel | admin + CSRF, ocioso/sem envio | expected_head SHA256 da cabeça + reason; cancela uma mensagem com auditoria |
| POST /api/events/replay | admin + CSRF, ocioso | sequence string + event_id; reenvia evento retido sem executar saídas |
| POST /api/recovery/activate | admin + CSRF, ocioso | Libera quarentena após conferência/novo token e reinicia |
| POST /api/outbox/retry | admin + CSRF, ocioso | Repetir cabeça mantendo corpo/IDs; não corrige token inválido |

Calibração HD-38: pontos raw 0..4095 separados por pelo menos 200; validar solo real e tensão AO antes de conectar. ROMs obtidos no diagnóstico e persistidos; não vincular por ordem de descoberta. Temperatura não controla irrigação.

## Testes e limites

```sh
python iot/firmware/scripts/test-native.py
npm run contracts:check
npm test
```

Executar os comandos acima a partir da raiz. Ambiente host requer g++, Python e Node. Resultados, limitações e roteiro físico: [../docs/VALIDACAO_FIRMWARE_V02.md](../docs/VALIDACAO_FIRMWARE_V02.md).

Esta versão usa 597 fusos/aliases IANA 2026e com DST e regras futuras embutidas; datas ISO com ano de quatro dígitos entre 2020 e 9999 (tempo de 64 bits obrigatório), limite de 64 horários expandidos e journal de até 128 ocorrências recentes no dispositivo, com corte temporal/compactação automática na 0.3.1. Não retoma execução no boot; recuperação resumida de observação disponível na 0.2.1; detalhes por agenda disponíveis na 0.3.2; Regras legais futuras exigem atualização da base; gap DST é omitido e fold usa o primeiro instante. Falha/saturação deve ser diagnosticada, não apagada automaticamente.

## Recuperação 0.2.1

Checkpoint de observação a cada 60 s, por revisão, sem alterar GUID ou registros existentes. Resumo local de janelas encerradas durante lacunas: datas específicas também fora dos últimos sete dias, recorrências semanais limitadas a sete dias com aviso. Não retoma saídas nem executa janelas encerradas. Janelas já abertas no retorno também são consumidas, bloqueando início tardio estático/híbrido. Primeiro uso sem checkpoint e nova revisão começam no instante atual. Retrocesso bloqueia avaliação de agendas até alcançar o último instante observado; a proteção entre boots cobre somente o checkpoint persistido.

O resumo é de períodos não observados, não prova de irrigação perdida: pode incluir o último minuto antes de uma queda e repetir após queda durante a gravação. Não é encaminhado como evento de acionamento ao portal central. Falha de armazenamento bloqueia automação; checkpoint inválido exige diagnóstico, sem apagar NVS. Retenção das 128 ocorrências/conjunto e conflitos detalhados permanecem pendentes. Ver [validação 0.2.1](../docs/VALIDACAO_FIRMWARE_V021.md).

## Portal incluído na flash — 0.2.2

O portal local está separado em [portal/LocalPortalPage.h](portal/LocalPortalPage.h) e integrado ao executável por `PROGMEM`/`send_P`. Compilar e gravar o firmware provisionado inclui a página na memória, sem `uploadfs` ou dependências web externas. Veja [instruções de compilação, gravação e acesso](portal/README.md). Padrões de fábrica continuam sem Wi-Fi/saídas; o portal requer Wi-Fi e senha admin de pelo menos 12 caracteres no arquivo privado de provisionamento. O portal central Node.js continua separado.

## Tempo de 64 bits — 0.3.0

Removidos os limites de 2037/INT32_MAX no calendário, no relógio válido e nos checkpoints. Compilação exige `sizeof(time_t) >= 8`, tipo assinado e ESP-IDF 5+. Não force uma definição de time_t nem desabilite esses asserts: o relógio, libc e TLS precisam concordar. O diagnóstico `/api/state` inclui `time_t_bits` e `calendar_max_year`.

O limite de ano 9999 é o formato ISO de quatro dígitos aceito nesta versão, não promessa de operação física ou de SNTP por milênios. O SDK possui convenções de eras NTP documentadas até 2104; não foi implementado um cliente NTP independente dessas convenções. UTC/BRT fixo continua sendo uma limitação separada.

A plataforma de build mudou, mas os GUIDs, chaves NVS, esquema de configuração, partições e protocolo central não foram alterados. As chaves epoch persistidas já eram int64. Não apague NVS para atualizar. A migração física entre versões do framework ainda exige bancada.

Para conferir os três perfis padrão e os três com provisionamento sintético, sem tocar nas credenciais do usuário e sem upload:

```sh
python -m pip install platformio==6.1.19
python iot/firmware/scripts/check-build.py
```

Esse comando deve ser executado na raiz, compila em pasta temporária e descarta os binários sintéticos. Para gravar sua placa, use seu provisionamento e os comandos normais do [portal local](portal/README.md). Evidências e próximos passos em [VALIDACAO_FIRMWARE_V03.md](../docs/VALIDACAO_FIRMWARE_V03.md).

## Retenção de ocorrências — 0.3.1

A retenção já não esgota após 128 execuções acumuladas por conjunto. O journal recente e um corte temporal monotônico são persistidos no mesmo blob NVS antes de acionar saída. No primeiro upgrade, os hashes legados são preservados e janelas anteriores ao instante de migração não são retomadas. Não faça downgrade sem plano/backup. O limite de 128 registros **recentes** ainda existe para rajadas/configurações que excedam o orçamento; não há expulsão insegura de registros.

Consulte [VALIDACAO_FIRMWARE_V031.md](../docs/VALIDACAO_FIRMWARE_V031.md) para formato, falhas, ensaio de 16 mil ocorrências, migração e limites. GUID e partições permanecem iguais. Diagnóstico informa corte, ocupação e capacidade do journal. A auditoria por agenda/ocorrência está disponível na 0.3.2.

## Diagnóstico de agendas — 0.3.2

Execuções automáticas e observações de recuperação agora identificam agenda, horário e ocorrência. Conflitos registram a agenda omitida e a concorrente quando existente. O híbrido segue sua janela original; agendas estáticas consecutivas podem terminar/iniciar no mesmo tick, sem conflito artificial.

Observações são registradas localmente e enviadas por telemetria com event_id estável, deduplicado pelo portal. Fila de 16 mensagens e auditoria de 64 registros permanecem limitadas: saturação pode deixar o evento somente local e incrementar lost_events, sem retransmissão posterior do anel. Consulte [VALIDACAO_FIRMWARE_V032.md](../docs/VALIDACAO_FIRMWARE_V032.md). O portal local completo continua sendo a próxima entrega.

## Portal local completo — 0.4.0

Fontes em portal/index.html, style.css e app.js. O pre-script PlatformIO gera LocalPortalPage.h, incorporado na flash: sem CDN, uploadfs ou servidor Node na placa. A verificação nativa rejeita snapshot desatualizado. Veja [portal/README.md](portal/README.md).

/api/state autenticado acrescenta local_role, local_role_type, configuration_authority, capabilities e configuration_body_limit. São informações da interface; autorização, CSRF e ociosidade continuam verificados no firmware. A configuração local usa expected_revision para impedir sobrescrita silenciosa e preserva o rascunho quando há conflito. SHA-256 é calculado no ESP32; não depende de WebCrypto/HTTPS do navegador.

Migração da API local de eventos: a resposta passa a ser paginada em ordem decrescente. Consumidores devem seguir next_before como string opaca, sem converter para Number. O anel pode girar entre páginas; não representa snapshot imutável. Os endpoints remotos /iot/v1 e os envelopes existentes permanecem compatíveis. GUID, partições e esquema de configuração são preservados.

Wi-Fi, senhas, CA, token, GPIO, bindings e presença física de sensores continuam provisionados fora deste editor; habilitar componente lógico não habilita canal físico nem saídas. Matriz consulta sensores sem calibrá-los; nó calibra apenas slot 0 e recebe comandos da matriz. Testes e limites: [VALIDACAO_FIRMWARE_V04.md](../docs/VALIDACAO_FIRMWARE_V04.md).

## Provisionamento sem recompilar — 0.5.0

Rede, senhas locais, origem HTTPS/token/CA e bindings podem ser mantidos pela GUI/USB. Primeiro cadastro/recuperação usa USB nos primeiros três minutos após reset, com controlador ocioso; não há AP aberto ou senha padrão. Alteração válida só entra após reiniciar; erro de rede/senha é recuperado por USB sem apagar dados. Campos secretos omitidos mantêm valores; remoção explícita por string vazia (admin não pode ser removido). Destino central só muda com outbox vazia. Perfil inválido bloqueia rede/automação até recuperação. GPIO, sensores fisicamente presentes, polaridade e habilitação elétrica continuam compilados.

Assistente: `python iot/firmware/scripts/provision-device.py --port COM5`; status/IP: mesmo comando com --status. Requer pyserial e monitor fechado. Portas podem reiniciar a placa ao abrir; mensagem de IP também disponível no Serial/roteador. Perfil/NVS/GUID persistem em upload comum. Ver [VALIDACAO_FIRMWARE_V05.md](../docs/VALIDACAO_FIRMWARE_V05.md) para API, precedência, testes e limites. FW-06 permanece parcial: cifragem/secure boot, gestão de produção e migração física não estão concluídos.

## Gravação corrigida — 0.5.1

Use 0.5.1 ou posterior para o próximo upload. O post-script mantém o aplicativo em 0x50000 e remove a imagem boot_app0 que antes incidia na NVS. Não usar os binários gerados anteriormente pela plataforma 55.03.38-1 sem essa correção. O comando normal de upload usa imagens separadas; firmware.factory.bin não é atualização que preserva NVS. Não executar erase_flash. Evidência e explicação em [VALIDACAO_FIRMWARE_V051.md](../docs/VALIDACAO_FIRMWARE_V051.md).

## Entrega 0.6.0

Backup criptografado externo, restauração retomável e quarentena, gestão individual da fila e recuperação automática/manual dos eventos retidos. A auditoria continua limitada a 64 registros; não há histórico infinito nem recuperação de amostras de sensores. O cursor persistente pode repetir um evento após queda; o event_id original permite deduplicação central. Uma posição da fila é reservada durante o backfill. Trocar destino impede reenvio do histórico anterior. Eventos sem horário sincronizado/ID/revisão não são reconstruídos. Cancelar uma mensagem não apaga o evento da auditoria: ele pode ser reenviado explicitamente ou pelo backfill se o cursor ainda não o tiver ultrapassado.

Consulte [recuperação, migrações e produção](../docs/RECUPERACAO_E_PRODUCAO_ESP32.md), [base IANA](timezones/README.md) e [validação 0.6.0](../docs/VALIDACAO_FIRMWARE_V06.md). O perfil hardened é um bloqueio de boot e **não prepara/cifra uma placa automaticamente**. Não fazer downgrade direto para 0.5.x após migrar a fila. As restrições de atualização em 0x50000 e de factory.bin da 0.5.1 permanecem.

Ferramentas complementares de pré-produção: [auditoria offline e tabela candidata separada](../docs/AUDITORIA_PREPRODUCAO_ESP32.md). `partitions.production-candidate.csv` não está ativada no build padrão. Não altera o roteiro do primeiro teste Wi-Fi/portal.
