# Firmware experimental 0.3.0 — tempo de 64 bits

15/10/2026. Primeira entrega de aprimoramento anterior à bancada.

## Causa e correção

A configuração anterior usava Arduino-ESP32 2.0.17 / ESP-IDF 4.4, com time_t de 32 bits. Havia também restrições explícitas a 2037 nas datas, INT32_MAX no relógio e nos checkpoints. Trocar apenas variáveis locais por int64 não corrigiria libc, SNTP ou TLS.

A plataforma foi fixada em pioarduino 55.03.38-1 (comunitária), que fornece o core oficial Arduino-ESP32 3.3.8 / ESP-IDF 5.5.4. PlatformIO Core 6.1.19. Não usa alias stable/latest. A migração ajusta SHA-256 para Mbed TLS 3 e o callback SNTP para a API esp_sntp. Versões de ArduinoJson, OneWire e DallasTemperature mantidas; compilação deve confirmar compatibilidade.

TimePolicy.h exige time_t assinado de ao menos 64 bits, e main.cpp exige ESP-IDF 5+. Qualquer regressão para uma base de tempo de 32 bits falha na compilação, inclusive se as funções de rede forem otimizadas por ausência de provisionamento.

Mesma política de datas nos validadores de calendário, relógio e checkpoint: anos ISO de quatro dígitos, 2020–9999; epoch entre 1577836800 e 253402300799. Diagnóstico informa time_t_bits/calendar_max_year. Mantidos UTC/BRT fixo e necessidade de sincronização após boot; uma política de fusos global é outra entrega.

**O calendário até 9999 não é garantia de SNTP até 9999.** A documentação do SDK descreve convenções de era NTP no intervalo 1968–2104. Esta entrega remove o estouro Unix de 2038; não implementa resolução de eras NTP arbitrárias nem promete autonomia de décadas sem manutenção, certificados válidos ou relógio confiável.

## Compatibilidade

GUID, esquema do portal, hash de configuração, nomes/chaves de NVS e tabela de partições preservados. Checkpoints já eram números int64; passam a aceitar epochs além de INT32_MAX. Não apagar dados nem executar erase_flash para atualizar. A mudança de framework ainda precisa de ensaio físico de NVS, sensores, GPIO, TLS e memória. O aviso de partições 0.1→0.2 continua aplicável a quem já usava aquela versão.

## Evidências

- Seis executáveis C++/UBSan/-Wall/-Wextra/-Werror e cinco fixtures de bytes/hash aprovados.
- Fronteira exata 2038-01-19 03:14:07→03:14:08 UTC; rejeição de epoch negativo resultante de wrap de 32 bits.
- Datas, agenda e recuperação em 2038, 2040, 2100, 2400 e 9999. Bissextos 2040/2400 aceitos; 2100-02-29 rejeitado. ISO no limite superior e limites de ano/epoch.
- Janela que cruza o instante de estouro antigo, UTC/BRT e recuperação de data específica após 60 anos.
- Sintaxe do portal central, 26 fixtures de contrato e 269 testes aprovados.
- Builds ESP32 bloqueados antes da compilação: o download do core Arduino 3.3.8 terminou com `SSLCertVerificationError: certificate verify failed: unable to get local issuer certificate`. A tentativa com bundle local de autoridades confiáveis combinado também falhou. A verificação TLS não foi desabilitada. PlatformIO 6.1.19, a plataforma fixada e tool-esp_install chegaram a ser instalados; nenhum dos seis builds foi confirmado nesta entrega. Testes host não comprovam libc/SNTP/TLS na placa.

Comando para reproduzir em ambiente com PlatformIO 6.1.19 e acesso aos downloads:

```sh
python iot/firmware/scripts/check-build.py
```

Nesta execução, usou-se `PYTHONPATH=/tmp/irrigation-platformio-v03` para a instalação temporária do PlatformIO. O script terminou com código 1 na dependência `https://github.com/espressif/arduino-esp32/releases/download/3.3.8/esp32-core-3.3.8.tar.xz`, antes de compilar o programa.

O script scripts/check-build.py compila os três perfis padrão e os três com provisionamento sintético em cópia temporária. Não lê nem sobrescreve o provisionamento do usuário, não faz upload e descarta os binários. O CI usa esse script para alcançar também os caminhos de rede/saída normalmente eliminados no build padrão.

## Fontes técnicas

- [Espressif: relógio, SNTP e time_t de 64 bits desde ESP-IDF 5](https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32/api-reference/system/system_time.html)
- [Espressif: migração Arduino 2.x→3.x](https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html)
- [Manifesto da plataforma fixada](https://github.com/pioarduino/platform-espressif32/blob/55.03.38-1/platform.json)
