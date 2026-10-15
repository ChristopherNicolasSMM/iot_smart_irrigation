# Sensores e bibliotecas

## HD-38

Base comercial: https://www.baudaeletronica.com.br/produto/sensor-de-umidade-do-solo-resistente-a-corrosao-hd-38.html

A loja informa resistência à corrosão; manual elétrico/pinout da unidade e vedação integral ainda não comprovados. Não transferir automaticamente especificações do anúncio de outra loja.

Proposta Arduino-ESP32: ADC do próprio framework (`analogRead`, atenuação e calibração conforme versão), sem biblioteca exclusiva HD-38. Usar AO para valores contínuos. Medir tensão antes de conectar, ruído com Wi-Fi/relés, resposta ao desconectar e alimentação 3,3 V. Módulo separado protegido em caixa.

Escala inicial: umidade relativa calibrada = clamp(100 × (raw − seco)/(molhado − seco), 0, 100). Fórmula admite orientação invertida; rejeitar pontos iguais/próximos e manter amostra bruta, qualidade e versão da calibração. Pontos são obtidos no solo/substrato representativo, não inferidos apenas de ar/copo de água. Clamp não deve transformar falha elétrica em leitura válida. Saturação/desconexão podem imitar solo seco/molhado: avaliar detecção e proteção complementar em bancada; não prometer detecção perfeita.

## DS18B20

Base comercial: https://www.usinainfo.com.br/sensor-de-temperatura/sensor-de-temperatura-ds-18b20-a-prova-d-agua-2645.html

Componente de referência: https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf

Proposta: sonda de três fios alimentada em 3,3 V, GND e DQ; pull-up inicial 4,7 kΩ entre DQ e 3,3 V. Confirmar fios com fornecedor, não por cores presumidas. Até quatro sondas em linha comum economizam portas; cabos longos/topologia/ruído podem exigir segmentação. Identificar por ROM persistida, nunca ordem de descoberta. Ausência/CRC/falha não geram temperatura zero.

Bibliotecas candidatas: OneWire (https://github.com/PaulStoffregen/OneWire) e DallasTemperature (https://github.com/milesburton/Arduino-Temperature-Control-Library). Conversão assíncrona, sem bloquear desligamento; validar resposta com as sondas reais. Resolução 10/11 bits como proposta econômica de tempo; resolução não aumenta precisão física. Não usar alimentação parasita nesta proposta.

Framework proposto: Arduino-ESP32, build reproduzível via PlatformIO ou Arduino CLI a decidir. **Nenhuma biblioteca instalada ou versão fixada nesta etapa.** Fixar versões e verificar licenças/compatibilidade quando modelo de placa e firmware forem definidos.

## Roteiro de bancada posterior

1. Conferir placa/módulos sem válvula: alimentação, limites elétricos e padrão desligado.
2. Medir AO do HD-38 e identificar conexões da sonda; calibrar por sensor.
3. Testar 0/1/4 sondas DS18B20, ROM, desconexão, cabos e conversão assíncrona.
4. Testar umidade com relés/Wi-Fi ativos; verificar leituras falsas e resets.
5. Testar válvula: corrente, pico, supressão, pressão/fechamento e limites.
6. Testar conjunto sem sensores, apenas umidade, apenas temperatura e ambos.
7. Validar boot/reset/queda de energia, relógio inválido, offline, comando duplicado e falha durante ciclo.
8. Registrar medições, fotos, modelo/lote e resultado; nenhum item concluído por documentação.
