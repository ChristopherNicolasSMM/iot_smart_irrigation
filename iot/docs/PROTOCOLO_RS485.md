# Protocolo RS485 experimental v1

Protocolo próprio, não Modbus. Implementação em firmware/include/RemoteProtocol.h e firmware/src/main.cpp. Meio físico e topologia ainda em avaliação; uma UART na matriz, endereços 1..4. Para estrela de ramais 100 m é necessário hub ativo ou revisão do hardware; não aprovar estrela passiva.

## Quadro binário — 28 bytes

| Offset | Bytes | Campo |
|---|---:|---|
| 0 | 1 | Marcador 0xA5 |
| 1 | 1 | Versão 1 |
| 2 | 1 | Endereço 1..4 |
| 3 | 1 | Tipo: Poll=1, Start=2, Stop=3, Reply=4 |
| 4 | 4 | Sessão de boot da matriz, little endian |
| 8 | 4 | Sequência, little endian |
| 12 | 4 | Start: duração ms; Reply: umidade centésimos nos 16 bits altos e temperatura centésimos signed16 nos baixos |
| 16 | 2 | Status/flags |
| 18 | 8 | Token aleatório de boot do nó, little endian |
| 26 | 2 | CRC16 polinômio 0xA001 sobre bytes 0..25 |

Reply flags: bit0 saída ativa, bit1 umidade válida, bit2 temperatura válida, bit3 solicitação aceita, bit4 driver disponível. Invalidade não vira zero; matriz considera flags, idade e bindings.

## Regras

- Poll entrega token do boot atual. Start exige mesmo token e sessão estabelecida por Poll; reboot do nó invalida comandos antigos.
- Sequência repetida/antiga nunca prolonga execução. Mudança de sessão via Poll desliga lease anterior.
- Stop tem prioridade e pode desligar sem token; CRC não oferece autenticação contra invasor no cabo. Protocolo pressupõe instalação física confiável; MAC/autenticação de enlace é evolução pendente.
- Resposta precisa corresponder a endereço/sessão/sequência, incluir driver disponível e confirmar estado antes de matriz registrar started.
- Perda de confirmação marca saída desconhecida; não afirmar off no histórico a partir de cache. Matriz tenta parada; nó limita duração e desliga por perda de Poll em 3 s.
- Nó de referência limita ciclo a IOT_NODE_MAX_RUN_MS (padrão 600000). Ajustar após validar válvula, fonte e capacidade. Limite de teste no controlador é 30 s.
- 9600 baud, 8N1, resposta aguardada até 60 ms após transmissão; validar latência real do nó/NVS e ajustar no hardware antes de campo. Não é garantia de tempo real rígido.
- GPIO DE19 exige polarização externa desligada. RX16/TX17 são proposta apenas para WROOM-32E sem PSRAM.

Acionamento confirmado é saída elétrica solicitada pelo nó, não posição da válvula/vazão medidas. Umidade/temperatura são opcionais por ponto; matriz configura IDs/área/conjunto, nó usa endereço subordinado e guarda sua própria identidade local.

Referências físicas e alimentação: [CABOS_E_PLACA.md](CABOS_E_PLACA.md).
