# Entrega v0.4.0 — dias/horários e propriedades

13/10/2026. Solicitação: seleção dos dias como checkboxes, com horários diferentes por dia, junto das próximas etapas do portal.

## Agendas

- Sete checkboxes com nomes de segunda-feira a domingo.
- Cada dia marcado tem seu próprio início e término por duração ou hora final.
- Até oito horários por dia, com IDs de ocorrência independentes; até 32 agendas por dispositivo.
- Dias desmarcados não executam.
- Disponível para modo estático e janelas híbridas; o dinâmico continua independente de agenda.
- Datas específicas continuam com data/início/término únicos por agenda.
- Horários que cruzam meia-noite têm indicação explícita de dia seguinte.
- Agendas antigas com weekdays e horário comum continuam aceitas. O editor converte para day_times ao publicar, preservando horários/durações. Não há alteração automática do agendamento aplicado na migração.
- Exemplo completo em examples/weekly-per-day.json: segunda 06:00 e 18:00, terça 16:30–16:40 e sexta 07:00.

## Propriedades e áreas

- Cadastro/edição de propriedades com nome e fuso; cadastro/edição de áreas com IDs estáveis.
- Seleção de propriedade e dispositivo nos filtros superiores.
- Dispositivos associados a uma propriedade e a uma área dessa propriedade.
- Publicação de configuração valida que a área pertence à propriedade selecionada.
- Eventos conservam nome/contexto ocorrido; renomear área não reescreve eventos anteriores.
- GUID permanece estável; transferência de dispositivo entre propriedades e exclusão de entidades ficam fora desta entrega.

## Acesso

- Conta admin original é administradora do sistema; mantém acesso inicial à propriedade principal e às que cadastrar.
- Administradora do sistema cadastra usuários e concede vínculo/perfil por propriedade.
- Administrador de propriedade edita componentes, agendas, rede, sensores e integrações da sua propriedade.
- Operador monitora e aciona/paralisa setores autorizados.
- Estado, leituras, eventos, fila e revisões são filtrados na API por propriedade. GUID externo retorna 404, sem revelar se existe.
- Alterações de perfil são verificadas em cada requisição; sessões abertas passam a obedecer o novo vínculo/perfil.
- Token de máquina continua vinculado ao GUID; autenticação humana não substitui credencial de dispositivo.
- Relógio do laboratório é global; somente administradora do sistema avança/pausa/altera velocidade. Isso evita que administradores de uma propriedade alterem simulação de outra.
- Gestão de contas tem criação e vínculo/alteração de perfil. Redefinição de senha, exclusão e revogação de vínculo ficam pendentes.

## Migração

Tabelas properties, areas e memberships criadas automaticamente; users recebe system_admin. Laboratório existente é vinculado à Propriedade principal; nomes de áreas anteriores são reaproveitados. Configurações, GUIDs, senhas, histórico, perfis de envio e filas são mantidos. Agendas aplicadas não são reescritas.

Não executar duas instâncias com o mesmo banco. Fazer backup com processo parado antes de atualizar. Sem apagar data ou executar SQL manual.

## Verificação

Node 24.19.0; npm run check e npm test: **75 testes aprovados**, incluindo 59 anteriores, 8 de agendamento e 8 de propriedades/migração. Cobertura: horários distintos por dia, múltiplos horários no mesmo dia, independência de ocorrência entre agendas, dias desmarcados, híbrido, compatibilidade legada, filtro de histórico/estado, acionamento cruzado negado, área de outra propriedade rejeitada, perfis em sessão aberta e migração.

As referências de elementos HTML/JS são verificadas; renderização e cliques em navegador permanecem para validação local do usuário. Não afirmar validação visual automatizada.

## Pendências

- Políticas completas de DST, relógio inválido e agendas perdidas (AT-34).
- Protocolo completo de máquina: consulta/ACK de configuração e comandos pelo ESP32.
- Retenção, limpeza e encriptação de segredos em banco para produção.
- Firmware e validação elétrica/hidráulica continuam em etapa posterior.

A aplicação ainda usa um único processo/SQLite para o laboratório. O isolamento é aplicado nas APIs humanas; acesso direto ao banco pelo administrador do host é externo a esse isolamento. O gate integral da fase 1 permanece aberto.
