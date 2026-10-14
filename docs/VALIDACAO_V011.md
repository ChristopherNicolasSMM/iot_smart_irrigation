# Validação v0.11 — backup criptografado e recuperação

Entrega incremental de P1-06, em 14/10/2026. Não inclui firmware nem alteração visual do portal. Backup/restauração são ferramentas de operação via CLI, fora da API web e das contas de operador.

## Criar e restaurar

Requer Node 24+, banco do portal v0.10 ou superior e diretório de destino já existente. Não atende o banco independente do cliente. Exemplo em Bash:

```bash
mkdir -p backups
read -r -s -p 'Senha do backup (mínimo 16 caracteres): ' BACKUP_PASSWORD
export BACKUP_PASSWORD
npm run backup -- create data/pilot.sqlite backups/pilot.irrbak
npm run backup -- restore backups/pilot.irrbak data/recovered.sqlite
unset BACKUP_PASSWORD
DB_PATH=data/recovered.sqlite npm start
```

Para evitar conflito de porta, encerre o portal original antes de iniciar a cópia. Não substitua o banco original. Faça login com uma conta existente no instante do backup; senhas e vínculos dessa data são preservados. Sessões antigas não são restauradas. Uma troca de senha posterior ao backup não existe na cópia: revise contas e vínculos antes de voltar ao serviço.

Guarde a senha fora do backup, em gerenciador de segredos. Sem ela não há recuperação. Não envie senha no chat, nem versione backup ou banco. Cada criação exige um nome novo; mantenha cópias externas e teste periodicamente a recuperação. Esta entrega não agenda backups nem garante retenção externa.

## Formato e consistência

Snapshot consistente SQLite por `VACUUM INTO`, incluindo transações já confirmadas no WAL; não copia apenas o arquivo principal. Verifica `integrity_check`, tabelas essenciais e estado antes de criar e depois de restaurar. Pode executar enquanto o portal está aberto, com custo de I/O; medir concorrência e volume em P1-07. Uma origem ausente não cria banco vazio.

Formato próprio IRRBACK1: AES-256-GCM, salt de 16 bytes, nonce aleatório de 12 bytes, tag de 16 bytes, chave derivada via scrypt padrão do Node (N=16384, r=8, p=1). Magic é autenticado; conteúdo/metadados são cifrados e autenticados. Metadados internos incluem versão, tipo portal, instante real de criação e SHA-256 do snapshot. Não há compressão. Limite do snapshot: 64 MiB; arquivo de entrada limitado antes da leitura. Uso de memória proporcional ao banco; não é ferramenta para grandes instalações.

Arquivos finais têm modo 0600 em sistemas POSIX. Snapshot temporário da criação fica em diretório temporário privado e é removido; restauração escreve arquivo temporário 0600 no diretório de destino. Publicação usa hard link exclusivo no mesmo filesystem, recusando destino existente; nenhum overwrite. Rejeita também WAL/SHM existentes no destino de recuperação. Falhas limpam arquivos temporários. Sistemas sem suporte a hard links recusam a operação. Não há garantia de persistência após queda de energia sem fsync; cópias externas e teste de recuperação permanecem necessários.

## Quarentena da cópia restaurada

- Preserva GUIDs, contas/hashes, vínculos, configurações e histórico com propriedades originais.
- Pausa a simulação; arquiva todos os dispositivos e marca comunicação offline. Limpa caches de execuções ativas/comandos, sem afirmar parada física.
- Desabilita perfis de envio, revoga todas as credenciais de dispositivo e remove cabeçalhos atuais de integração.
- Cancela mensagens pending/sending, preservando IDs, payloads, cabeçalhos capturados e tentativas para análise. Nenhum envio reinicia automaticamente. Mensagens terminais permanecem como estavam.
- Comandos requested ficam expired; received/started ficam unconfirmed, pois podem ter sido executados. Já unconfirmed permanece desconhecido. Essa condição bloqueia restauração do dispositivo pela política existente; requer investigação e reconciliação operacional. Não há botão para declarar arbitrariamente uma parada.
- Registra recovery_audit com data, origem/hash e quantidade de dispositivos. Histórico de irrigação não é reescrito; os eventos posteriores ao snapshot estão ausentes.

Dispositivos sem comandos inconclusivos podem ser restaurados pelo fluxo existente: agendas/solenoides voltam desabilitadas, seguindo v0.9. Reprovisione cada cliente HTTP com nova credencial, aplique configuração desabilitada e confira runtime antes de habilitar irrigação. Reveja autenticação das integrações e mensagens antigas antes de reprocessar. **Nunca opere dois portais como autoridades simultâneas dos mesmos dispositivos.** Um ESP32 real pode continuar irrigando offline com a configuração antiga; esta ferramenta não envia parada nem valida saídas físicas.

## Proteção e limites pendentes

O artefato de backup está cifrado. O SQLite em uso, WAL, corpos/perfis e cabeçalhos capturados da fila continuam em texto claro, inclusive na cópia restaurada. Remover cabeçalhos atuais não apaga segredos de snapshots da fila, backups anteriores ou páginas SQLite. Use controle de acesso ao host/volume, autenticação própria e TLS na implantação. Não declarar banco integralmente cifrado.

P1-06 permanece **parcial**: falta proteção dos segredos de integração em repouso no banco ativo, chave externa/rotação e migração controlada dos segredos legados; falta recuperação do cliente independente. A gestão de chave desta entrega é a senha externa do backup, não KMS. O formato criptográfico não recebeu auditoria independente. Não abrir backups de origem não confiável, mesmo conhecendo sua senha.

## Evidências

189 testes aprovados no total; cinco testes novos verificam snapshot com portal/WAL aberto e reinício da cópia, preservação de GUID/histórico/contas, revogação/quarentena e origem intacta, senha errada/adulteração sem criar destino, não sobrescrita, formato/fonte/senha inválidos e randomização do ciphertext. `npm run check` e `git diff --check` aprovados. Não há nova validação visual ou física. Próxima entrega: concluir proteção de segredos no banco ativo e recuperação do cliente; depois P1-07.
