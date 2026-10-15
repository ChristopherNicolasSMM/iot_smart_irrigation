# Portal local na memória do ESP32

A página `LocalPortalPage.h` contém HTML e JavaScript, sem CDN ou arquivos externos. É incluída por `src/main.cpp` na compilação, armazenada na flash com `PROGMEM` e servida por `WebServer::send_P`. Não exige LittleFS/SPIFFS, `uploadfs`, cartão ou servidor Node.js no ESP32.

O portal central continua na raiz do repositório e roda no computador/servidor. O portal desta pasta oferece autenticação admin/operador, estado dos conjuntos e teste/parada; as APIs locais de configuração, calibração, eventos e fila permanecem documentadas no README do firmware. A página atual não é a interface completa do portal central nem um editor visual de agendas.

## Compilar e gravar (a partir da raiz)

1. Instale PlatformIO/VS Code e copie `iot/firmware/include/provision.example.h` para `iot/firmware/include/provision.h`.
2. Preencha Wi-Fi, senha admin com pelo menos 12 caracteres e senha diferente para operador. Configure GUID/bindings e CA/token do portal central quando usado. O arquivo é ignorado pelo Git.
3. Mantenha `IOT_PHYSICAL_OUTPUTS=0` até validar circuito, polaridade e proteção.
4. Execute:

```sh
pio run -d iot/firmware -e esp32_matrix
pio run -d iot/firmware -e esp32_matrix -t upload
pio device monitor -d iot/firmware -e esp32_matrix
```

Use `esp32_sensor_bench` para bancada com sensores locais ou `esp32_remote_node` para nó, conforme documentação. Se necessário, informe a porta com `--upload-port COM5` no comando de upload (substitua pela porta real).

5. Consulte o IP da placa na rede/roteador e abra `http://IP_DA_PLACA/`. Use `admin` ou `operador` com a senha provisionada. O portal só é iniciado quando há SSID e senha admin válida. Sem provisionamento, os padrões não habilitam Wi-Fi, portal ou saídas. O compilador pode remover a página do binário quando o portal está desabilitado por constantes.

A gravação normal já inclui a página na flash. Não há etapa separada de upload do site. Não use `erase_flash` como atualização comum: preserve GUID/NVS e observe a alteração de partições da versão 0.2. A memória e o funcionamento em placa continuam sujeitos à validação de bancada. O portal local usa HTTP Basic sem TLS e deve ficar em LAN confiável.
