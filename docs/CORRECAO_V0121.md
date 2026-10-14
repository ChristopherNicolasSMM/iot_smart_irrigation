# Correção v0.12.1 — limpeza SQLite e sincronização

14/10/2026. Corrige o retorno da v0.12 no Windows: 215 testes, 209 aprovados e 6 falhas. É ajuste da entrega 1–3, antes de iniciar a próxima entrega 4–6.

## Cinco falhas de backup: EBUSY no teardown

Os erros enviados ocorreram em tests/backup.test.js:3, na remoção de original.sqlite. A fixture registrava primeiro o hook que remove o diretório, depois o que fecha o aplicativo. No Windows o arquivo aberto não pode ser removido. Linux permitia unlink mesmo com o banco aberto, ocultando o problema de ordem.

Agora há um único hook que executa app.close() e só depois remove o diretório. Um teste de regressão verifica DatabaseSync.isOpen=false na função que remove os arquivos, também no Linux. Não há supressão de EBUSY nem exclusão de bancos do usuário: a alteração se aplica aos temporários da suíte.

## Sexta falha: reconexão ainda em started

O teste de término offline esperava completed logo após um único await client.poll(). Havia um defeito de concorrência no cliente: poll retornava imediatamente quando um ciclo automático estava ocupado. Agora uma chamada explícita aguarda o ciclo em andamento e executa seu próprio ciclo novo. O timer ignora um intervalo ocupado, evitando acumular requisições. close aguarda a Promise de trabalho em andamento antes de fechar SQLite.

Além disso, a primeira requisição após reiniciar um servidor pode falhar transitoriamente. O teste de reconexão agora injeta essa falha, mantém ACK persistido e verifica completed em até três ciclos de sincronização. Não muda duração, força estado completed no banco, descarta ACK ou oculta falha permanente; após três tentativas sem confirmação, o teste continua falhando e inclui logs do cliente no diagnóstico.

Um novo teste mantém a consulta de configuração bloqueada e chama poll novamente. Verifica que a segunda chamada não termina cedo e realiza um ciclo novo. Esse teste foi executado contra o cliente anterior: falhou com código 1 e a asserção esperada. Com a correção passa.

## Validação e atualização

217 testes aprovados no ambiente Linux disponível, npm run check e git diff --check. Os testes focados foram repetidos para verificar concorrência; guardas verificam fechamento do banco e sincronização em disputa. Não houve execução nativa de Windows nesta entrega: atualizar e repetir a suíte nesse ambiente para confirmar a correção enviada pelo usuário.

PowerShell, com portal/cliente parados:

```powershell
git pull --ff-only
npm test
npm run check
```

Para diagnóstico focado, se necessário:

```powershell
node --test tests/backup.test.js tests/client-recovery.test.js
```

Depois reinicie portal/cliente. Não há migração adicional, troca de chave ou necessidade de apagar data/. O ExperimentalWarning de SQLite mostrado no log é distinto das exceções EBUSY e da asserção started/completed. Não removemos avisos do runtime para fazer testes parecerem aprovados.

A validação visual/hardware e o alerta nativo do ensaio longo da v0.12 continuam com o status anterior. Etapas 4–6 permanecem reservadas para a próxima entrega consolidada.
