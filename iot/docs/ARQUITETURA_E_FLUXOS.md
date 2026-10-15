# Arquitetura e fluxos

Documentação proposta. Controle local continua sem Internet quando configuração e relógio permitem; nenhuma rotina de rede deve atrasar parada/limite de duração.

```mermaid
flowchart TD
  F["Fonte 12 V DC"] --> V["Contatos dos relés e válvulas"]
  F --> B["Conversor 5 V"]
  B --> E["ESP32 e alimentação dos módulos"]
  U["0–4 HD-38 / ADC1"] --> E
  T["0–4 DS18B20 / 1-Wire"] --> E
  E --> D["Drivers dos relés"]
  D --> V
  E <--> P["Portal central / HTTPS"]
  E <--> L["Portal local autenticado"]
```

O bloco de alimentação dos módulos segue a tensão real de cada componente. Contatos de potência e comando são circuitos diferentes. O portal central armazena histórico; não é o relógio de duração do dispositivo.

```mermaid
flowchart TD
  R["Boot ou reset"] --> O["Saídas desligadas por hardware"]
  O --> C["Validar configuração, identidade e relógio"]
  C --> A["Avaliar comando ou agenda"]
  A --> M{"Modo exige umidade?"}
  M -->|"Sim"| S{"Leitura válida e condição atendida?"}
  M -->|"Não"| G["Verificar habilitação, duração e capacidade"]
  S -->|"Sim"| G
  S -->|"Não"| N["Omitir ou falhar com motivo"]
  G --> K{"Proteções permitem?"}
  K -->|"Sim"| I["Acionar e registrar saída"]
  K -->|"Não"| N
  I --> Q["Monitorar limite monotônico e parada"]
  Q --> H["Desligar e registrar término"]
```

Durante ciclo dinâmico/híbrido, leitura inválida/antiga exige parada; estático/manual não dependem dela. Histórico a cada 900 s não substitui aquisição frequente para controle. Relé acionado não comprova vazão; sem sensor hidráulico registrar saída elétrica, nunca irrigação fisicamente confirmada.

Módulos futuros: drivers de sensores/saídas; controlador local; relógio/calendário; configuração persistente; protocolo HTTP; fila limitada; autenticação/portal local. Estado seguro, watchdog e reconciliação precedem reconexão. Não retomar automaticamente execução interrompida por energia. GUID é identidade, não segredo.
