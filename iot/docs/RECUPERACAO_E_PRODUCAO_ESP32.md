# Recuperação e proteção — firmware 0.6.0 experimental

## Backup de configuração e provisionamento

O utilitário `iot/firmware/scripts/backup-device.py` gera arquivo `.irrfw` com AES-256-GCM, salt/nonce aleatórios e chave derivada da senha externa por PBKDF2-HMAC-SHA256 (600.000 iterações). A senha é solicitada sem eco, não é gravada na placa e não entra na linha de comando. Cabeçalho/GUID são autenticados; GUID e parâmetros criptográficos ficam visíveis. O arquivo é criado com exclusividade, permissão 0600 em sistemas compatíveis e sem arquivo temporário com segredos em claro. Guardar senha separadamente. Não versionar backups.

Instalação e uso na raiz do projeto:

```sh
python -m pip install pyserial==3.5 cryptography==50.0.2
python iot/firmware/scripts/backup-device.py backup --port PORTA_USB --file dispositivo.irrfw
python iot/firmware/scripts/backup-device.py restore --port PORTA_USB --file dispositivo.irrfw
python iot/firmware/scripts/backup-device.py accept --port PORTA_USB
```

Fechar o monitor serial e reiniciar a placa antes de cada operação. A janela USB de manutenção é de três minutos; operações exigem controlador ocioso e saídas conhecidas quando habilitadas. O utilitário abre a porta a 115200. A conexão USB transporta o snapshot em claro antes da cifragem no computador; exige acesso físico confiável. O terminal normal não exporta segredos espontaneamente. Não usar captura de sessão serial durante backup.

O snapshot contém **provisionamento completo com segredos**, envelope lógico de configuração e quatro calibrações/vínculos ROM. Não é imagem completa da NVS: não exporta fila, ocorrências, comandos ou auditoria. Esses registros existentes e o GUID são preservados na recuperação. Limite de snapshot/packet 32.256 bytes; a placa também limita o manifesto persistido a 32.768 bytes e recusa excesso/erro de alocação. O utilitário nunca sobrescreve arquivo existente.

A restauração exige o mesmo GUID, revisões atuais na requisição e configuração válida/hash correto. Recusa configuração mais antiga ou conteúdo diferente com a mesma revisão. Backup sem configuração não apaga a atual. Troca de destino exige fila vazia; registros anteriores não são reenviados automaticamente para o novo destino. Não permite clonar identidade em outra placa nem restaura NVS apagada.

O manifesto `restore_v1` é validado e persistido antes de reiniciar. No boot, `recover_hold` impede automação/rede central e o manifesto permanece até todas as gravações terminarem; nova tentativa repete as gravações de forma idempotente após interrupção. Falha/corrupção mantém quarentena. Não é rollback transacional de todas as chaves NVS: pode haver gravação parcial, sempre com retomada e sem liberar saídas.

Após restaurar, conferir os dados no portal local e gerar novo token no portal central. O endpoint central existente `POST /api/device/credential` emite token uma única vez e revoga imediatamente o anterior. Cadastrar esse token na aba **Wi-Fi e acesso** e aguardar o reinício. Depois usar **Conferi os dados: liberar recuperação e reiniciar**, ou `accept` por USB. Com integração central ativa, a liberação recusa reutilizar o token contido no backup. O firmware verifica mudança do token; a confirmação da validade/revogação no servidor depende da integração real. Sem integração, exige apenas liberação explícita.

`abort --port PORTA_USB` remove um manifesto pendente na janela física e reinicia mantendo quarentena. Não desfaz chaves já gravadas. Corrigir/reprovisionar e conferir antes de liberar. Nunca apagar NVS como solução de rotina.

## Migrações aditivas

- `prov_v1` mantém schema e revisão próprios, com `audit_origin_floor` opcional (zero em registros legados). Nenhuma mudança de GUID ou partições.
- `queue_v2` substitui a leitura de `qmeta` após a primeira alteração da fila. Mensagens `q0`…`q15` existentes são preservadas; `qmeta` legado permanece, sem ser atualizado. Registro novo inválido bloqueia a fila, sem zerá-la.
- `queue_v2` reúne posição/contagem/geração, total cancelado e oito cancelamentos recentes num único commit NVS. Cancelar exige digest da cabeça exibida, motivo de 1–120 bytes, admin/CSRF e nenhum envio em andamento. Avanço e auditoria são atômicos; só depois a chave da mensagem é removida. Não existe limpeza em massa.
- `replay_v1` reúne cursor e perdas por rotação; migra `areplay`/`audit_lost` se encontrados. Eventos legados sem ID/timestamp válido não recebem identidade ou horário inventados.
- Não fazer downgrade direto para 0.5.x depois de usar a fila 0.6.0: a versão antiga ignora `queue_v2` e pode ler metadados obsoletos. Não foi implementada migração reversa. Migração de tabelas físicas diferentes continua pendente de bancada.

## Segredos na placa e perfil de produção

**O perfil de desenvolvimento continua sem garantia de cifragem dos segredos na flash/NVS.** Backup cifrado no computador e HTTPS para o portal central não cifram automaticamente a NVS. O portal local continua HTTP Basic em LAN confiável; precisa de solução de transporte/rede protegida para produção.

`esp32_matrix_hardened` é um perfil de compilação com `IOT_REQUIRE_HARDENED_BOOT=1`. Antes de carregar segredos ou iniciar rede/automação, exige flash encryption, secure boot e NVS encryption com partição de chaves cifrada/configuração válida. Caso contrário, para com `security_requirements_unmet`. Não exporta nem altera provisionamento por USB nesse modo. O diagnóstico normal informa as proteções detectadas.

**Esse perfil não ativa criptografia nem grava eFuses.** O SDK padrão e a tabela atual não fornecem a preparação completa de NVS cifrada. A implantação segura ainda exige selecionar/revisar a revisão exata do chip, preparar SDK/bootloader/partição `nvs_keys`, política e custódia das chaves, provisionamento inicial, recuperação autorizada e ensaiar o fluxo em placa reservada. Secure boot e flash encryption têm efeitos persistentes e requisitos específicos do ESP32; não foram ativados remotamente.

Referência da versão usada: [Espressif — flash encryption, IDF 5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/security/flash-encryption.html). NVS encryption e secure boot são mecanismos distintos. FW-06 de produção permanece parcial até essa integração e a validação física; o bloqueio compilado evita tratá-la como concluída.

Auditoria offline e tabela candidata não ativada: [AUDITORIA_PREPRODUCAO_ESP32.md](AUDITORIA_PREPRODUCAO_ESP32.md). O auditor verifica a configuração/tamanhos fornecidos, não confirma prontidão de produção nem cifra a placa.
