# Entrega executável v0.2.0 — 13/10/2026

## Implementado

Portal central e local responsivos, autenticação admin/operador, múltiplos GUIDs, áreas textuais, componentes com limites, setores, três modos, agendas por data/semana e hora final/duração, acionamento manual/teste/parada, injeção de valores/falhas, relógio acelerado, eventos e amostras a cada 900 s, SQLite e prévia de integração.

## Verificações realizadas

- Node.js 24.19.0.
- npm run check: sintaxe de engine, servidor e frontend.
- npm test: 36 testes aprovados de controlador, API HTTP e persistência.
- Servidor iniciado e arquivos /, /local, /app.js e /style.css servidos por HTTP.
- Tentativa de validação visual automatizada bloqueada: Chromium não estava instalado e download retornou arquivo inválido. Não foi possível conferir renderização e cliques em navegador nesta execução. O código de frontend passou verificação de sintaxe; validação visual local permanece necessária.

## Cobertura e gate

A entrega é um piloto executável inicial; o gate integral da fase 1 NÃO está concluído.
Cobertos em testes: componentes vazios/limites/IDs, estático sem umidade/solo úmido, datas/semana, dinâmico e alvo, híbrido dentro/fora de janela, sensor inválido, testes local/web offline, expiração, deduplicação, parada, reinício, amostragem, tempo virtual, validação atômica, operador e persistência.

Ainda pendentes do aceite completo: AT-26 ingestão real, AT-27 ACK de persistência do dispositivo remoto, AT-28 fila/envio externo, AT-30 isolamento por propriedade, AT-32 revisão desejada/aplicada assíncrona, AT-34 DST/relógio inválido com fluxo completo e parte dos estados de transporte/falhas. Não há alegação de 40 critérios integralmente aprovados.

## Decisão de implementação do piloto

Usado Node.js nativo + SQLite + HTML/CSS/JS em vez de Nuxt/Fastify/Supabase nesta primeira demonstração. Objetivo: iniciar com um comando e zero dependências, controlar tempo virtual e validar regras. Stack proposta de produção permanece sujeita à revisão; esta escolha não constitui migração definitiva dos projetos de referência.

## Próxima entrega

Validação visual pelo usuário, ajustes de usabilidade e evolução da API/persistência para propriedade/áreas estruturadas, revisão assíncrona, ingestão e perfis de integração com entrega/ACK reais. Firmware somente após avaliação completa prevista no plano.
