# Primeiro acesso — firmware 0.7.0: rede própria e portal

31/10/2026. O ESP32 cria uma rede protegida para o primeiro cadastro pelo navegador. Com o roteador configurado, tenta conectar a ele; se a conexão Wi-Fi não ocorrer ou cair por 30 segundos, habilita novamente sua rede própria. Usar saídas físicas desabilitadas no primeiro ensaio.

## 1. Atualizar, compilar e gravar

Na raiz do checkout atualizado:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d iot/firmware -e esp32_matrix
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -d iot/firmware -e esp32_matrix -t upload --upload-port COM6
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor --port COM6 --baud 115200
```

Substituir COM6 pela porta real. Alternativa com Python próprio: instalar `platformio==6.1.19 pyserial` e usar `python -m platformio` com os mesmos argumentos. Matriz é adequada ao teste de Wi-Fi/portal; bancada e nó possuem funções físicas diferentes. Não selecionar hardened no primeiro ensaio: ele exige preparação de segurança ainda separada.

Não é necessário criar `provision.h`. O perfil padrão não contém rede/senhas humanas nem habilita relés. Um perfil NVS válido já salvo prevalece sobre valores compilados. O carregador distinto ProvisionConfig.h preserva a correção Windows da 0.6.1: [diagnóstico de IOT_* indefinido](CORRECAO_BUILD_WINDOWS_V061.md).

Upload normal mantém os endereços verificados (app em 0x50000). Não usar `erase_flash`, `uploadfs` ou gravar `firmware.factory.bin` inteiro em atualização que precisa preservar NVS. Placa com tabela anterior diferente exige avaliação da migração. Para falhas de certificados no computador, ver [diagnóstico de compilação](CERTIFICADOS_COMPILACAO_ESP32.md); não desativar TLS/checksum.

## 2. Conectar à rede criada pelo ESP32

No primeiro boot sem conta cadastrada, o Serial informa:

```text
Initial device WiFi password: <senha individual gerada>
First portal login: admin / same device WiFi password; set permanent admin in portal
Device WiFi ready: ConectaAgro-<identificador>; portal http://192.168.4.1/
```

A senha é aleatória, individual e persistida; não há senha universal de fábrica. Ela é exibida ao ser gerada e em cada boot enquanto o primeiro cadastro estiver pendente; após cadastrar a conta permanente, não é exibida automaticamente nos boots seguintes. Guarde-a e **oculte essa linha antes de compartilhar logs**. Nome e senha poderão ser alterados no portal.

1. No celular/computador, conecte à rede `ConectaAgro-...` informada pelo Serial.
2. Use a senha individual indicada.
3. Se o sistema avisar que não há Internet, mantenha a conexão.
4. Abra **http://192.168.4.1/**. DNS/redirecionamento ajudam na detecção do portal; a abertura automática depende do sistema/navegador. Se não aparecer, digite o endereço HTTP completo.
5. No primeiro cadastro, usuário **admin** e senha **igual à senha inicial da rede do ESP32**.

Esse acesso inicial só funciona pela interface AP, no desenvolvimento e sem perfil persistido/conta anterior. Ele permite somente estado e provisionamento, com CSRF; não libera comandos, edição da irrigação ou histórico. Uma conta existente não é substituída por essa senha inicial.

## 3. Configurar no portal e reiniciar

A aba **Wi-Fi e acesso** abre automaticamente no primeiro cadastro.

1. Informe o SSID do seu roteador **2,4 GHz** e a senha dele. Para roteador aberto, use a opção de rede sem senha.
2. Defina uma **senha admin permanente**, com pelo menos 12 bytes. Operador é opcional e deve ter senha diferente da admin.
3. Em **Rede própria do ESP32**, escolha o nome e, se desejar, uma nova senha de 12–63 caracteres ASCII sem espaços. Campo secreto vazio mantém a senha atual.
4. Se preferir não associar a nenhum roteador, marque **Usar apenas a rede do ESP32**. Essa opção remove explicitamente SSID/senha do roteador, não abre a rede própria sem senha.
5. Clique em **Salvar provisionamento e reiniciar** e aguarde cerca de 1,5 s.

Não é preciso enviar comandos USB para cadastrar SSID/senhas. O Serial é usado inicialmente para conhecer a chave individual e acompanhar o diagnóstico. Alterar nomes/senhas não altera GUID, configuração lógica ou históricos.

## 4. Reabrir o portal após salvar

Se a conexão ao roteador funcionar, o Serial informa:

```text
WiFi connected; portal http://IP_DA_PLACA/
```

Conecte o celular/computador à rede do roteador e abra o IP informado. Ele é obtido por DHCP e pode mudar; não há garantia de hostname mDNS. Entre com `admin` e a **senha permanente cadastrada**, não com a chave temporária inicial. Não é necessário Internet para carregar a interface; agendas/testes continuam sujeitos à validade do relógio e às proteções existentes.

Se a associação falhar, a rede própria abre após 30 segundos. Reconecte usando o nome/senha próprios definidos e abra **http://192.168.4.1/**. No modo somente AP, ela abre sem esperar por roteador. Se você alterou a senha admin, o navegador poderá pedir nova autenticação.

Depois que o roteador volta, a placa mantém o AP enquanto houver clientes conectados. Após 30 segundos de conexão estável e sem clientes no AP, encerra a rede própria. O critério é conexão Wi-Fi ao roteador: uma falha isolada de Internet/servidor central não força o AP se a LAN continua acessível.

## 5. Recuperar acesso

- Roteador indisponível/senha errada: aguarde o AP e ajuste a rede em **Wi-Fi e acesso**, usando a conta admin existente.
- Senha da rede do ESP32 esquecida: no modo desenvolvimento, dentro dos três minutos após reset e com controlador ocioso, envie **WIFI_ACCESS?** pelo Serial. A resposta física inclui nome/senha atuais; não compartilhe essa saída. Fora dessa janela ou no perfil hardened, a consulta de segredo é recusada.
- Senha admin esquecida: recuperação física USB continua disponível. Fechar o monitor e executar, a partir de `iot/firmware`:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" .\scripts\provision-device.py --port COM6
```

