# Preparação para avaliar ESP32

Documento para a etapa seguinte, depois das entregas consolidadas 1–3 e 4–6. Não escolhe componentes, pinos, drivers nem implementa firmware. O usuário confirmou a v0.12.1 no Windows; v0.13 tem evidências de software/navegador local e aguarda revisão no ambiente do usuário.

## Informações necessárias

| Área | Levantar antes da decisão |
|---|---|
| Placa | Modelo exato ESP32, módulo/placa, flash e eventual PSRAM, alimentação disponível |
| Umidade (0–4) | Modelo, saída/interface, tensão, faixa e calibração; umidade relativa ou volumétrica; distância dos cabos |
| Temperatura (0–4) | Modelo/interface, resolução, alimentação e distância; participação é monitoramento nesta versão |
| Solenoides (0–4) | Tensão AC/DC, corrente nominal/pico, tipo normalmente fechado/aberto, duração segura |
| Acionamento | Driver/isolação/proteções adequados, estado no boot e em reset, capacidade da fonte e ligação física |
| Água | Pressão/vazão, simultaneidade permitida, necessidade de bomba e intertravamentos; não presumir quatro saídas simultâneas |
| Instalação | Área por GUID/conjunto, cabos, umidade/caixa, conectividade e operação sem Internet |
| Recursos | Limites medidos de RAM/flash, JSON/TLS, agendas, IDs consumidos, fila e escrita persistente; não copiar orçamento Node |
| Operação | Provisionamento, armazenamento de credencial, relógio/fuso, reinício sem retomar saídas, logs/reconciliação |

## Decisões de software já disponíveis

- Contrato HTTP v1/documentação 1.0.0 e fixtures compartilhadas; hash de configuração confere antes da aplicação.
- GUID + IDs de componentes/conjuntos; até quatro por categoria, agenda por conjunto/solenoide e dias com horários separados.
- Estático/manual sem dependência de umidade; dinâmico/híbrido dependem de leitura válida e proteções. Híbrido até um ciclo por ocorrência/revisão.
- Teste curto, expiração, parada idempotente e duração limitada por tempo monotônico.
- Operação local durante perda de nuvem, amostra histórica a cada 900 s e eventos imediatos.
- Reconciliação após falta de energia/tempo, omissão explícita de ocorrência perdida e recorte semanal em recuperação longa.
- Temperatura não controla irrigação; saída elétrica não confirma abertura física/vazão.

## Gate de entrada no firmware

1. Conferir os fluxos/documentação da v0.13 e resolver falhas encontradas no ambiente do usuário.
2. Aprovar lista de materiais, capacidade elétrica/hidráulica e pinagem segura (HW-01).
3. Definir/medir limites de armazenamento, TLS/JSON/filas/agendas e política de falhas no ESP32 (HW-02).
4. Então implementar firmware C++ (FW-01), com build reproduzível e fixtures do contrato.
5. Compilar, validar bancada e campo com o usuário (FW-02). Registrar evidências; sem elas não declarar hardware funcional.

MQTT/OTA, RS485, vazão/pressão e intertravamento de bomba permanecem evoluções no backlog, salvo necessidade física identificada na avaliação. Hospedagem, KMS e ensaio contínuo prolongado são decisões de implantação; o alerta nativo do ensaio v0.12 ainda deve ser reproduzido/avaliado no ambiente escolhido.
