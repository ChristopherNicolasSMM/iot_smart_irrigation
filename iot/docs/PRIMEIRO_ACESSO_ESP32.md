# Primeiro build, Wi-Fi e acesso ao portal — firmware 0.6.0

Roteiro para ESP32 clássico WROOM-32/esp32dev. Esta entrega foi verificada em software; os passos abaixo ainda precisam ser executados na sua placa. Para este primeiro teste, use somente USB e mantenha as saídas físicas desabilitadas. Não é necessário conectar sensores ou válvulas.

**Use a versão 0.6.0 atual**, que corrige os endereços de gravação da plataforma. Não use binários antigos 0.5.0. Upload normal preserva as regiões de dados da tabela atual; não gravar firmware.factory.bin inteiro em placa que precisa preservar NVS.

## 1. Atualizar e compilar

Na raiz do repositório atualizado:

```sh
python -m pip install platformio==6.1.19 pyserial
python -m platformio run -d iot/firmware -e esp32_matrix
python -m platformio device list
```

O perfil matriz serve para testar Wi-Fi/portal sem sensores locais. Use esp32_sensor_bench apenas quando for testar sensores conectados diretamente por cabos curtos. O perfil da placa e os parâmetros elétricos continuam sendo definidos na compilação.

O provisionamento USB dispensa criar provision.h para este teste: os padrões não contêm Wi-Fi/senhas nem habilitam saídas. Se já existir provision.h privado, confira IOT_PHYSICAL_OUTPUTS=0. Um perfil prov_v1 já salvo na placa prevalece sobre as credenciais e bindings compilados; não muda por novo upload comum.

Se o download do core falhar com CERTIFICATE_VERIFY_FAILED, o compilador ainda não foi executado. Verifique data/hora do computador e cadeia de certificados do Python/proxy corporativo; mantenha TLS validado. Não altere o SDK fixado para tentar esconder esse erro. Esse bloqueio foi superado na conferência 0.5.1 configurando a confiança nos bundles Python efetivamente usados; veja [CERTIFICADOS_COMPILACAO_ESP32.md](CERTIFICADOS_COMPILACAO_ESP32.md).

## 2. Gravar

Substitua COM5 pela porta listada:

```sh
python -m platformio run -d iot/firmware -e esp32_matrix -t upload --upload-port COM5
```

A gravação normal inclui o portal na flash. Não executar uploadfs ou erase_flash. Em placa com firmware/partições anteriores, avaliar a migração descrita no README antes de gravar; não apagar NVS para resolver problemas de acesso.

## 3. Cadastrar rede e senhas por USB

Feche o monitor Serial do VS Code/PlatformIO e execute:

```sh
python iot/firmware/scripts/provision-device.py --port COM5
```

O assistente pede SSID da rede **2,4 GHz**, senha Wi-Fi, senha admin de pelo menos 12 bytes e senha operador opcional, diferente da admin. Senhas não são exibidas e o assistente não cria arquivo de credenciais. No primeiro cadastro, admin é obrigatório; em atualização, campos secretos vazios mantêm o valor atual.

Depois de coletar os dados, o assistente abre a porta, consulta GUID/revisão e salva. Abertura da porta costuma reiniciar DevKits; se a placa não responder ou informar janela fechada, pressione EN/reset e repita. A gravação USB só é aceita nos primeiros três minutos após reset e com controlador/nó ocioso. Consultas PROVISION? continuam disponíveis depois da janela. O script não executa apagamento ou mudança de GUID.

A placa informa salvamento e reinicia após cerca de 1,5 s. Se o processo retornar erro, os dados não são considerados aplicados; anote apenas o código do erro, sem compartilhar senhas.

## 4. Identificar IP e abrir o portal

Aguarde a conexão e consulte:

```sh
python iot/firmware/scripts/provision-device.py --port COM5 --status
```

Se abrir a porta reiniciar sua placa, a consulta pode ocorrer antes da associação ao Wi-Fi. Nesse caso, consulte o roteador ou abra o monitor e aguarde a mensagem de conexão:

```sh
python -m platformio device monitor -d iot/firmware --port COM5 --baud 115200
```

A mensagem esperada é `WiFi connected; portal http://IP/`. A rede usa DHCP: o IP pode mudar após reinício; não há promessa de hostname mDNS. Computador/celular precisam alcançar a mesma rede, sem isolamento de clientes/rede de convidados.

Abra `http://IP_DA_PLACA/` e entre com usuário `admin` e a senha cadastrada. O portal já está na flash, sem servidor Node na placa e sem Internet para carregar a interface. Sem Internet/SNTP, o relógio pode ficar aguardando sincronização; isso não impede o acesso ao portal. Saídas permanecem desabilitadas neste ensaio.

Confira as abas Operação, Configuração, Sensores, Eventos, Fila e Wi-Fi e acesso. O usuário `operador`, se cadastrado, deve ver somente Operação/Eventos. Na aba Wi-Fi e acesso, os dados atuais mostram SSID/IP/revisão e presença de segredos, nunca seus valores. Salvar reinicia a placa; mudar a senha pode exigir nova autenticação do navegador.

## 5. Recuperar Wi-Fi ou senha esquecida

Conecte por USB, reinicie e use o mesmo assistente dentro da janela. Informe a nova rede/senha admin. Campos omitidos preservam os demais valores. GUID, configurações lógicas, calibrações, ocorrências e histórico não são apagados. Se o registro de provisionamento estiver corrompido, rede/automação ficam bloqueadas e será necessário fornecer novamente todos os dados relevantes, inclusive integração/bindings; o registro antigo não é apagado automaticamente.

Para parâmetros avançados ou remoção explícita de senhas, o assistente aceita `--file CAMINHO_PRIVADO`. O arquivo deve conter somente o objeto settings, com campos desejados: ssid, wifi_password, admin_password, operator_password, portal_url, device_token, portal_ca e bindings. Strings secretas vazias removem o valor; admin não pode ficar vazio. bindings usa arrays de quatro strings em actuators/moisture/temperature; string vazia significa sem vínculo. Use extensão .provision.private.json e mantenha fora do Git/chat. Alterar destino central exige fila vazia; atualizar credenciais do mesmo destino preserva mensagens.

## Registrar o resultado da bancada

Registrar versão, modelo/porta, sucesso/falha do build/upload, tempo de conexão, IP, acesso admin/operador e persistência após desligar/ligar. Nunca enviar senhas, token, CA privada ou arquivo de provisionamento. Testes de relé, sensores, RS485/100 m e TLS com portal central ficam para etapas posteriores.

Na 0.6.0, usar o perfil padrão para o primeiro acesso. `esp32_matrix_hardened` exige preparação prévia de segurança da placa/SDK e bloqueará o boot no ambiente padrão. Backup e migrações: [RECUPERACAO_E_PRODUCAO_ESP32.md](RECUPERACAO_E_PRODUCAO_ESP32.md).
