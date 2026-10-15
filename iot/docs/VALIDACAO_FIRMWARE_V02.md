# Firmware experimental 0.2 — validação e limites

14/10/2026. Usuário autorizou iniciar firmware mesmo com hardware incompleto. FW-01 parcial; HW-01/HW-02 e FW-02 não encerrados.

## Evidências de software

Conferência de encerramento em 14/10/2026: repetidos os três builds padrão com sucesso, os quatro executáveis nativos/UBSan e cinco fixtures de bytes/hash, `npm run check`, `npm run contracts:check` (26 fixtures) e `npm test` (269 aprovados, zero falhas). `git diff --check` sem erros. Nenhum teste físico foi executado nesta conferência.

- PlatformIO Core 6.1.18, espressif32 6.10.0, Arduino-ESP32 2.0.17.
- Três perfis compilados: esp32_matrix, esp32_sensor_bench, esp32_remote_node.
- Segunda compilação com provisionamento **sintético, somente para build**, inclui Wi-Fi, TLS, autenticação, GPIO/RS485 e bindings no binário. Arquivo temporário removido; nenhum segredo real usado. Não entregar/instalar binários deste ensaio como provisionamento de campo.
- Quatro executáveis nativos C++ com -Wall -Wextra -Werror e UBSan: deadline, control, span e auth. Cenários: duração/overflow, testes limitados, exclusão/simultaneidade, falha de sensor/saída/auditoria, híbrido fora/fim de janela, alvo, persistência de ocorrência, CRC, parser, lease/idempotência/token de boot, meia-noite e rejeição de autorização incorreta.
- Cinco fixtures de configuração verificadas para bytes extraídos e hashes normativos, incluindo envelope publicado. Isso não equivale à execução de todos os schemas no ESP32.
- contracts:check: 26 fixtures aprovadas; npm test: 269 testes do portal aprovados. Sem alteração de tabelas/dados/contrato do portal.
- Plataforma e dependências foram obtidas após falhas iniciais de download; compilação finalmente concluída. OneWire 2.3.8 emite dois avisos externos de #undef por perfil de sensores; não houve erro de link.

Medições estáticas em [validacao/FIRMWARE_V02_BUILD.json](validacao/FIRMWARE_V02_BUILD.json). Não representam heap livre com conexões TLS, carga de portal, máximo JSON/fila ou GPIO ligados.

## O que continua delimitado

| Área | Limite / pendência |
|---|---|
| Hardware | Relé, polaridade/boot, válvula AC/DC/corrente, transceptores, fonte/cabo e ramais ainda não validados |
| Tempo | SNTP necessário após boot. Somente UTC e America/Sao_Paulo como BRT fixo atual; sem base histórica IANA/DST. Datas 2020–2037 por time_t da plataforma |
| Agenda | Até 32 agendas e 64 horários expandidos globais; seleção por dia é consumida do portal. Até 128 ocorrências por conjunto; ao esgotar não iniciar automaticamente; retenção/rotação segura é próxima etapa |
| Recuperação | Boot não retoma saída. Registro de interrupção/intent; reconciliação de comandos conservadora. Varredura detalhada de agendas perdidas e recuperação temporal longa do piloto ainda não portadas |
| Conflitos | Controlador impede simultaneidade/conflito de saída; quando várias agendas do mesmo conjunto coincidem escolhe uma janela. Auditoria detalhada por agenda conflitante pendente |
| Comunicação | HTTPS/CA/Bearer e respostas com Content-Length conhecido até 16 KiB; chunked/redirect não aceitos. Teste real ESP32→portal/proxy ainda pendente |
| Filas | 16 trabalhos persistidos, corpo máximo 4096 bytes, ACK matching para telemetria. Até 5 tentativas com atraso; cabeça esgotada bloqueia até reprocessamento administrativo. 401/403 bloqueiam rede até novo boot/provisionamento |
| Identidade/comandos | Até 32 comandos; terminal expirado pode ser substituído. Reconciliação sem registro informa histórico indisponível, nunca autoriza reexecutar |
| Logs | Auditoria local de 64 registros em anel; substituição de antigos explícita no limite. Saturação de outbox incrementa lost_events (desde boot); não prometer histórico infinito offline |
| Segurança | Senhas/token/CA via provision.h, sem provisionamento seguro remoto, gestão de contas dinâmica, cifragem NVS ou flash seguro. HTTP Basic local somente LAN confiável, sem TLS local. CRC do cabo não é autenticação |
| Recursos | Partição NVS 256 KiB/flash 4 MB; transição desde 0.1 não validada. Heap, latência/vida útil NVS, watchdog/carga prolongada e economia do nó final pendentes |
| RS485 | Uma UART/barramento; estrela de 100 m requer hub ativo ou novo hardware. Lease depende do processamento do nó; ausência de circuito externo de segurança não dá garantia de tempo real rígido |

Não classificar a versão como pronta para produção ou integralmente equivalente ao simulador.

## Roteiro de bancada — ainda não executado

1. Confirmar placa/flash, driver e polaridade. Sem válvula, medir entrada de relé durante energização/reset/programação.
2. Compilar/gravar com saídas desabilitadas; conferir GUID persistente, NVS e zero sensores.
3. Conectar sensor em tensão correta; obter raw/ROM, calibrar e testar desconexão/saturação, sem válvula ligada.
4. Validar portal local: sem credencial nega; operador testa/para e não configura/calibra; CSRF ausente nega.
5. Habilitar um canal somente após circuito aprovado; teste 5 s, parada, boot e falha de sensor. Verificar física e logs, não apenas resposta HTTP.
6. Subir portal via HTTPS na LAN, provisionar GUID/token/CA corretos; testar hash adulterado, revisão pendente, ACK perdido, duplicata e expiração.
7. Testar matriz/nó com RS485 curto: CRC, endereço, boot/token, desconexão e lease; validar tensão e latência de resposta.
8. Só depois validar cabo 100 m com carga real, tensão no destino, fonte/temperatura, proteção e topologia aprovada.
9. Testar uma e até quatro válvulas, respeitando capacidade elétrica/hidráulica medida. Ensaiar 24 h, memória e recuperação.
10. Registrar evidências, atualizar backlog e fechar pendências antes de campo.
