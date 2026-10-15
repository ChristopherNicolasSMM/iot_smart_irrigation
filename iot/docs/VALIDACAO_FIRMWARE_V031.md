# Firmware experimental 0.3.1 — retenção segura de ocorrências

15/10/2026. FW-03a implementado em software; validação física permanece pendente.

## Resultado

O controlador não desabilita mais um conjunto após acumular 128 ocorrências ao longo da vida. O journal guarda registros recentes com instante inicial da janela e um corte temporal monotônico. Todo início de janela no corte ou anterior a ele é inelegível, mesmo após limpeza do cache, reboot, alteração de revisão/hash ou retrocesso de relógio. Isso protege os identificadores removidos sem guardar histórico de deduplicação infinito.

O corte e os registros são gravados juntos na chave NVS `occ_v2`, antes de alterar a RAM ou autorizar saída. Gravação incompleta/falha bloqueia automação. CRC32, tamanho, versão, faixa temporal, slots, chaves e duplicatas são conferidos ao carregar; corrupção não cria um journal vazio automaticamente. CRC não é autenticação contra adulteração.

## Retenção e limites explícitos

- Preserva pelo menos 24 h de inícios recentes, compatível com a duração máxima de uma janela. A compactação começa quando o corte está 25 h atrás do relógio, avançando-o para agora−24 h; depois ocorre no máximo em incrementos de uma hora durante observação contínua.
- A capacidade é **128 registros recentes no dispositivo**, não um teto de execuções acumuladas por conjunto. Com os 64 slots de agenda e configuração estável, o recorte limita o crescimento. Mudanças muito frequentes de revisão ou rajadas que excedam essa capacidade recente continuam falhando de forma conservadora, sem expulsar chaves ainda necessárias.
- O cache do controlador permanece limitado, mas é auxiliar: o journal persistido também é consultado. Compactação limpa só esse cache, sem reconfigurar conjuntos, pausas ou duração monotônica.
- Janelas retiradas continuam abertas para a finalização de uma execução híbrida em andamento; o corte impede novos inícios e não interrompe, por si, o ciclo já iniciado.
- Retrocesso abaixo do corte bloqueia avaliação automática. Não se reduz o corte para tentar recuperar horário. Os comandos manuais explícitos continuam separados da retenção de agendas.
- O blob mede no máximo 2198 bytes, com inteiros codificados explicitamente, sem dependência do alinhamento de structs C++.
- Auditoria `retention` registra migração/compactação por conjunto no anel local existente de 64 eventos. O histórico do portal central não é removido; a auditoria local continua tendo seu limite de sobrescrita.

## Migração

As antigas chaves `occ0`…`occ3` são preservadas, mas deixam de ser restauradas no cache. Os hashes antigos não carregam data: no primeiro relógio válido, o novo journal estabelece um corte no instante atual. Assim, nenhuma janela anterior ou aberta naquele instante é retomada. A gravação do novo corte precisa concluir antes de liberar automação. Se a energia cair antes da gravação, o próximo boot repete a migração com novo corte conservador; se cair depois, o blob completo é carregado.

GUID, configuração, calibração, contas provisionadas, outbox, checkpoint de observação e partições não mudam. Sem apagar NVS ou banco. O novo formato não tem caminho automático de downgrade para firmware antigo que ignore `occ_v2`; qualquer downgrade exige plano de bancada e backup, sem presumir que os registros legados abrangem execuções novas.

O diagnóstico local acrescenta `occurrence_journal_ready`, `occurrence_floor`, `retained_occurrences` e `occurrence_recent_capacity`.

## Evidências de software

- Sete executáveis nativos C++ com UBSan/-Wall/-Wextra/-Werror, cinco fixtures de bytes/hash.
- 16000 acionamentos simulados em 250 dias, 64 ocorrências/dia; compactações, duplicatas e reboot sem cache aproximadamente a cada 97 ocorrências. Total de acionamentos conferido exatamente.
- Rejeição de janela anterior ao corte com hash/revisão diferente; estático e híbrido sem reativação. Execução híbrida já ativa não é parada apenas pela marca de retenção.
- Falhas antes da escrita, após escrita durável com retorno de erro e escrita truncada: saída não acionada. Após reboot, commit ambíguo já persistido mantém a ocorrência consumida, com omissão conservadora.
- Corrupção de cada byte do blob, truncamento e versão inválida com CRC recalculado rejeitados. Corte não retrocede. Capacidade recente excedida não remove registros necessários.
- Migração sem commit não altera o corte em memória nem cria novo journal durável.
- Sintaxe do portal central, 26 fixtures de contrato e 269 testes existentes aprovados.

Build ESP32 desta revisão bloqueado antes de compilar: `python iot/firmware/scripts/check-build.py`, com PlatformIO 6.1.19 via PYTHONPATH temporário, terminou com código 1 no download do core Arduino 3.3.8. Erro: `SSLCertVerificationError: certificate verify failed: unable to get local issuer certificate`. Nenhum dos seis builds foi confirmado e a verificação TLS não foi desabilitada. Nenhuma gravação em placa, ensaio NVS real, memória/latência/desgaste medidos ou teste de campo foi executado. As falhas injetadas usam armazenamento simulado; não substituem cortes reais de energia.

## Próxima entrega

FW-03b: auditoria detalhada de conflitos e recuperação por agenda/ocorrência, incluindo integração ao portal central. O formato acima é um journal de deduplicação, não substitui o histórico de irrigação.
