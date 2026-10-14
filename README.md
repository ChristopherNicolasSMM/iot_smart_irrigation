# IoT Smart Irrigation — Conecta Agro Irrigação

Piloto executável v0.13.0 — 14/10/2026.

Sistema de irrigação configurável para múltiplos ESP32, com portal central, portal local, simulação e futura implementação de firmware. Repositório canônico: https://github.com/ChristopherNicolasSMM/iot_smart_irrigation.

## Estado atual

Esta entrega contém portal central/local, simulador determinístico, persistência SQLite e cliente HTTP independente. Inclui propriedades e áreas, conjuntos por solenoide, agendas por dia, diagnóstico, integração HTTP(S), gestão de acesso e histórico filtrado com exportação CSV/JSON e arquivamento/transferência com histórico por propriedade e gestão auditada da fila. Inclui proteção por chave externa, migração/rotação, backup criptografado e recuperação em quarentena do portal/cliente, encaminhamento HTTP e ensaio de estabilidade documentado. Ainda não contém firmware ou comunicação com ESP32 real.

## Executar

Requer Node.js 24 ou superior.

```bash
npm start
```

Abrir http://127.0.0.1:8080 (central) ou http://127.0.0.1:8080/local (local simulado).
Login inicial: admin / admin-demo-2026 ou operador / operador-demo-2026.

Leia [execução e demonstração](docs/EXECUCAO.md) e [validação/limites](docs/VALIDACAO_V013.md). O piloto não fecha todo o gate da fase 1.
Sequência solicitada: **documentação → portal e simulação → avaliação completa do firmware → implementação e entrega para compilação e validação física**.

## Documentos

- [Backlog atualizado na raiz](BACKLOG.md)
- [Correção de testes Windows e sincronização do cliente](docs/CORRECAO_V0121.md)
- [Entrega consolidada 1–3 e medições](docs/VALIDACAO_V012.md)
- [Entrega consolidada 4–6](docs/VALIDACAO_V013.md)
- [Contrato JSON Schema/OpenAPI e fixtures](contracts/README.md)
- [Roteiro de conferência do portal](docs/ROTEIRO_PORTAL_V013.md)
- [Preparação para avaliar ESP32](docs/PREPARACAO_ESP32.md)
- [Operação segura: chave, migração, rotação e recuperação](docs/OPERACAO_SEGURA.md)

- [Requisitos e regras de irrigação](docs/REQUISITOS.md)
- [Arquitetura e decisões](docs/ARQUITETURA.md)
- [Telas e simulação](docs/PORTAL_SIMULACAO.md)
- [Contrato de comunicação](docs/CONTRATO_API.md)
- [Plano, esforços e pendências](docs/PLANO.md)
- [Critérios de aceitação](docs/ACEITE.md)
- [Exemplo de configuração](examples/device-config.json)
- [Exemplo de telemetria](examples/telemetry.json)

## Escopo

- Modos dinâmico por umidade, estático independente de umidade e híbrido.
- Agendamento por data específica ou dias da semana, com hora final ou duração.
- Umidade desejada e critérios de calibração.
- De zero a quatro sensores de temperatura e zero a quatro solenoides por ESP32.
- De zero a quatro sensores de umidade por ESP32, associados aos setores.
- GUID persistente por dispositivo, nomes e identificadores por componente.
- Associação do dispositivo e de seus setores a uma propriedade/área.
- Histórico de umidade a cada 15 minutos, eventos imediatos de operações e falhas.
- Teste de acionamento nos portais local e central.
- Administrador e operador; credenciais de dispositivo separadas das humanas.
- Envio configurável de dados e interpretação limitada de respostas.

## Projetos de referência

- DeviceMQTT: https://github.com/ChristopherNicolasSMM/DeviceMQTT — portal HTML/CSS/JS e simulador Python; firmware não encontrado na main examinada, árvore fe151e76e02b1ad75bf679b8ea0110a4cab86e56.
- ConectaAgro: https://github.com/ChristopherNicolasSMM/conectaAgro — Fastify, Nuxt/Vue e Supabase, árvore 020159e537e2ca4bf21762bdedbeeaa86d8375ce.

A análise anterior foi estática. Banco remoto, políticas RLS, operação real e compatibilidade de dependências não foram validados. O reaproveitamento de código deverá preservar os termos das licenças de origem; este repositório não recebe uma licença presumida.

## Trabalho neste chat

O GitHub conectado já confirmou acesso de leitura e escrita ao repositório. Informar seu link e mantê-lo autorizado na conexão permite trabalhar com seus arquivos aqui. Se o acesso for revogado, será necessário habilitá-lo novamente nas permissões da conexão GitHub. Nunca enviar tokens ou senhas no chat.

## Próxima sequência

Entregas **1–3 e 4–6 implementadas no piloto**. Conferir v0.13 no ambiente do usuário, revisar as evidências e iniciar avaliação HW-01/HW-02: placa/flash, sensores, solenoides, drivers/fonte, pinagem e limites. Firmware será preparado após essas decisões. Ver docs/PREPARACAO_ESP32.md e BACKLOG.md.
