# Etapa 5 — persistência ASIO, ADSR por voz e Envelope Filter

## Validação do usuário que orienta esta etapa

Em 20/10/2026, Hugo confirmou que o pacote da etapa 4 executou áudio ASIO com fluidez, efeitos e volumes funcionaram, e comandos de teclado, rolagem e botões responderam corretamente. A pendência observada foi a reconexão permanente do ASIO ao reabrir. Essa validação se refere à etapa 4; as modificações abaixo ainda não foram testadas por ele no Windows.

Pedido explícito: concluir estas alterações no código e depois trabalhar o layout; não gerar/publicar novo pacote de teste nesta etapa. O pacote anterior permanece preservado.

## Reconexão ASIO

- A seleção de driver ASIO agora conecta e guarda automaticamente o driver e buffer, sem depender de um clique adicional em Conectar. Alterar o buffer aplica e guarda no driver selecionado.
- O botão Conectar continua disponível e também persiste a conexão bem-sucedida. Ao fechar normalmente, o driver efetivamente conectado é gravado de novo; a escolha visual de um driver que não conectou não substitui a configuração válida.
- A localização permanente vem de `SHGetKnownFolderPath(FOLDERID_RoamingAppData)`, em vez de depender somente da variável de ambiente APPDATA. O caminho relativo de `audio.settings` permanece igual, preservando os arquivos anteriores.
- A reconexão é agendada depois de criar a janela, com no máximo três tentativas locais, espaçadas em 750 ms. Escolher outro driver ou desconectar cancela a restauração pendente.
- O driver é encontrado por nome, independentemente de mudanças no índice de enumeração. Ausência ou duplicidade de nomes exige seleção manual; nenhum outro backend é usado. Falhas de leitura/gravação/conexão são informadas.
- Desconectar encerra a sessão atual; não apaga o último driver salvo, que será tentado na próxima abertura.

Não foi possível reproduzir a falha original no computador do usuário aqui. Foram reforçados os pontos de gravação e inicialização e testada a restauração entre instâncias do host, sem afirmar que a reconexão no hardware já está homologada.

## ADSR nativo por voz

Integração com `sfizz_send_message`, cliente OSC e endereços `/regionN/ampeg_attack`, `ampeg_decay`, `ampeg_sustain` e `ampeg_release`. As quatro funções da API de mensagens estão presentes na DLL recuperada. Caminhos e valores originais de cada região são preparados/consultados ao carregar o candidato, fora do callback.

Mover um knob ADSR ativa a substituição de ADSR apenas naquele favorito. Enquanto não houver edição, a envoltória original do SFZ é mantida, protegendo a sonoridade que o usuário já validou. A flag de ativação e os valores viajam no mesmo snapshot atômico para o callback.

Mapeamento dos controles normalizados:

| Controle | Valor base enviado ao sfizz |
| --- | --- |
| Attack | 0–8 segundos, curva quadrática |
| Decay | 0–8 segundos, curva quadrática |
| Sustain | 0–100% |
| Release | 5 ms–12 segundos, curva quadrática |

As alterações são aplicadas às descrições das regiões para as próximas vozes, preservando a independência das notas anteriores. Não há multiplicação de uma envoltória global sobre o acorde inteiro. SFZs não são reescritos; includes, herança e caminhos de amostra continuam sendo resolvidos pelo próprio sfizz. Modulações CC/velocity que o instrumento já define são preservadas e podem contribuir além dos valores base.

Mudar ADSR não para/reabre ASIO nem carrega outro sintetizador. Mensagens com os mesmos valores não são reenviadas a todas as regiões quando só volume/outro efeito mudou. A API permite restaurar os valores originais de cada região, mantendo seus valores distintos; a exposição desse comando pode ser incorporada na etapa de layout.

## Envelope Filter

O controle Filter passa a ser Envelope Filter, com filtro passa-baixas ressonante controlado pela intensidade do sinal estéreo antes dos demais efeitos. Detector compartilhado entre L/R; ataque de 2 ms e recuperação de 80 ms. Notas mais fortes abrem o filtro; ao perder intensidade ele fecha. Frequência entre 80 Hz e o menor valor entre 16 kHz e 45% da taxa de amostragem; Q fixo de 1,2.

Para preservar a configuração padrão já validada, posição máxima significa totalmente aberto/bypass. Reduzir o controle baixa a abertura de repouso e aumenta a excursão da envoltória. Tabela de coeficientes interpolada, preparada na criação: não há tan/pow nem alocação por sample. Estado do detector/filtro é limpo pelo pânico e pela troca de instrumento, como os outros efeitos. O controle e o MIDI Learn continuam usando o mesmo índice do perfil.

## Perfis

Formato de escrita `LPHS_PROFILE 4`: acrescenta a flag ADSR após o vínculo Program Change. Leitura das versões 1–3 mantida. Nas versões antigas, valores ADSR diferentes dos padrões ativam a substituição; valores intocados preservam o envelope original. Vínculos antigos de notas/CC da v2 continuam sendo descartados, mantendo Program Change 1–20 como contrato dos favoritos.

Favoritos, efeitos e vínculos MIDI continuam dependentes de Salvar perfil; somente configurações de áudio são gravadas automaticamente.

## Validação

- Cinco grupos de testes locais: sessão/perfis, motor/MIDI, DSP, host ASIO simulado e renderização real sfizz.
- `AudioHost` com ASIO4ALL e TASCAM simulados: escolha, gravação, destruição/recriação e reconexão; troca de nome/índice, nome duplicado e driver ausente sem fallback.
- ADSR simulado: valores enviados a todas as regiões e restauração dos valores originais diferentes entre regiões.
- sfizz 1.2.3 real compilado em Linux: readback OSC; medições numéricas de Attack/Decay/Sustain/Release; uma nova voz não reinicia a envoltória da anterior; cauda de release e silêncio final; renderização dos três instrumentos recuperados. Não se trata de teste auditivo no Windows.
- O teste de render offline usa explicitamente freewheeling do sfizz: renderização acelerada precisa aguardar leituras de disco. A aplicação ao vivo continua com esse modo desabilitado. Uma falha inicial do teste acelerado foi resolvida separando esse modo de teste do streaming ao vivo.
- Envelope Filter: sinal fraco/forte, fechamento após um ataque, neutralidade em bypass, continuidade entre blocos e estabilidade nas taxas já suportadas.
- ASan/UBSan nos módulos alterados, com LeakSanitizer desativado pela limitação já registrada do ambiente.
- Compilação cruzada Windows x64 realizada apenas para conferir o código, sem gerar ZIP/instalador, substituir o pacote anterior ou enviar algo ao Google Drive. Não foram usados GitHub Actions.

Fontes primárias da integração: https://github.com/sfztools/sfizz/blob/f5c6e29f23b8057867c08e88f5f6ac6738baa30b/src/sfizz/SynthMessaging.cpp ; https://sfz.tools/sfizz/api/sfizz.h/ . Cabeçalho público e licença preservados em `third_party/sfizz`.

## Próxima etapa

Layout acessível e apresentação dos controles. Depois: teste integrado no Windows das modificações desta etapa. Continuam pendentes importação dos INI originais e troca de timbres sem interrupção/caudas preservadas.

Saldo autenticado de API/billing/Actions continua não consultável pelas ferramentas disponíveis; saldo desconhecido. Publicação prevista em um commit de fontes/documentação, sem binários/amostras e sem Actions.
