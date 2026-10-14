# Operação segura — v0.12

Estas ferramentas atendem portal e cliente Node do piloto. O firmware do ESP32 ainda não foi entregue. Não use exemplos com credenciais de demonstração em uma implantação real.

## Chave externa e banco protegido

`VAULT_KEY_FILE` aponta para um JSON gerado pelo comando abaixo: chave aleatória de 256 bits, identificador e versão. O arquivo fica fora do banco e dos backups SQLite. Em POSIX, o modo deve ser 0600; no Windows, configure ACL para a conta do serviço. O programa nunca imprime o material da chave. As pastas secrets/ e data/ e arquivos *.vault-key.json e *.irrbak estão ignorados pelo Git.

Exemplo Bash para **banco novo**:

```bash
mkdir -p secrets data
npm run secrets -- keygen secrets/portal.vault-key.json
export VAULT_KEY_FILE="$PWD/secrets/portal.vault-key.json"
export VAULT_REQUIRED=1
export DB_PATH="$PWD/data/new-protected.sqlite"
# Defina ADMIN_PASSWORD e OPERATOR_PASSWORD por meio de seu gerenciador de segredos.
npm start
```

Para usar dados existentes, siga a migração, não o exemplo de banco novo. Portal e cada cliente devem ter chaves próprias na implantação. O exemplo local usa uma pasta ignorada pelo Git; em produção, use caminho fora do checkout, permissões do serviço e cópia segura da chave em outro local. Arquivo de chave e backup devem ter acessos separados. `/health` informa apenas `secrets_protected`, sem chave, identificador ou caminho.

`VAULT_REQUIRED=1` rejeita execução sem chave. Compatibilidade: bancos antigos continuam abrindo em modo de demonstração sem proteção, quando essa exigência está desligada. Banco já protegido sempre exige a chave correta, independentemente dessa variável; não existe downgrade automático. Definir a chave sobre banco legado causa erro e pede migração, evitando mistura silenciosa de dados em claro/cifrados.

## Migração do banco existente

Pare portal e cliente. Em dispositivos reais futuros, confirme saídas desabilitadas/ociosas antes de interromper o serviço; este Node não controla hardware. Faça backup e mantenha a origem. Uma cópia migrada não inclui mudanças feitas na origem depois do snapshot.

```bash
# Com portal parado, após gerar a chave e exportar VAULT_KEY_FILE:
npm run secrets -- migrate data/pilot.sqlite data/pilot-protected.sqlite
export DB_PATH="$PWD/data/pilot-protected.sqlite"
npm start
```

Migração captura SQLite/WAL com `VACUUM INTO`, valida integridade, cifra em transação no novo arquivo, usa secure_delete e VACUUM na cópia antes de publicá-la. Não altera a origem, GUIDs, histórico, vínculos, estados da fila ou tentativas. Exige destino novo sem WAL/SHM e publica por hard link exclusivo. O arquivo final tem modo 0600 em POSIX. Não apagar a origem durante validação.

Banco do cliente usa o mesmo comando, com chave própria e `CLIENT_DB_PATH` apontando para a cópia migrada. `DEVICE_TOKEN` continua sendo fornecido externamente, nunca pela chave do portal nem por senha humana.

## Rotação

Pare o processo que usa o banco. `VAULT_KEY_FILE` deve apontar para a chave atual. Gere outra chave e um novo banco:

```bash
npm run secrets -- keygen secrets/portal-next.vault-key.json
export VAULT_NEXT_KEY_FILE="$PWD/secrets/portal-next.vault-key.json"
npm run secrets -- rotate data/pilot-protected.sqlite data/pilot-rotated.sqlite
export VAULT_KEY_FILE="$VAULT_NEXT_KEY_FILE"
export DB_PATH="$PWD/data/pilot-rotated.sqlite"
unset VAULT_NEXT_KEY_FILE
npm start
```

Todos os campos protegidos da cópia são decifrados com a chave anterior e cifrados com a nova. O banco original permanece utilizável com a chave anterior. Retenha chaves antigas enquanto houver backups/bancos que dependam delas; testar a recuperação antes de descartar qualquer chave. Rotação do armazenamento não troca senha humana ou token de API: são operações distintas.

## O que está protegido

