# Conferência do portal — v0.13

Usar apenas banco de laboratório, sem ESP32 físico ou integrações de produção. A v0.12.1 foi validada pelo usuário no Windows; isso não comprova todos os fluxos visuais abaixo.

## Preparar e atualizar

Com portal e cliente parados:

```powershell
git pull --ff-only
npm test
npm run check
npm run contracts:check
# Banco novo somente para esta conferência; não substituir o banco do projeto.
$env:DB_PATH = "$PWD\data\qa-v013.sqlite"
npm start
```

Se VAULT_REQUIRED/VAULT_KEY_FILE estiverem configurados, manter a chave correta para bancos protegidos ou usar um banco novo apropriado. Nunca remover chave ou banco para resolver falha de abertura. `/health` informa versão do aplicativo, protocolo e contrato, sem segredos.

Abrir http://127.0.0.1:8080 e http://127.0.0.1:8080/local. No banco de teste novo, credenciais de demonstração são admin/admin-demo-2026 e operador/operador-demo-2026, salvo configuração das variáveis iniciais de senha. Saídas permanecem simuladas.

## Roteiro manual e critérios observáveis

| ID | Conferir | Resultado esperado |
|---|---|---|
| UI-01 | Login, logout, erro de senha e navegação central/local | Erro visível; painel abre após login; logout bloqueia nova consulta; local continua identificado como simulação |
| UI-02 | Visão geral e diagnóstico | Nome/GUID, revisão aplicada/desejada, qualidade/idade dos sensores e fonte; offline/estado desconhecido não aparecem como confirmação física |
| UI-03 | Editor de agenda semanal | Sete checkboxes com segunda a domingo; marque apenas segunda/terça e confira horários separados |
| UI-04 | Horários por dia | Segunda 06:00 por 5 s; terça 07:30 até 07:40; adicione segundo horário somente na terça. Publique, aguarde aplicada, recarregue: dias/horários preservados |
| UI-05 | Conjuntos e agendas | Dois conjuntos com solenoides distintas; agenda de um conjunto não aciona a outra solenoide; umidade/temperatura identificadas corretamente |
| UI-06 | Estático/manual independentes da umidade | Solo úmido/inválido não bloqueia estático/teste; dinâmica/híbrida exigem umidade válida e condições permitidas |
| UI-07 | Componentes vazios e limites | Dispositivo com zero componentes abre sem erro; máximo 4 umidades, 4 temperaturas, 4 solenoides; excesso/referência inválida mostra erro sem publicar revisão |
| UI-08 | Teste e parada | Solicitar teste de 5 s; mostrar solicitado/recebido/iniciado/concluído conforme controlador. Parar é idempotente; não repetir saída com mesmo ID |
| UI-09 | Histórico | Avançar 15 min; consultar umidade e temperatura, filtrar período/conjunto/qualidade; gráficos não substituem dados ausentes por zero |
| UI-10 | Exportação | CSV/JSON autenticados, filtros respeitados, datas UTC; erro explícito ao exceder limite, sem arquivo parcial |
| UI-11 | Integração e fila | Usar somente receptor de teste; prévia não envia; tokens não retornam ao consultar perfil. Fila distingue pendência, confirmação/falha; repetição preserva ID |
| UI-12 | Arquivo/restauração/transferência | Somente com saídas ociosas; arquivo revoga token e preserva histórico; restauração vem desabilitada; transferência mantém histórico anterior na origem |
| UI-13 | Operador | Navegação sem editor/integrações/gestão administrativa; monitora, testa e para; tentativa por API de editar é recusada |
| UI-14 | Propriedades | Troca de propriedade não conserva consulta ou credencial visível da anterior; GUID não autorizado não pode ser acessado por API |
| UI-15 | Largura e teclado | Conferir desktop e 390 px; sem rolagem horizontal da página, tabelas podem rolar internamente; labels/foco/checkboxes utilizáveis por teclado |
| UI-16 | Temporal/reconexão | Logs de agenda omitida em português; revisão pendente não é aplicada durante execução; cliente offline encerra por duração e confirma após voltar |

Não aplicar retenção nem revogar acesso em banco de uso real durante essa conferência. Contas/senhas/credenciais devem ser exercitadas em laboratório próprio. Nenhum teste Node confirma vazão ou estado hidráulico.

## Teste visual opcional em navegador local

O script inicia seu próprio portal em memória, com porta livre; usa apenas credenciais públicas de demonstração desse banco e fecha servidor/navegador ao terminar. Não usa data/pilot.sqlite. A ferramenta de navegador é exclusiva para teste e não é dependência de npm start.

Se Chrome estiver instalado no Windows:

```powershell
npm install --no-save --package-lock=false playwright
$env:UI_BROWSER_CHANNEL = "chrome"
npm run test:ui
```

Alternativamente, use o Chromium de teste do Playwright:

```powershell
npx playwright install chromium
Remove-Item Env:UI_BROWSER_CHANNEL -ErrorAction SilentlyContinue
npm run test:ui
```

Não publicar node_modules nem mudar package.json/lock para executar esse teste opcional. A origem do software é o pacote Playwright e seu download oficial. Se preferir dispensar a ferramenta, execute o roteiro manual acima.

UI_QA_OUTPUT opcional aponta para pasta onde o script salva capturas desktop/móvel; deixe-a em data/ para não versionar imagens de teste. O script confere login, dias/horários e publicação, abas, teste e término, amostra histórica, perfil de operador, ausência de erro JS e largura móvel. Não automatiza troca de senha, transferência ou retenção destrutiva; esses fluxos seguem roteiro manual.

## Registrar retorno

Informar versão, resultado de npm test/contracts:check e IDs UI-01..UI-16 aprovados ou com problema. Para falha visual, indicar aba/ação, tamanho da janela e mensagem exibida. Não enviar chave, token ou senha reais. Avaliação do hardware/ESP32 começa depois da revisão dessas evidências e das decisões HW-01/HW-02.
