# Base de fusos embutida

IANA tzdata **2026e**, 597 nomes/aliases e 344 tabelas deduplicadas. Histórico retido desde 01/01/2019 UTC para cobrir a política civil do projeto (2020–9999). Após a última transição explícita, regras POSIX recorrentes do próprio TZif são interpretadas pelo firmware, incluindo transições negativas, acima de 24h e deslocamentos de meia hora/45 minutos. Não depende do parser POSIX da Newlib para agendamento. Datas anteriores ao início da tabela são recusadas.

Horário inexistente na entrada do DST não é normalizado para outro horário: a ocorrência é omitida. Em horário repetido, escolhe o primeiro instante; o segundo não reabre a janela. Duração continua monotônica. Regras futuras representam a legislação conhecida nesta versão: mudanças legais exigem atualizar a base/recompilar. Não existe corte em 2037; a garantia do SNTP continua limitada pelas convenções do SDK (ver TimePolicy).

Fonte: https://www.iana.org/time-zones/releases/tzdata2026e.tar.gz
SHA-256: `b26882805f26aac59d5b222978e6580484b834ccdc98be89df2f05a6dc53a652`.
Licença pública original em LICENSE. A geração não depende de serviço online em execução.

Reprodução, na raiz do projeto, com `zic` instalado e arquivo verificado extraído em DIRETORIO_FONTE:

```sh
mkdir -p DIRETORIO_COMPILADO
zic -d DIRETORIO_COMPILADO DIRETORIO_FONTE/africa DIRETORIO_FONTE/antarctica DIRETORIO_FONTE/asia DIRETORIO_FONTE/australasia DIRETORIO_FONTE/europe DIRETORIO_FONTE/northamerica DIRETORIO_FONTE/southamerica DIRETORIO_FONTE/etcetera DIRETORIO_FONTE/backward
python iot/firmware/scripts/generate-timezones.py DIRETORIO_COMPILADO 2026e
python iot/firmware/scripts/test-timezones.py DIRETORIO_COMPILADO
```

O teste diferencial usa ZoneInfo lendo esses mesmos TZif, nunca a base potencialmente antiga do sistema. Conferência 0.6.0: 163.755 instantes em todos os nomes, fronteiras explícitas ±1 segundo, meses de 2020/2026/2037/2038/2040/2100/2400/9998 e limites recorrentes futuros. Casos de gap/fold e ano 9999 também têm testes C++ dedicados. Esses testes host não medem tempo de execução/heap no ESP32.