O assistente preserva GUID/dados; precisa da janela de três minutos após reset. Campos secretos omitidos mantêm valores atuais. Consulta de estado sem segredos: acrescentar `--status`. O comando **PROVISION?** também retorna metadados sem senhas. Abrir a porta pode reiniciar a placa; aguardar a associação antes de concluir que o IP está ausente.

Registro de provisionamento corrompido não habilita um bypass da senha admin existente: continua exigindo recuperação física. Não apagar NVS para resolver acesso.

## Limites e validação de bancada

O portal usa HTTP Basic/CSRF e a rede própria tem senha individual; isso não conclui a preparação de segurança de produção. Wi-Fi e tentativas de conexão rodam em tarefa separada do controlador. O rádio AP/STA é compartilhado e pode haver interrupções breves de cliente ao mudar canal; validar com os equipamentos reais. Se o roteador também usa 192.168.4.0/24, avaliar conflito de sub-rede antes de operar simultaneamente.

Verificar na placa: boot sem perfil, chave/SSID, primeiro cadastro, reinício, conexão válida, senha de roteador errada, perda/retorno da rede, cliente AP mantido, mudança de nome/senhas, contas e persistência após desligar/ligar. `physical=0` é esperado para este ensaio. Os logs de core dump ausente são distintos da criação de rede; o acesso depende das mensagens AP/Wi-Fi acima.

[Manual ilustrado](manual-esp32/README.md) · [recuperação e produção](RECUPERACAO_E_PRODUCAO_ESP32.md) · [evidências 0.7.0](VALIDACAO_FIRMWARE_V07.md).
