# Firmware 0.8.0 — revisão do portal único e operação offline

06/11/2026. Mesmo painel para Wi-Fi e irrigação, acessível pelo AP ou IP da LAN. O firmware já servia o mesmo HTML; o primeiro cadastro ocultava as outras abas. A revisão mantém Operação, Configuração, Sensores, Eventos, Fila e Wi-Fi e acesso visíveis e oferece o atalho Concluir cadastro. Durante a credencial temporária, as consultas das rotas conhecidas são permitidas somente pelo AP, e operações/alterações ficam bloqueadas na API e na GUI. Não há liberação de conta existente, remoção de CSRF ou de autoridade central.

Após o cadastro, escolha somente rede do ESP32 para operar sem roteador. Configuração, eventos, sensores e comandos usam a API embutida, sem CDN/Node/Internet. A configuração lógica aplicada e os sensores/saídas físicas continuam sujeitos às condições existentes. Perfil padrão physical=0 não aciona hardware.

## Relógio sem Internet

Sem hora válida após boot offline, administrador pode usar a hora UTC do navegador em Operação. POST /api/clock aceita epoch_s inteiro 2020–9999, exige admin/CSRF/cadastro concluído/ociosidade/ausência de quarentena/falha e não sobrescreve hora válida. Não altera fuso, revisão, diários, checkpoint ou previousEpoch; a observação/recuperação normal permanece. O status informa origem browser/sntp/unset e hora UTC. SNTP posterior pode assumir novamente. O navegador fornece a hora declarada pelo administrador; conferir relógio antes de enviar. Sem RTC com bateria, repetir após desligar/reiniciar sem rede. Não promete autonomia de calendário após perda de energia sem fonte de hora.

## Evidências

- 15 executáveis nativos C++/UBSan e cinco fixtures de bytes/hash; testes de rotas GET/POST do primeiro acesso, interface AP, bloqueio de comandos e política de relógio (perfis, CSRF, cadastro, ociosidade, relógio válido e limites 2020/9999).
- 288 testes Node e sintaxe aprovados.
- Chromium 153: 26 cenários com API sintética, incluindo primeiro acesso com seis abas, configuração apenas para consulta, AP-only, relógio do navegador, teste/parada e edição de irrigação/Wi-Fi no mesmo painel; bloqueios de operador/atividade/quarentena. Zero erros JavaScript e verificação de largura desktop/móvel. [Registro](LOCAL_PORTAL_UI_V08.json), capturas e hashes no manual. Não executa CPU/SDK/NVS/HTTP Basic real.
- [Compilação alvo](BUILD_FIRMWARE_V08.json): matriz padrão aprovada em Linux, 67.024 bytes de RAM estática e 1.422.792 bytes de aplicativo (50,5% da partição). Offset 0x50000, imagens extras fora das partições de dados e aplicativo combinado conferido byte a byte. Fontes compiladas conferidas por hash. Outros perfis/Windows desta revisão não foram compilados. As falhas iniciais de instalação/download foram resolvidas sem desativar TLS/checksum.

## Migração e validação real

Sem alteração de esquema/chaves NVS, partições, GPIO, GUID, configuração ou histórico; conta e senhas existentes preservadas. Gravar firmware completo inclui a página nova; não usar uploadfs/erase_flash. Capturas antigas no manual são históricas. O portal central de gestão Node/SQLite não é hospedado pelo ESP32 nem se torna idêntico à interface local.

Conferir na placa: cadastro sem roteador, reinício, menu completo no AP, hora inicialmente inválida, envio da hora correta, edição da configuração local, leitura/eventos e comandos quando habilitados; retorno ao roteador e preservação de conta/revisão. Não foi conectado hardware a este ambiente; testes sintéticos não comprovam rádio, HTTP Basic real, persistência/heap/latência ou funcionamento elétrico/hidráulico. FW-04/FW-05/FW-06 e avaliação de hardware permanecem pendentes.
