# Firmware experimental 0.5.1 — preparação da gravação

15/10/2026. Correção identificada ao conseguir executar o build alvo após resolver a cadeia de certificados do computador. Não há placa conectada e não foi realizado upload.

## Problema encontrado

A plataforma fixada pioarduino 55.03.38-1 identifica o endereço do aplicativo pela partição ota_0, assumindo 0x10000 na ausência dela. Este projeto usa uma única partição factory em 0x50000 para manter NVS de 256 KiB. O build inicial da 0.5.0 compilou, mas preparou a imagem combinada/upload com aplicativo em 0x10000 e boot_app0.bin em 0xe000. Ambos endereços caem na região NVS. Esses binários não devem ser usados para gravar a placa.

A correção é no projeto: post-script flash-layout.py mantém a tabela de partições existente, define aplicativo em 0x50000 e remove boot_app0.bin da lista de imagens e dos argumentos de upload. Reaplica o offset após a ação checkprogsize da plataforma, que antes o redefinia para o padrão. Dados/GUID não são apagados para contornar o problema. Não foi editado o código da plataforma instalada nem desativada verificação de integridade.

Foi removida também a expressão com initializer_list temporária que gerava aviso de referência possivelmente pendente na serialização dos bindings; arrays explícitos mantêm o formato e a ordem dos campos.

## Verificação automatizada

O script check-build.py verifica cada perfil, padrão e sintético: flash-layout.json, offset 0x50000, ausência de boot_app0 nos argumentos, imagens auxiliares fora das partições de dados e correspondência byte a byte do aplicativo na posição correta da imagem combinada. Três testes Python reproduzem a região NVS, a redefinição tardia do offset e a recusa de layout OTA inesperado. O layout não é migrado silenciosamente.

A imagem firmware.factory.bin inclui preenchimento entre imagens. Ela não é uma atualização que preserva NVS: gravar o arquivo combinado inteiro pode apagar dados mesmo quando o preenchimento é 0xff. Para atualização, usar o upload normal do PlatformIO, que grava somente bootloader/tabela/aplicativo nos endereços verificados. Não executar erase_flash. Migração de uma tabela antiga continua exigindo avaliação/backup.

## Certificados e limites

Core 3.3.8, bibliotecas IDF 5.5.4 e compilador foram obtidos com TLS validado após configurar as autoridades já confiáveis do sistema nos bundles Python efetivamente usados. O primeiro espelho SCons foi rejeitado por checksum divergente; outro espelho passou pela verificação e foi instalado. Não foram desativadas verificações TLS/checksum ou alteradas versões para esconder o erro. [Diagnóstico de certificados](CERTIFICADOS_COMPILACAO_ESP32.md).

Build não valida boot, Wi-Fi, USB, Basic Auth, NVS/TLS em execução, estabilidade ou circuitos. RAM/flash de link não medem heap/pilha em uso. Saídas padrão continuam desabilitadas. O próximo marco continua sendo o ensaio do usuário descrito em [PRIMEIRO_ACESSO_ESP32.md](PRIMEIRO_ACESSO_ESP32.md).

## Resultado final da conferência

Seis builds aprovados: matriz, bancada e nó, cada um padrão e com provisionamento sintético. Verificações dos argumentos/imagens e dos bytes em 0x50000 aprovadas nos seis. Assert de time_t assinado de 64 bits compilado no alvo. Nenhum upload ou binário retido. Recursos do link registrados em BUILD_FIRMWARE_V051.json:

| Perfil | Variante | RAM estática | Aplicativo na flash |
|---|---|---:|---:|
| esp32_matrix | default | 66792 B | 1258368 B |
| esp32_sensor_bench | default | 66936 B | 1279012 B |
| esp32_remote_node | default | 61972 B | 1152021 B |
| esp32_matrix | synthetic_compile_only | 66864 B | 1265608 B |
| esp32_sensor_bench | synthetic_compile_only | 66936 B | 1279648 B |
| esp32_remote_node | synthetic_compile_only | 61972 B | 1152965 B |

Limites de link: 327680 B de RAM e 2818048 B para aplicativo; não medem heap/pilha durante Wi-Fi/TLS. Permanecem avisos de macro GPIO unsigned da dependência OneWire 2.3.8 e de nome littlefs com subtipo legado spiffs/0x82; não foram silenciados. A partição de arquivos está reservada, não é usada pelo portal e foi mantida para não migrar a tabela nesta correção. O aviso dos vínculos no código do projeto foi removido.

Regressão: dez executáveis C++/UBSan, cinco fixtures de bytes/hash, adaptador ArduinoJson real no host, quatro testes Python do protocolo USB e três novos testes de layout, 283 testes Node, sintaxe e 26 fixtures/consistência OpenAPI aprovados. A interface não recebeu alterações nesta correção; não há novo ensaio visual ou físico.
