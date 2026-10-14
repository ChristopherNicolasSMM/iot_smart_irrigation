# Entrega consolidada v0.13 — etapas 4 a 6

14/10/2026. Sequência autorizada após validação da v0.12.1 pelo usuário no Windows. Fecha contrato/testes temporais/conferência do portal no escopo de software. Não entrega firmware, drivers, pinagem nem validação física.

## 4. Contrato compartilhado

Contrato HTTP v1 em contracts/iot-v1.schema.json, com revisão documental 1.0.0; OpenAPI 3.1 gerado em contracts/openapi.json. Seis rotas de máquina: telemetria, configuração/ACK, comandos/eventos e runtime. API administrativa humana continua no manual CONTRATO_API.md, fora deste OpenAPI.

Servidor e cliente usam a mesma validação estrutural. Cliente confere o hash SHA-256 da configuração antes de aplicá-la. Body inválido gera 400 antes da ingestão; cliente com resposta inválida/hash adulterado não cadastra ou aciona controlador a partir dela. Autorização, referências, tipos de sensor, fuso, revisão, transições e monotonicidade permanecem regras de domínio.

26 fixtures válidas/inválidas compartilhadas e verificáveis por `npm run contracts:check`. Exemplos examples/device-config.json e telemetry.json foram corrigidos: a configuração antiga era uma proposta com estrutura diferente da API real, e o evento de telemetria não tinha device_guid. Nenhum banco ou configuração persistida é reescrito por essa correção dos exemplos.

Compatibilidade: schema_version=1 e /iot/v1 mantidos; propriedades adicionais permitidas. Campos opcionais de legado continuam opcionais, incluindo revisão/boot/outputs e diagnóstico. Configuração semanal aceita weekdays antigo e day_times. Datas impossíveis/sem fuso, tipo errado, valor não finito e limite estrutural excedido são rejeitados. Sem coerção de texto para número.

Stop serializado agora usa duration_s=0 independentemente da duração informada, pois parada não deve ser bloqueada por duração inválida. Estados/IDs/expiração permanecem os mesmos. Novo `/health` informa protocol_version e contract_version. Schemas públicos em GET /contracts/iot-v1.schema.json e /contracts/openapi.json, sem segredos.

Validador Node usa apenas o subconjunto de JSON Schema empregado no documento e recusa keywords desconhecidas na carga; não é motor genérico completo nem exige dependência em runtime. Documento/26 fixtures também passaram no jsonschema 4.25.1 independente, com FormatChecker e rfc3339-validator 0.1.4 ativados. Informações de formatos/domínio/hash em [contracts/README.md](../contracts/README.md).

## 5. Tempo raro e recuperação longa

Novos cenários verificados:

| Cenário | Comportamento |
|---|---|
| Lord Howe: DST de 30 minutos | Hora inexistente omitida; hora repetida executa uma vez |
| Chatham: offset de 45 minutos e DST | Localização civil/omissão corretas |
| Apia: 30/12/2011 inexistente inteiro | Log de omissão no primeiro instante civil válido; nunca executar a agenda omitida |
| Hora final inexistente | Janela inteira inválida, sem ligar parcialmente |
| Hora final após salto DST | Duração efetiva usa instantes reais, limitada pelo relógio monotônico |
| Kathmandu: janela híbrida na meia-noite | Um ciclo pela janela/data original e término correto |
| Falta de energia por 90 dias | Datas específicas antigas conhecidas registradas; resumo semanal delimitado; não executar atrasado |
| Retorno exatamente na abertura e ajuste para trás | Ocorrência consumida, sem duplicação |
| Reboot de comando ativo e retrocesso UTC | Não retomar saída nem prolongar duração |
| Cliente persistido em parada longa | Interrupção/reconciliação e log de data antiga, sem retomada tardia |

resolveWindow procura o primeiro minuto civil válido até 48 h após uma lacuna. Esse instante serve somente para registrar omissão: invalid=true sempre bloqueia acionamento. Isso cobre a data inteira pulada que a busca anterior de 180 min não cobria.

Recuperação weekly continua limitada aos últimos sete dias para não gerar trabalho/histórico sem limite. Agora agendas date habilitadas da configuração aplicada também são verificadas ao longo de toda a interrupção, mesmo antigas. Logs incluem missed_scan_truncated e specific_dates_included=true quando há recorte semanal. Ocorrências já registradas não são duplicadas; log é no instante de descoberta, com ID contendo a data original. Não inferir agenda de revisões antigas não disponíveis no controlador.

