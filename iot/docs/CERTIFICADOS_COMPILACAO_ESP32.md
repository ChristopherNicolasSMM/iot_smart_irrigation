# Certificados durante o build ESP32

O erro `SSLCertVerificationError: unable to get local issuer certificate` ao baixar o core ocorre no computador de compilação, antes de compilar o firmware. Não identifica defeito na placa ou problema no Wi-Fi do ESP32.

Em 15/10/2026 o download do core Arduino-ESP32 3.3.8 e das bibliotecas IDF 5.5.4 foi liberado neste ambiente mantendo TLS validado. A cadeia de confiança do Python usado pelo build não incluía todas as autoridades já instaladas e confiáveis no sistema. O pioarduino também configura SSL_CERT_FILE/REQUESTS_CA_BUNDLE com o caminho do certifi efetivo do seu Python; definir apenas a variável externa pode não bastar.

A correção local acrescentou as autoridades **já confiáveis no sistema** aos pacotes certifi isolados usados pelo launcher PlatformIO e pelo Python do pioarduino. Não foram alterados o SDK, a plataforma fixada, a validação TLS ou os certificados do firmware. Nenhum certificado específico deste ambiente deve ser copiado para a placa ou para o repositório.

Para diagnosticar no seu computador:

1. Confira data/hora, proxy e eventual inspeção HTTPS por antivírus/rede corporativa.
2. Identifique o Python efetivamente usado pelo PlatformIO e pelo pioarduino. VS Code e terminal podem usar instalações diferentes.
3. Confira a origem da confiança: `python -c "import ssl; print(ssl.get_default_verify_paths())"` e, no Python usado pela ferramenta, `python -c "import certifi; print(certifi.where())"`.
4. Se a rede usa uma autoridade adicional, obtenha-a da administração da rede e configure-a no ambiente de build. A autoridade precisa ser confiável por uma razão independente do download que está falhando. Não aceitar o certificado apresentado pela conexão sem conferir sua origem.
5. Repita o build e confira se o download passou e se o compilador realmente foi executado. Atualizações do certifi/ambiente podem exigir refazer a configuração de confiança.

Não desative verificação TLS nem use `verify=False`/strict SSL desabilitado. Um download HTTPS comum funcionar não prova que o Python isolado tem a mesma cadeia de confiança.

A CA do build é separada de `portal_ca`: a segunda é configurada no ESP32 para validar o portal central. Ela não resolve downloads do PlatformIO e não é necessária para abrir o portal local por HTTP na LAN.

O roteiro de bancada continua em [PRIMEIRO_ACESSO_ESP32.md](PRIMEIRO_ACESSO_ESP32.md). Downloads concluídos não equivalem a build aprovado, e build aprovado não equivale a teste físico de Wi-Fi/portal.
