# Portal local na memória do ESP32

A página `LocalPortalPage.h` contém HTML e JavaScript, sem CDN ou arquivos externos. É incluída por `src/main.cpp` na compilação, armazenada na flash com `PROGMEM` e servida por `WebServer::send_P`. Não exige LittleFS/SPIFFS, `uploadfs`, cartão ou servidor Node.js no ESP32.

O portal central continua na raiz do repositório e roda no computador/servidor. A partir da versão 0.4.0 este portal local inclui:

- Operação para administrador/operador: diagnóstico, teste de 1–30 segundos e parada.
- Administrador: editor visual de componentes, conjuntos, sensores opcionais, agendas por data ou horários independentes por dia; importação/exportação JSON e edição avançada.
- Calibração HD-38 e vínculo ROM DS18B20 no canal físico; matriz não calibra, nó usa apenas slot 0.
- Eventos locais paginados e fila com reprocessamento da primeira mensagem.

Com portal central configurado, a configuração local fica somente para consulta/exportação. Aplicar exige revisão atual, controlador parado e validação do firmware; falha/conflito preserva o rascunho. A atualização de estado não substitui dados em edição. Senhas são administradas pelo navegador via HTTP Basic; não há logout confiável implementado nessa autenticação. O portal não guarda credenciais em localStorage.

Edite index.html, style.css e app.js. O pre-script PlatformIO gera LocalPortalPage.h automaticamente; para conferir antes do commit: `python iot/firmware/scripts/embed-portal.py --check`. O hash da configuração é calculado na placa, sem exigir contexto HTTPS do navegador. IDs novos usam valores aleatórios para evitar reutilização automática de nomes removidos; IDs existentes são preservados. Presença física/GPIO continuam compilados; bindings e segredos podem ser provisionados em NVS na 0.5.0.

Na 0.7.0, a aba **Wi-Fi e acesso** permite primeiro cadastro pela rede própria e manutenção administrativa sem recompilar: roteador, nome/senha AP, contas, integração HTTPS e bindings, com reinício após salvar. Primeiro acesso e recuperação opcional por USB: [PRIMEIRO_ACESSO_ESP32.md](../../docs/PRIMEIRO_ACESSO_ESP32.md). Os passos de provision.h abaixo são opcionais para o primeiro teste e continuam disponíveis como base compilada. Pinos e habilitação física continuam compilados.

## Compilar e gravar (a partir da raiz)

1. Instale PlatformIO/VS Code. O primeiro cadastro pode ser feito inteiramente pelo portal.
2. Opcional: copie `iot/firmware/include/provision.example.h` para `iot/firmware/include/provision.h` para uma base compilada. Configure Wi-Fi, senha admin (mínimo 12 caracteres), GUID/bindings e CA/token quando usados. Esse arquivo privado é ignorado pelo Git.
3. Mantenha `IOT_PHYSICAL_OUTPUTS=0` até validar circuito, polaridade e proteção.
4. Execute:

```sh
pio run -d iot/firmware -e esp32_matrix
pio run -d iot/firmware -e esp32_matrix -t upload
pio device monitor -d iot/firmware -e esp32_matrix
```

Use `esp32_sensor_bench` para bancada com sensores locais ou `esp32_remote_node` para nó, conforme documentação. Se necessário, informe a porta com `--upload-port COM5` no comando de upload (substitua pela porta real).

5. Sem conta anterior, abra o monitor Serial e pressione EN/reset. Conecte à rede `ConectaAgro-<final do GUID>` com a chave individual exibida e abra **http://192.168.4.1/**. Primeiro login na 0.7.1: usuário `admin` e senha `admin`. Cadastre uma senha admin permanente e o roteador; salve e reinicie. Dispositivos já configurados usam as contas existentes. Sem conexão ao roteador por 30 s, o AP abre novamente. Nome/senha próprios podem ser modificados no portal. Consulte o [roteiro completo](../../docs/PRIMEIRO_ACESSO_ESP32.md).

A gravação normal já inclui a página na flash. Não há etapa separada de upload do site. Não use `erase_flash` como atualização comum: preserve GUID/NVS e observe a alteração de partições da versão 0.2. A memória e o funcionamento em placa continuam sujeitos à validação de bancada. O portal local usa HTTP Basic sem TLS e deve ficar em LAN confiável.
