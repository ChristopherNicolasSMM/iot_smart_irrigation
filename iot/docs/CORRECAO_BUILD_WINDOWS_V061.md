# Firmware 0.6.1 — correção de IOT_* indefinido no Windows

31/10/2026. Erro relatado durante `esp32_matrix` com Arduino-ESP32 3.3.8/IDF 5.5.4. O compilador chegava a `src/main.cpp` e recusava todas as definições de provisionamento: IOT_ACTUATOR_ID_*, IOT_WIFI_SSID, IOT_PHYSICAL_OUTPUTS, IOT_RS485_* e demais IOT_*.

## Causa e correção

O cabeçalho versionado `include/Provision.h` procurava o arquivo opcional privado `include/provision.h`. Esses nomes colidem em sistemas de arquivos do Windows que não distinguem maiúsculas/minúsculas. Assim, `__has_include("provision.h")` encontrava o próprio carregador; o `#pragma once` interrompia a inclusão repetida e o ramo de padrões era pulado. As macros ficavam indefinidas. Os builds anteriores em Linux não identificavam essa diferença.

Na 0.6.1:

- Carregador versionado renomeado para **ProvisionConfig.h**, com inclusão atualizada no `main.cpp`. O antigo Provision.h foi removido do Git; o arquivo privado continua sendo `provision.h`, ignorado pelo Git.
- O carregador inclui o arquivo privado, quando existe, e completa valores ausentes com padrões protegidos por `#ifndef`. Arquivos privados parciais ou vazios deixam de suprimir todas as definições.
- Valores informados explicitamente são preservados. Os padrões continuam com Wi-Fi/segredos/bindings vazios e saídas/RS485 desabilitados.
- Acrescentado job `windows-build` no CI, além dos testes de cabeçalhos e builds Linux. A existência do job não equivale a uma execução Windows já aprovada; conferir a execução correspondente no GitHub Actions.

GUID, perfil NVS salvo, partições e histórico não são migrados/apagados por essa alteração. O perfil NVS válido continua prevalecendo sobre os dados compilados. Não foi alterada a configuração de caminhos longos do Windows.

O aviso `Windows Long Path Support is disabled` e o aviso legado `littlefs`/subtipo spiffs não causaram os erros IOT_* mostrados. São avisos distintos; não é necessário alterar o Registro do Windows para corrigir esse conflito de cabeçalhos.

## Atualização recomendada para o log enviado

O log usa chamadas diretas a IOT_PORTAL_URL/IOT_ADMIN_PASSWORD que já foram substituídas por provisionamento em runtime nas versões mais recentes. Portanto, também está sendo compilada uma cópia anterior do `main.cpp`. Atualizar somente `platformio.ini` ou trocar um arquivo isolado não garante um firmware consistente.

Para evitar conflito com a cópia antiga ou com um arquivo privado que possa ter ocupado o nome Provision.h no Windows, a alternativa mais simples é um **checkout novo**, mantendo a pasta antiga intacta. No PowerShell:

```powershell
Set-Location $env:USERPROFILE
git clone https://github.com/ChristopherNicolasSMM/iot_smart_irrigation.git iot-irrigacao-corrigido
Set-Location .\iot-irrigacao-corrigido
python -m pip install platformio==6.1.19 pyserial
python -m platformio run -d iot/firmware -e esp32_matrix
```

Se esse nome de pasta já existir, escolha outro nome para o clone. Não apague a pasta existente. Abra a pasta nova no VS Code e confira se o PlatformIO está usando `iot/firmware/platformio.ini` dessa cópia.

Para o primeiro teste de Wi-Fi/portal, não é necessário copiar nenhum `provision.h`: compilar os padrões e cadastrar rede/senhas pelo assistente USB. Consulte [PRIMEIRO_ACESSO_ESP32.md](PRIMEIRO_ACESSO_ESP32.md). Não copiar binários 0.5.0 antigos, não usar `erase_flash` e não gravar `firmware.factory.bin` inteiro como atualização preservando NVS.

### Atualizar um checkout existente

Se não há alterações locais nem arquivo privado sobrepondo o carregador antigo, executar na raiz do repositório:

```powershell
git status --short
git pull --ff-only
python -m platformio run -d iot/firmware -e esp32_matrix -t clean
python -m platformio run -d iot/firmware -e esp32_matrix
```

`clean` remove resultados de compilação locais; não conecta nem apaga a placa. O `pull` pode recusar alterações locais — preserve-as antes de resolver o conflito. **Não usar `git reset --hard`, `git clean -fdx` ou restaurar Provision.h indiscriminadamente para contornar a recusa**: no Windows, esse arquivo pode ter sido sobrescrito com seus dados privados.

Caso use provisionamento compilado, preserve o arquivo privado fora do repositório antes de atualizar e revise-o localmente. Não envie seu conteúdo no chat/Git. A partir da correção, o carregador é ProvisionConfig.h e os dados privados ficam exclusivamente em provision.h. Um arquivo que contém apenas o antigo carregador não é um cadastro de Wi-Fi/senhas. Para bancada inicial, mantenha saídas desabilitadas.

## Verificação e evidências

- Falha original reproduzida no host por dois nomes que resolvem ao mesmo arquivo: o carregador antigo pulou os padrões e IOT_PHYSICAL_OUTPUTS ficou indefinido.
- Sete testes novos compilam os cabeçalhos reais com g++: ausência de privado, privado vazio, privado parcial, overrides físicos/bindings explícitos, arquivo que inclui o exemplo antes dos overrides, alias do carregador legado e nome distinto do carregador no main.
- Regressão: 14 executáveis C++/UBSan e cinco fixtures de bytes/hash da configuração; testes USB/layout/auditoria de produção integrados ao script nativo.
- Sete builds alvo aprovados em Linux: matriz/bancada/nó padrão e sintético, mais hardened. Offset 0x50000, imagens auxiliares fora das partições de dados e aplicativo combinado verificados. Recursos em [BUILD_FIRMWARE_V061.json](BUILD_FIRMWARE_V061.json). A reprodução por alias em Linux é evidência da resolução dos cabeçalhos, não uma execução nativa do Windows.

Depois do upload, a identificação esperada é **firmware=0.6.1** no Serial e **0.6.1-experimental** no estado do portal. A versão do SDK permanece fixa; o portal embutido conserva as telas do [manual ilustrado](manual-esp32/README.md). Nenhuma placa foi gravada/testada por esta conferência. Wi-Fi, USB, saídas e persistência reais continuam dependendo da bancada do usuário.
