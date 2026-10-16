# Auditoria offline de pré-produção — complemento da 0.6.0

15/10/2026, horário de São Paulo. A entrega adiciona ferramentas de desenvolvimento; o código C++ e o perfil padrão de compilação permanecem na 0.6.0. Não transforma o build Arduino pré-compilado em SDK de produção.

## Executar

Na raiz do repositório, usar Python 3.11 ou posterior, sem dependências extras:

```sh
python iot/firmware/scripts/audit-production.py --sdkconfig CAMINHO_SDKCONFIG_GERADO --partitions iot/firmware/partitions.production-candidate.csv --bootloader CAMINHO_BOOTLOADER_BIN --application CAMINHO_FIRMWARE_BIN --chip-revision-major 3 --flash-size 4M
```

Usar `sdkconfig` resolvido/gerado pelo SDK customizado e as imagens finais, incluindo o tamanho acrescido pela assinatura. Não usar apenas `sdkconfig.defaults`. `--chip-revision-major 3` é exemplo: informar a revisão realmente identificada, não deduzir a partir do nome comercial do módulo. O auditor não conecta USB nem mede a placa; a revisão é declarada. Não lê chaves privadas, executa compilador, gera chaves, grava firmware ou altera eFuses.

A saída JSON contém códigos de conferência e SHA-256 das entradas, sem exibir valores de configuração, caminhos privados ou conteúdo dos arquivos. Retorno **0** significa consistência da configuração/tamanhos fornecidos dentro da política delimitada; **2** significa entrada inválida ou pendência. Ausência de bootloader/aplicativo é pendência, não tamanho zero aceito. `production_ready` é sempre `false`: assinatura das imagens, vínculo binário↔sdkconfig, estado efetivo das chaves/eFuses, criptografia em execução, transporte local e bancada não são verificados.

## Conferências implementadas

- Alvo ESP32 clássico, revisão principal declarada ECO3 ou posterior e mínimo do SDK a partir de ECO3 para a política de secure boot v2.
- Secure boot/v2, flash encryption e modo Release, NVS encryption; recusa modo Development e opções permissivas de JTAG/ROM BASIC/encriptação, decriptação ou cache pela UART.
- Flash declarada de 4 MiB, coerente com o SDK, partições com offsets explícitos, alinhamentos, nomes únicos, limites e ausência de sobreposição.
- Região NVS atual 0x9000/0x40000, aplicativo factory em 0x50000 e reserva de arquivos em 0x300000/0x100000. Deslocar tabela/partições é pendência de migração explícita, nunca correção silenciosa.
- Uma partição `nvs_keys` de pelo menos 4 KiB, marcada encrypted. A NVS usa sua própria cifragem e não recebe essa flag diretamente.
- Bootloader completo cabe entre 0x1000 e a tabela em 0x8000; aplicativo cabe em sua partição, cujo tamanho precisa ser múltiplo de 64 KiB para a política de secure boot.

O auditor não é um validador genérico de todas as famílias/variantes IDF: limita-se à política ESP32 clássico/4 MiB e símbolos da versão 5.5.4. Outras políticas precisam de revisão. Não recomenda opções de liberação insegura para fazer uma configuração passar.

## Tabela candidata separada

`iot/firmware/partitions.production-candidate.csv` acrescenta `nvs_keys` em **0x49000**, com 0x1000 bytes, no intervalo não alocado entre o fim da NVS em 0x49000 e o aplicativo em 0x50000. Mantém nome/subtipo/regiões das três partições existentes e o tamanho atual do aplicativo. A NVS não foi reduzida. A configuração padrão `platformio.ini` continua apontando para `partitions.csv`, sem chaves.

A candidata foi convertida para binário e novamente para CSV com `gen_esp32part.py` do ESP-IDF **v5.5.4**, usando `--flash-size 4MB --secure v2 --offset 0x8000`; verificação aprovada. Persiste o aviso conhecido de nome littlefs com subtipo legado spiffs: foi preservado para não alterar a região existente. Esse teste verifica a tabela, não o bootloader real. Se o bootloader assinado crescer além do espaço, a candidata não será suficiente: o auditor recusará e será preciso planejar migração física sem apagar GUID/histórico.

**Não selecionar/gravar essa tabela como atualização comum.** A criação e proteção das chaves, inicialização/migração da NVS existente e recuperação precisam ser implementadas no SDK e ensaiadas em placa reservada. Acrescentar a partição não cifra dados previamente existentes.

## Testes e próximo desenvolvimento

16 testes Python: configuração sintética consistente sem declarar produção pronta; tabela atual recusada sem alteração; falta/desativação das proteções; opções inseguras individuais; chip/revisão/alvo/flash; preservação de regiões, sobreposição, flags, bootloader/aplicativo no limite e acima dele, tamanho de página, offset da tabela, CSV inválido/duplicado/implícito, relatório sem segredo e entradas limitadas. Integrados ao script nativo usado pelo CI.

Regressão desta entrega: 14 executáveis C++/UBSan e cinco fixtures de bytes/hash; sete testes Python anteriores de USB/layout; 16 do auditor. Gerador oficial: candidata↔binário com conferência dos campos preservados. Não foi repetido build ESP32: nenhum fonte C++/SDK/portal padrão mudou; os sete builds da 0.6.0 continuam sendo evidência da entrega anterior. Sem ensaio visual novo, acesso USB, upload ou teste físico.

FW-06 continua parcial. Próximos itens de desenvolvimento: construir integração reproduzível de SDK/bootloader com NVS cifrada, provisionamento inicial e migração, custódia/rotação de chaves e recuperação autenticada sem o bypass USB de desenvolvimento; definir transporte protegido para o portal local. A revisão real do chip e a estratégia de atualização/recuperação precisam orientar essa integração. O primeiro teste de Wi-Fi/portal do usuário continua usando o perfil padrão, com saídas desabilitadas.

Referências oficiais da versão:

- [Fluxos de habilitação de segurança](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/security/security-features-enablement-workflows.html).
- [Flash encryption e tamanho do bootloader](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/security/flash-encryption.html).
- [Kconfig do bootloader, v5.5.4](https://github.com/espressif/esp-idf/blob/v5.5.4/components/bootloader/Kconfig.projbuild).
- [Kconfig da NVS, v5.5.4](https://github.com/espressif/esp-idf/blob/v5.5.4/components/nvs_flash/Kconfig).
- [Gerador oficial de partições, v5.5.4](https://github.com/espressif/esp-idf/blob/v5.5.4/components/partition_table/gen_esp32part.py).