AES-256-GCM com nonce aleatório de 12 bytes e tag de 16 bytes por gravação. O contexto autenticado inclui tabela, identidade da linha e coluna; copiar um ciphertext para outra linha/coluna é rejeitado. Protege:

- state.body do portal, incluindo perfis de integração e valores constantes em URL/template;
- controller.body do cliente;
- integration_headers.body;
- outbox.body, outbox.profile e outbox.headers, incluindo snapshots antigos não retidos.

Metadados para consulta/fila, configurações de irrigação, histórico, auditorias, hashes de senhas/tokens e ACKs continuam em claro. Não é criptografia integral do SQLite. Não coloque segredos em eventos, nomes de componentes ou dados históricos; use cabeçalhos de autenticação. O portal devolve nomes dos cabeçalhos, nunca seus valores. URLs/templates são editáveis e visíveis aos usuários que consultam configuração; não use esses campos para credenciais.

A proteção não cobre processo em execução, memória, host comprometido, logs externos, cópias antigas em claro ou permissões do disco. A migração preserva a origem legada, que precisa de acesso restrito. Não garante eliminação física em SSD/snapshots. O arquivo da chave precisa de backup separado e seguro; perder a chave impede abrir o banco e recuperar backups correspondentes. Não há KMS automático nem auditoria criptográfica independente nesta entrega.

## Backup e restauração do portal ou cliente

Mesmo formato IRRBACK1 da v0.11, agora com tipo portal/client identificado automaticamente. Snapshot até 64 MiB. Backup tem sua própria senha de 16–1024 caracteres; ela não substitui a chave externa de um banco protegido.

```bash
mkdir -p backups
read -r -s -p 'Senha do backup: ' BACKUP_PASSWORD
export BACKUP_PASSWORD
npm run backup -- create data/pilot-protected.sqlite backups/portal.irrbak
npm run backup -- restore backups/portal.irrbak data/recovered-portal.sqlite
unset BACKUP_PASSWORD
```

Mantenha `VAULT_KEY_FILE` da época do backup durante criação/restauração. Senha incorreta, chave ausente/errada, adulteração, integridade inválida ou destino existente recusam a operação. Backups legados do portal continuam aceitos. Nunca substituir banco em uso. Não há fsync explícito nem garantia de durabilidade em queda de energia; mantenha cópias externas verificadas.

Portal recuperado permanece pausado, dispositivos arquivados/offline, credenciais revogadas, envios pending/sending cancelados. Contas e histórico voltam ao instante do snapshot; revise permissões/senhas, pois mudanças posteriores não estão no backup. Comandos solicitados expiram; recebidos/iniciados permanecem inconclusivos. Roteiro anterior em VALIDACAO_V011.md permanece válido para esse fluxo.

Para cliente, substitua os caminhos pelos bancos próprios e use sua chave. A restauração mantém GUID/configuração e registros de comandos, limpa cache ativo, cancela envios antigos e remove ACKs pendentes do snapshot. Acrescenta `recovery_hold`: não executa controle, comandos ou envio enquanto não obtiver do portal uma configuração com todas as solenoides e agendas desabilitadas. Token novo deve ser provisionado pelo administrador e informado em DEVICE_TOKEN; execute apenas uma instância para aquele GUID. Banco com outro GUID é recusado.

Depois de aplicar e receber ACK dessa configuração desabilitada, o cliente libera o bloqueio, reconcilia comandos que o snapshot registrava como iniciados e informa runtime ocioso. Falha do ACK conserva o bloqueio após reinício. Só então publique configuração habilitada, após revisão operacional. No simulador isso confirma a saída lógica; em hardware futuro será necessário verificar drivers/saídas na bancada. Se o portal também foi restaurado e bloqueia restauração por comandos inconclusivos, investigue esses comandos antes de reprovisionar: não há botão para declarar uma parada física sem evidência.

## PowerShell

Os comandos npm são os mesmos. Para definir variáveis use, por exemplo:

```powershell
$env:VAULT_KEY_FILE = (Resolve-Path .\secrets\portal.vault-key.json).Path
$env:VAULT_REQUIRED = "1"
$env:DB_PATH = "$PWD\data\pilot-protected.sqlite"
npm start
```

Forneça BACKUP_PASSWORD e DEVICE_TOKEN por seu mecanismo local de segredos, sem colocá-los em linha de comando compartilhada ou no Git.
