# Firmware 0.7.0 — rede própria e portal de configuração

31/10/2026. Alteração solicitada após o primeiro upload real da 0.6.1: primeiro cadastro no portal da rede criada pelo ESP32 e alternativa automática quando o roteador não estiver disponível.

## Entrega

- Sem conta/perfil anterior, AP protegido `ConectaAgro-<GUID final>` e portal em http://192.168.4.1/. Chave aleatória de 24 caracteres, persistida em `ap_key`, exibida ao gerar e nos boots com primeiro cadastro pendente; não há chave global de fábrica.
- Primeiro login admin/chave inicial pelo AP, com acesso restrito a estado/provisionamento e CSRF. Não funciona pela interface do roteador, em recuperação com erro, com perfil persistido ou no hardened. Cadastro exige senha admin permanente e salva no mesmo registro versionado `prov_v1` antes de reiniciar.
- Depois do cadastro, a placa usa as contas normais. SSID/senha do roteador podem ser configurados pelo portal. Campo vazio de segredo preserva o atual; opção somente AP remove explicitamente a associação ao roteador.
- Campos novos `ap_ssid` e `ap_password`: nome da rede própria e senha customizável (12–63 ASCII sem espaços). Não devolve senha por HTTP; `has_ap_password` informa presença. Nome vazio retorna ao nome derivado do GUID. A chave gerada permanece como alternativa ao valor customizado, sem modificar GUID.
- Com roteador cadastrado, AP abre depois de 30 s sem associação Wi-Fi. Tentativas de reconexão continuam. Ao retornar a rede, mantém AP para clientes existentes; encerra após 30 s estáveis e nenhum cliente conectado ao AP. Não trata indisponibilidade isolada da Internet como perda da LAN.
- Gerenciamento do rádio em tarefa FreeRTOS separada do controlador; interface HTTP permanece acessível quando AP ativo. DNS para auxílio ao portal e redirecionamento HTTP no AP; abertura automática depende do cliente. Endereço manual sempre documentado.
- Consulta física `WIFI_ACCESS?`, apenas desenvolvimento/janela USB de 180 s/ociosidade, permite recuperar a chave própria. `PROVISION?` e GETs continuam sem senhas. A chave gerada no boot e a consulta explícita são exceções documentadas à ausência de segredos no Serial; ocultar antes de compartilhar logs.

Não há reset de usuário/GUID/histórico, mudança de pinos ou partições. Campos AP ausentes em perfis/backup antigos são opcionais; usam chave individual e nome derivado. Não regrava/hash-normaliza registros legados ao carregar. A gravação de uma revisão nova inclui os campos novos no hash. Não fazer downgrade para versões que recusam esses campos após salvar o perfil novo. Backup cifrado incorpora senha AP customizada; `ap_key` local é preservada numa restauração do mesmo dispositivo.

## Evidências

- 15 executáveis nativos C++ com UBSan e cinco fixtures normativas de bytes/hash.
- Política Wi-Fi: sem roteador, timeout, reconexão, flapping, cliente conectado, histerese, overflow de millis; primeira configuração bloqueada para conta existente/hardened/recuperação com erro e rotas limitadas ao AP.
- Adaptador ArduinoJson real no host: roundtrip de nome/senha AP, redação de segredo, modo AP-only, patch preserva senha omitida e perfis anteriores sem os campos novos. Validação de senha e comprimento em bytes.
- 288 testes Node, 26 fixtures/consistência OpenAPI e sintaxe. Três testes novos do formulário AP-only, segredo vazio/primeiro cadastro e limites do nome/senha.
- Chromium 149/API sintética: 24 cenários, incluindo abertura automática da aba de primeiro cadastro, menu restrito, migração para roteador ao digitar SSID, alteração de nome/senha própria e redação do segredo. Sem erros JavaScript/transbordamento nos cenários desktop/celular. Capturas novas no manual. Não executa handlers Arduino, associação de rádio ou autenticação Basic real.
- Sete builds alvo aprovados: matriz/bancada/nó padrão, os três com provisionamento sintético e matriz hardened (guarda de boot). Ver [BUILD_FIRMWARE_V07.json](BUILD_FIRMWARE_V07.json): matriz padrão com 67.024 bytes de RAM estática e 1.418.924 bytes de aplicativo. Offset 0x50000, imagens extras fora das partições de dados e aplicativo combinado verificado byte a byte. Execução Linux; Windows/radio/hardware desta entrega não testados.

## Limites reais

Não foi conectado ESP32 a este ambiente, realizado upload ou medida de rádio/heap/latência/NVS. O usuário precisa validar o roteiro em [PRIMEIRO_ACESSO_ESP32.md](PRIMEIRO_ACESSO_ESP32.md). O rádio AP/STA compartilha canal; transições podem provocar desconexão breve. Sub-rede AP fixa 192.168.4.0/24 pode conflitar com roteador na mesma faixa: avaliar configuração da rede real. Captive browsers podem não abrir o diálogo Basic automaticamente; usar navegador normal e endereço HTTP direto.

Perfil corrupto não ganha reset de conta pela rede; USB físico continua sendo a recuperação. Segredos em flash, SDK/bootloader/chaves/secure boot e proteção de transporte de produção permanecem FW-06 parcial. O modo hardened exige preparação prévia e não oferece o cadastro temporário. Sem validação elétrica/hidráulica/RS485 ou garantia de bancada.

## Ajuste 0.7.1 — login inicial solicitado

Primeiro login do portal alterado para `admin`/`admin`, exclusivamente no cadastro inicial pelo AP. A senha individual da rede Wi-Fi permanece independente. Perfis/contas existentes, restrições de rotas e exigência de senha permanente permanecem. Os sete builds acima são da 0.7.0; não representam compilação alvo da 0.7.1. Regressão nativa executada nesta revisão; sem upload ou teste físico.
