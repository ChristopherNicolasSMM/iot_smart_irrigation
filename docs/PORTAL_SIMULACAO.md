# Portal e simulação — primeira fase

## Fluxos e telas

| Tela | Conteúdo e ações |
|---|---|
| Login | Usuário/senha, erros, sessão e perfis administrador/operador |
| Visão geral | Áreas, dispositivos online/offline, leituras recentes e execuções |
| Dispositivos | GUID, nome, propriedade/área, função, real/simulado e revisão aplicada |
| Detalhe | Estado desejado/observado, última comunicação, sensores e saídas |
| Componentes | 0–4 temperaturas, 0–4 umidades, 0–4 válvulas e vínculos, nomes e IDs |
| Setores | Válvula, sensor controlador, modo, limites, alvo e calibração |
| Agendas | Data ou semana, múltiplos horários, término por hora/duração e fuso |
| Acionamento | Manual/teste com duração, feedback por etapa e parada |
| Histórico | Gráfico/tabela a cada 15 minutos, períodos sem dados e eventos |
| Eventos | Acionou/não acionou/falhou, motivo, ator, GUID, setor e revisão |
| Integrações | URL, método, headers, corpo, regra de resposta e teste |
| Simulação | Relógio, cenários, dispositivo selecionado e injeção de falhas |

Administrador altera configuração; operador consulta e controla manual/teste/parada. Ocultar botão não substitui validação no servidor.

## Portal local

Interface leve separada do portal central, com status, componentes, setores/agendas, rede, integração, usuários e testes. Na primeira fase, usa API do simulador e marca “dispositivo simulado”. Sem SSR, bibliotecas grandes ou dependência de CDN no futuro ESP32.

Controle remoto no portal central atravessa API e protocolo. Portal local envia ao controlador simulado diretamente, reproduzindo operação LAN futura. Ambos utilizam os mesmos comandos e critérios de segurança.

## Monitoramento de 15 minutos

- Controlador atualiza leitura interna em passo menor.
- Publicação histórica a cada 900 segundos, com instante e qualidade.
- Gráfico exibe pontos recebidos; não inventa pontos nos períodos offline.
- Eventos de início/fim/falha aparecem antes da próxima amostra.
- Dashboard mostra idade da leitura. “Sem dados” não significa “não irrigou”.
- “Não acionou” é decisão registrada para uma ocorrência ou janela avaliada.
- Rendimento/alvo: mostrar umidade anterior e leitura posterior disponível, sem atribuir causalidade automática.

## Simulador determinístico

- Múltiplos GUIDs, áreas e configurações isolados.
- seed e relógio virtual configuráveis.
- Pausar, avançar 15 minutos, avançar até agenda ou acelerar dias.
- Controlador processa eventos intermediários: um salto de tempo não pode pular parada/agenda.
- Umidade decai ou aumenta por modelo simples configurável; valores fictícios explicitamente identificados.
- Não modela agronomia, pressão real ou eficiência hidráulica.
- Temperatura controlável por cenário, sem comandar irrigação inicialmente.
- Cada execução oferece manifest com seed, configuração, relógio inicial, comandos e falhas para reprodução.
- Controle temporal e injeção de falhas restritos ao ambiente/credenciais de simulação.

## Cenários obrigatórios

1. Zero sensores e zero válvulas: dashboard vazio válido.
2. Somente temperatura: monitoramento.
3. Estático sem umidade: executar segunda 06:00 por 10 minutos.
4. Estático com umidade alta: ainda executar, pois independente.
5. Dinâmico: abaixo de 30% inicia e em 45% encerra.
6. Híbrido com solo úmido: omitir e registrar motivo.
7. Híbrido com solo seco: executar apenas na janela; encerramento no fim.
8. Data única: não repetir no próximo dia.
9. Teste local/web: duração curta, retorno por etapas e parada.
10. Falha de sensor: bloquear/parar dinâmica; estática continua conforme regra.
11. Offline: testes remotos não executar ao voltar; controle local continua.
12. Timeout/ACK perdido: repetir comando não repete operação.
13. Reinício com ciclo ativo: estado seguro e evento reconciliado.
14. Configuração inválida: não aplicar parcialmente.
15. Histórico de 15 minutos e eventos imediatos com múltiplos dispositivos.
16. Conflito/simultaneidade: rejeição com motivo, sem fila oculta.
17. Resposta de integração inválida e fila cheia: não perder dados silenciosamente.
18. Credencial revogada/usuário sem permissão: rejeitar e auditar.

## Persistência e demonstração

Demonstração pode começar com seed local e banco local reproduzível. Dados não podem desaparecer a cada reload sem indicação de ambiente temporário.
Primeira entrega executável inclui instruções, dados de demonstração, comandos de execução e testes.
Nenhuma tela apresenta leitura fictícia como resposta a erro de produção.