Ainda não se promete registrar individualmente todos os horários semanais de meses offline: o recorte é explicitado. Tempo de controle usa uptime local; ajuste de UTC não amplia execução. Dados de fuso dependem do ICU/tzdata do Node, e o futuro ESP32 precisará de política/implementação equivalente. Fontes históricas consultadas: [IANA Australasia](https://data.iana.org/time-zones/tzdb/australasia). Não prova todas as transições IANA possíveis ou comportamento físico após falta de energia.

## 6. Conferência do portal

Testes de API/código verificam editor real com sete checkboxes, horários próprios, seleção por conjunto/solenoide, escape de HTML, IDs/abas/referências existentes, configuração/histórico de 15 min, permissões e portal local. Renderer da agenda foi extraído para módulo compartilhado testável, preservando markup/formatos.

Conferência efetiva de navegador: Chromium 153.0.8010.0, Linux, Node v24.19.0, viewport desktop 1366×900 e móvel 390×844. Banco em memória e credenciais públicas de demonstração; nenhuma instância/banco do usuário usada. Passaram:

- login admin, sete checkboxes e segunda/terça com horas diferentes;
- publicação pendente/aplicada e persistência dos horários;
- navegação pelas abas, teste de saída e atualização para completed;
- amostras de umidade/temperatura e consulta de histórico;
- logout, resposta de estado atrasada após logout bloqueada, login operador;
- editor/integrações ocultos ao operador e página móvel sem overflow horizontal;
- nenhuma exceção JavaScript de página.

As capturas desktop/histórico e móvel/visão geral foram inspecionadas. Registro reproduzível sem segredos em [benchmarks/V013_UI.json](benchmarks/V013_UI.json). O navegador remoto não alcançou o servidor e o download oficial de Chromium falhou; foi usada uma instalação temporária de Chromium de teste via pacote @sparticuz/chromium, sem acrescentar dependência ao projeto.

Falhas reais encontradas e corrigidas:

1. “Sair” fazia GET, enquanto servidor exige POST; agora envia POST e limpa dados de sessão/credencial/senhas locais, aguardando logout antes de permitir novo login.
2. Resposta de consulta iniciada antes do logout podia reutilizar estado antigo. Um contador de sessão invalida respostas atrasadas, também na troca de conta.
3. Resultado de comando interno permanecia started depois do término. Agora acompanha estado interno; no HTTP continua dependendo de ACK. Troca de dispositivo/propriedade limpa resultado antigo e respostas tardias não o substituem indevidamente.
4. IDs padrão baseados no tamanho da lista podiam colidir após remoção. Geração agora evita IDs atuais/arquivados, incluindo horários por dia.
5. Motivos comuns de comando/recuperação e publicação ganharam apresentação em português; códigos persistidos permanecem iguais.

Script opcional `npm run test:ui` reproduz a conferência em navegador local, sem acessar banco existente. Playwright somente para teste opcional, não dependência do portal. Roteiro completo desktop/móvel/UI-01..UI-16 e comandos Windows em [ROTEIRO_PORTAL_V013.md](ROTEIRO_PORTAL_V013.md). A conferência automatizada não cobre troca real de senha, retenção destrutiva, todas as transferências ou todos os dispositivos de tela; esses fluxos possuem testes de API e roteiro manual. A confirmação desta versão no Windows e revisão visual do usuário ainda precisam ocorrer.

## Evidências e atualização

269 testes aprovados, npm run check, npm run contracts:check e git diff --check; teste visual local aprovado e validação independente dos schemas/fixtures. Sem nova dependência runtime, migração de tabelas ou remoção de dados. Inicie cliente atualizado para habilitar validação de resposta/hash. Legado compatível, porém pacotes malformados antes tolerados por Date.parse ou campos sem estrutura válida agora retornam erro explícito.

Com portal/cliente parados, preservar bancos/chaves, atualizar git pull --ff-only, npm test, npm run check e npm run contracts:check; reiniciar processos com os parâmetros habituais. Banco protegido ainda exige sua chave. Não apagar data/.

O ensaio longo/alerta nativo da v0.12 mantém suas evidências e pendência; não houve novo ensaio de cinco minutos nesta entrega. A validação da v0.12.1 no Windows foi confirmada pelo usuário. Não declarar esta v0.13, firmware ou bancada/campo validados por esse retorno anterior.

## Próxima sequência

As duas entregas consolidadas estão implementadas no piloto. Próxima ação: revisão conjunta destas evidências e preparação HW-01/HW-02. Dados a levantar em [PREPARACAO_ESP32.md](PREPARACAO_ESP32.md). Firmware só começa depois de hardware, pinagem, fonte/driver, política de falhas e orçamento de recursos aprovados.
