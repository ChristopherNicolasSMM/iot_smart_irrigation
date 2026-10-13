# IoT Smart Irrigation — Conecta Agro Irrigação

Documentação inicial v0.1 — 13/10/2026.

Sistema de irrigação configurável para múltiplos ESP32, com portal central, portal local, simulação e futura implementação de firmware. Repositório canônico: https://github.com/ChristopherNicolasSMM/iot_smart_irrigation.

## Estado atual

Esta entrega contém requisitos, arquitetura proposta, contratos exemplificativos, critérios de aceitação e planejamento. Não contém portal executável, simulador executável ou firmware. Nenhum teste de hardware ou compilação foi realizado nesta etapa.

Sequência solicitada: **documentação → portal e simulação → avaliação completa do firmware → implementação e entrega para compilação e validação física**.

## Documentos

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
- Quantidade máxima de sensores de umidade ainda pendente.
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

## Próxima entrega

Portal e simulador determinístico, iniciando por dispositivos/áreas, componentes, agendas, modos de irrigação, teste de acionamento e histórico. Hardware real permanece fora dessa etapa.
