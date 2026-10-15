# Firmware experimental 0.5.0 — provisionamento local

15/10/2026. Entrega FW-06 parcial: primeiro cadastro/recuperação USB e manutenção pelo portal, sem recompilar credenciais ou bindings. [Roteiro do primeiro acesso](PRIMEIRO_ACESSO_ESP32.md).

## Comportamento e compatibilidade

Perfil versionado prov_v1 em uma chave NVS, com revisão própria, settings e SHA-256 dos bytes. Esquema estrito, limites de bytes, URL HTTPS somente origem (DNS/IPv4 e porta opcional), CA PEM parseada pelo mbedTLS, senhas distintas e vínculos sem duplicatas por tipo. O hash detecta corrupção acidental; não é cifragem nem autenticação de quem lê a flash.

Sem perfil persistido, provision.h/example continua como base de compatibilidade. Primeiro cadastro USB não exige SSID/senhas no código. Perfil persistido válido prevalece sobre as credenciais/bindings compilados e é carregado antes da configuração lógica. Downgrade para firmware anterior não lê prov_v1 e volta a exigir sua base compilada; não usar como recuperação automática. Registro inválido bloqueia rede/automação; não cai silenciosamente para credenciais antigas nem apaga a chave. GUID, configuração lógica, ocorrências, calibração, fila e auditoria não são removidos. GPIO, presença física, polaridade e habilitação de saídas continuam compilados; migração física das partições não é automatizada.

POST /api/provisioning exige administrador, CSRF, expected_revision da revisão de provisionamento, controlador/lease ocioso e estado elétrico conhecido quando as saídas físicas estão habilitadas. settings é patch: omitido mantém, string vazia remove, admin obrigatório e operador opcional. Campos desconhecidos/GUID/flags elétricos são recusados. Configuração central não impede manutenção administrativa local de rede/acesso/bindings.

A gravação usa uma única operação putString, sem store.clear/remove ou alteração de GUID. Sucesso só após retorno do armazenamento; aplicação apenas no próximo boot. Enquanto isso, snapshots usados pela tarefa de rede ficam imutáveis e o loop não aceita novos acionamentos. API retorna saved/restart_pending e reinício ocorre após aproximadamente 1,5 s. Não há rollback automático se uma senha/rede válida sintaticamente estiver incorreta. Recuperar por USB.

GET /api/provisioning devolve apenas SSID, origem, bindings, flags de presença de segredos, revisão, GUID, estado/IP e janela USB. Não retorna senhas, token ou CA. /api/state acrescenta conexão Wi-Fi/IP/restart_pending. Exportar configuração lógica continua sem segredos. GUI e assistente não imprimem/salvam segredos; nenhuma credencial real usada nos testes.

USB aceita PROVISION? sem limite temporal; escrita PROVISION seguida de envelope {expected_revision,settings} somente nos primeiros 180 s após reset. Janela fechada não reabre no wrap de millis; limite de linha, orçamento por loop e timeout de entrada parcial. Acesso físico USB é a autorização de recuperação, inclusive sem senha antiga. Não equivale a recuperação remota ou proteção contra acesso físico. Fechar outros monitores e reiniciar quando necessário.

A outbox atual não guarda destino por mensagem: trocar/desativar origem exige fila vazia. Não há descarte automático ou redirecionamento de histórico retido; mesma origem pode atualizar token/CA. Permanece pendente fluxo avançado de cancelamento/recuperação de fila bloqueada ao trocar provedor.

## Evidências

- Dez executáveis nativos C++ com UBSan e cinco fixtures de bytes/hash aprovados; política de provisionamento cobre autorização, ociosidade, pendência, revisão/exaustão e bloqueio do destino.
- Adaptador real ArduinoJson 6.21.5 compilado no host com UBSan: roundtrip, leitura sem segredos, patch parcial, desconhecidos, binding malformado e NUL embutido. `python iot/firmware/scripts/test-provision-codec.py`; headers obtidos pelo gerenciador oficial, sem vendoring. CI instala headers com --no-save.
- Quatro testes Python do protocolo do assistente USB aprovados com porta simulada: filtro de logs, UTF-8, recusa e timeout. Não testa USB físico/reset automático.
- 283 testes Node, sintaxe e 26 fixtures/consistência OpenAPI aprovados.
- Chromium 149: 17 cenários da página embutida sobre API HTTP sintética, zero erros JS, inspeção visual desktop/celular sem rolagem horizontal. Inclui campos secretos vazios, revisão conflitante preservando edição, patch sem sobrescrever senha Wi-Fi omitida, limpeza de senha após salvar e bloqueio enquanto reinicia. Relatório VALIDACAO_PORTAL_LOCAL_V05.json. Não executa as rotas WebServer/Basic/NVS reais.
- Snapshot embutido verificado e pre-script preservado. Fontes da GUI aproximadamente 40 KiB em flash; sem uploadfs.

## Build e limites reais

PlatformIO 6.1.19 foi instalado nesta sessão. Tentativa do perfil esp32_matrix e repetição com cadeia de certificados do sistema falharam no download esp32-core-3.3.8.tar.xz com SSLCertVerificationError: unable to get local issuer certificate, antes de compilar. Validação TLS mantida. Nenhum build alvo 0.5.0 confirmado; não mede flash/RAM reais. Os três perfis/padrão+provisionado continuam previstos no CI e precisam ser confirmados em ambiente com download funcional.

NVS de segredos permanece sem cifragem de flash/NVS, secure boot e eliminação comprovada de cópias antigas. HTTP Basic local não cifra credenciais; uso em LAN de confiança. Backup seguro/rotação/revogação central, hardening de produção, migração física e testes de queda de energia/rollback continuam pendentes em FW-06. Não declarar provisionamento de produção concluído.

A primeira bancada do usuário será build/upload, Wi-Fi 2,4 GHz, portal admin/operador e persistência ao reiniciar; verificar tempo de associação/IP, reabertura pelo roteador e recuperação USB. Depois: TLS/NVS/memória, sensores/relés e RS485. Sem dispositivo físico conectado nesta entrega.

Atualização posterior, 0.5.1: downloads liberados e seis builds alvo confirmados; corrigidos offset do aplicativo/imagem auxiliar antes de upload. Use 0.5.1 ou posterior e consulte VALIDACAO_FIRMWARE_V051.md. O bloqueio descrito acima registra a tentativa da 0.5.0; bancada permanece pendente.
