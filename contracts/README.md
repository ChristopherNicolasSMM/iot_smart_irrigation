# Contrato HTTP do dispositivo — v1 / revisão documental 1.0.0

Fonte: iot-v1.schema.json (JSON Schema 2020-12). OpenAPI 3.1 em openapi.json é gerado pelo script, não editado manualmente. Disponíveis também por GET /contracts/iot-v1.schema.json e /contracts/openapi.json; contêm somente descrição/exemplos públicos. `/health` informa protocol_version=1 e contract_version=1.0.0. Versão do aplicativo é independente.

Cobertura: seis rotas de máquina, para telemetria, configuração/ACK, comandos/eventos e runtime. API humana do portal permanece em docs/CONTRATO_API.md; não afirmar que este OpenAPI descreve toda a API administrativa.

```bash
npm run contracts:generate
npm run contracts:check
```

26 fixtures em fixtures/manifest.json: configuração vazia/máxima/legada/day_times, resposta/hash/ACK, leituras ausentes, legado sem revisão/outputs, comandos normais/stop/reconciliação, runtime completo/legado, respostas e casos inválidos. São consumidas pelos testes compartilhados; não incluem credenciais. Referência para futura implementação C++, após hardware aprovado.

## Compatibilidade e semântica

schema_version continua 1, rotas /iot/v1 preservadas e propriedades adicionais permitidas. Revisão, boot_id, métrica/unidade, outputs e diagnóstico continuam opcionais onde o legado já os omitia. Novos GUIDs são UUID; transporte não reescreve identificadores legados. Agendas aceitam weekdays com horário comum e day_times por dia. Temperatura continua sendo monitoramento. Sensor ausente/inválido na telemetria tem valor null, não zero.

Datas precisam ser reais e incluir fuso; rejeita data impossível, NaN/Infinity, tipos errados e excesso de componentes/itens. Não aceita segundos intercalares (60) no perfil do piloto, pois o relógio usado pelo Node não os representa. Asserção de format=date/date-time deve ser habilitada em validadores externos; no padrão JSON Schema ela pode ser apenas anotação. Limites específicos estão nos schemas e em docs/VALIDACAO_V013.md.

Schema estrutural não substitui autorização, GPIO/capacidade, referências/uniqueness por ID, umidade 0–100 por categoria, fuso IANA, estado de comando, revisão/propriedade, expiração ou relógio monotônico. São regras de domínio do portal/controlador e precisam ser reproduzidas no firmware. Limites do simulador não são orçamento aprovado de RAM/flash.

Configuração/hash: SHA-256 dos bytes UTF-8 de JSON.stringify(configuration), preservando ordem serializada de chaves, sem normalização/ordenação implícita. Servidor fornece hash e cliente confere antes de aplicar; ACK inclui revisão/hash e idle=true. Firmware futuro deve conservar a representação JSON exata ou implementar serialização equivalente, usando fixtures para conferir. TLS/Bearer continuam necessários: hash não é assinatura nem autenticação.

IDs de comando são idempotentes. reconcile_only é reconciliação, nunca permissão para reiniciar saída. Stop é independente de duração; comando serializado tem duration_s=0 nesse caso. Iniciado após validade só pode ser reconciliado com horário de execução dentro da janela original; não renovar validade ao reenviar.

Telemetria accepted=true confirma persistência local. Encaminhamento externo tem outro ID estável, source_message_id e integration_trace; depende do ACK configurado no receptor. Não promete exatamente uma entrega sem deduplicação no destino.

## Validador Node

src/contracts.js aplica o subconjunto de keywords realmente usado, sem dependências, coerção, eval, referência externa ou mutação de defaults. Keywords não suportadas nas definições são recusadas na carga, para evitar ampliar o schema e ignorar uma regra em runtime. Profundidade limitada e erros mostram caminhos, não valores. Não é motor genérico completo de JSON Schema. Campos fora do contrato que sejam extensão permitida não recebem semântica presumida.

Servidor usa schemas para inputs de telemetria/ACK/eventos/runtime e verifica respostas; cliente valida requests/respostas e seus envelopes, além de conferir hash. Domínio de configurações continua em validateConfig e regras do protocolo. O firmware não deve executar o módulo Node; deve usar o documento/fixtures como referência e testes comuns.

Padrões consultados: [OpenAPI 3.1](https://spec.openapis.org/oas/v3.1.0.html) e [JSON Schema 2020-12](https://json-schema.org/draft/2020-12/json-schema-validation). Reprodução independente de fixtures pode usar qualquer validador 2020-12 com asserção de formato habilitada.
