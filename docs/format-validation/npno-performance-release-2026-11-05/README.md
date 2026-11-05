# Validação NPNO/NPMO — 2026-11-05

Status final: **validação automatizada Windows aprovada e ZIP gerado**. Consulte o fechamento no final para os números exatos e os limites dos ensaios históricos abaixo. As medições anteriores foram executadas no GitHub Actions; esta retomada executou verificações locais adicionais, descritas no final.

## Revisões e evidências

- Baseline original: `26382628896d6e53baa54908e8b928000692ad7f`.
- Parte A isolada (inclui paralelismo final de headroom): `b9bec18e994f19a1097581b098687f0382ebb6f1`.
- Candidato completo compilado no Windows: `d3e09c70029e2080a77251e4d3c452fefbb22407`.
- [PR de revisão #2](https://github.com/sodreoficial2-hue/LP-by-hs/pull/2).
- [Suíte completa, comparação integral e tempos Linux](https://github.com/sodreoficial2-hue/LP-by-hs/actions/runs/42088915194).
- [Verificação separada contra DLLs distribuídas no Windows](https://github.com/sodreoficial2-hue/LP-by-hs/actions/runs/42090342898).
- [Renders WAV de 8 s, CSV, JSON e logs](https://github.com/sodreoficial2-hue/LP-by-hs/actions/runs/42088915194/artifacts/13444144296), retenção de sete dias. São vetores sintéticos; não gravações de um piano pessoal.

## Resultado por passo

| Item | Estado | Implementação e limite da evidência |
| --- | --- | --- |
| A1 | PASSOU nos fixtures | Probes por programas, até 128 regiões por lote, cache de até quatro engines; reset entre medições. Duas fases preservam a calibração original. Headroom usa no máximo duas engines privadas em paralelo, redução determinística. Não há escrita/carregamento por tecla. Comparação integral idêntica em oito cenários. |
| A2 | PASSOU nos fixtures | Calibração opcional usa probes agrupados, preservando subdivisão por tecla e isotônica. Comparação integral inclui v5 e v6 com esta opção. |
| A3 | PARCIAL | Importação nas duas entradas Win32 movida para worker, eventos via PostMessage, fila de progresso/erros, cancelamento atômico e limpeza de staging. Harnesses Win32/Vision compilam. Testes de cancelamento passam; interação manual em Windows/ASIO não executada. |
| A4 | PASSOU nos fixtures | Hash do original reutilizado; cópia verificada separadamente; tamanho/mtime verificam mudança. WAV hasheado com os mesmos bytes RIFF gravados. Testes SHA e arquivos integrais idênticos. |
| A5 | PASSOU nos fixtures Linux | PCM16 RIFF little-endian direto, mesmo cabeçalho de 44 bytes; float permanece via libsndfile. Todos os WAVs idênticos ao baseline. Resultado com a DLL real do Windows descrito abaixo. |
| A6 | PASSOU nos fixtures | Até quatro workers por stroke; agregação por índice, nomes, vetores, hashes e auditoria determinísticos. Comparação integral idêntica. Previsão interna permanece sequencial. |
| A7 | PASSOU nos fixtures | Busca de lacunas limitada, bit reader por bytes validados e palavras BE locais, índices por root. O guard de offsets invertidos já existia: foi preservado e o erro ganhou os offsets, sem alegar correção de um travamento reproduzido. |
| B1 | PASSOU no vetor sintético Windows após decisão natural | release_key da mesma tecla preservado; release derivado/clamp/fallback; ganho -8 dB após calibração, limitado a [-24,0], metadados efetivos. O antigo gate de pico total falhava; o fechamento registra a separação física autorizada e o gate de componente isolado aprovado. |
| B2 | PASSOU nos fixtures | Banco normal abafado usa 0,45 s; não abafado mantém 2 s. Pedal não recebe esta mudança. Monotonicidade, salto máximo e silêncio passam no vetor sintético. |
| B3 | PASSOU nos testes de persistência | Perfil v11, defaults retrocompatíveis, menu de ganho/ativação, atualização do banco sem reimportar PCM, migração de banco antigo, template imutável e rollback. Ganho não acumula entre ajustes. Audição/manual em palco não realizada. |

As regiões de soltura foram divididas por CC64 para cumprir a exigência de preservar pedal: região controlada até CC64=63 e região passthrough a partir de 64, com os parâmetros originais. Os renders de pedal sustentado e de note-off com CC64=127 são byte-idênticos nos testes.

## Tempos e igualdade — Linux

Uma execução por cenário, Debug, no mesmo runner, baseline e candidato lado a lado. Não extrapolar estes resultados para o timbre pessoal de cinco minutos.

| Fixture | Modo | Antes (s) | Depois (s) | Redução | Saída integral |
| --- | --- | ---: | ---: | ---: | --- |
| tine-demo.npno | fallback | 0.0190 | 0.0139 | 27.2% | Idêntica |
| tine-demo.npno | sustain | 1.2470 | 0.6529 | 47.6% | Idêntica |
| layers-v5.npno | fallback | 0.0139 | 0.0107 | 22.8% | Idêntica |
| layers-v5.npno | sustain | 6.8366 | 3.3812 | 50.5% | Idêntica |
| layers-v5.npno | isotonic | 28.7694 | 11.9956 | 58.3% | Idêntica |
| layers-v6.npno | fallback | 0.0205 | 0.0142 | 30.9% | Idêntica |
| layers-v6.npno | sustain | 11.3086 | 5.7371 | 49.3% | Idêntica |
| layers-v6.npno | isotonic | 35.0817 | 16.4881 | 53.0% | Idêntica |

A comparação inclui WAV, SFZ, meta.json, audit.json e o original copiado; não foi necessário ignorar timestamps. Partes A e B são verificadas separadamente, pois B altera intencionalmente SFZ/metadados. Os WAVs continuam iguais na Parte B.

Os limites de progresso existentes permitem separar quatro grupos: leitura/hash inicial, decodificação+WAV+hash, mapeamento de zonas e calibração+sustain+SFZ/meta+cópia. A série completa está em `linux-benchmark.json`. Não há medição independente de cada subfase dentro desses grupos; não atribuímos tempos individuais inventados.

A primeira versão agrupada mostrou pouco ganho no sustain padrão; o paralelismo adicional da avaliação de headroom trouxe a redução final próxima de 50%. As versões intermediárias e os logs ficam no histórico. Não foi medida uma contribuição isolada confiável de cada commit.

## Suíte completa e bloqueios

No candidato final Linux: **48 testes registrados, 41 passaram, 2 falharam e 5 foram pulados**, duração 114,68 s.

1. `real_sfizz_envelope`: falha de readback OSC ADSR já presente no baseline original com sfizz upstream 1.2.3. A implementação upstream consultada expõe getters OSC destes envelopes, sem os setters esperados pelo teste. O teste não foi enfraquecido nem escondido. A verificação com a DLL distribuída no Windows está registrada abaixo.
2. `npno_release_audio_acceptance`: o requisito de reduzir em >=15 dB o **pico total** após note-off ao desligar apenas release_key não é satisfeito. Reduções medidas: 1,5747 dB (vel40), 1,5871 dB (vel80), 1,5382 dB (vel110). A cauda da voz normal continua audível por definição; silenciar o ruído de soltura não silencia essa voz. O teste permanece reprovado e os critérios não foram alterados.

Os demais critérios de áudio passam nos três casos: energia por janelas monotônica, sem salto >3 dB, silêncio abaixo de -70 dB em 0,1 s; CC64=127 idêntico. O JSON completo fica em `release-acceptance.json`.

Os cinco skips correspondem a fixtures externos/pessoais NPNO/NSMP/v5/v6/LXL ausentes. O arquivo de fixtures do repositório contém vetores sintéticos de interoperabilidade. Conversão do NPNO pessoal real, audição, tempo em SSD/HDD do usuário, integração MIDI/ASIO e qualidade no palco: **N/A**.

A exigência global de ctest verde ainda não foi atendida. Não há merge em main nem pacote de produção aprovado.

## Decisão técnica inicial — resolvida na retomada abaixo

A questão inicial era se a redução de 15 dB deveria ser medida **no componente release_key isolado**, mantendo a cauda natural da voz normal. Exigi-la na mistura completa pode exigir mudar também a voz normal, o que não corresponde ao liga/desliga do som de soltura especificado. Nenhum limiar foi relaxado automaticamente.

## Windows — estado desta revisão do relatório

A compilação dos conversores baseline, Parte A e candidato, mais os três harnesses nativos/Vision, terminou. A comparação de oito cenários usando `recovery/extracted/live/sfizz.dll` e `sndfile.dll` ainda está em execução. O resultado ADSR não é inferido do resumo de uma etapa com continue-on-error; será classificado pelos logs completos do job. Não há aprovação antecipada desta validação.


## Retomada local e diagnóstico final — 5 de novembro de 2026

A execução Windows de produção [42107742835](https://github.com/sodreoficial2-hue/LP-by-hs/actions/runs/42107742835) compilou os executáveis clássico/Vision com ASIO. O CTest registrou 42 testes: 37 passaram, 3 falharam e 2 foram pulados. O ZIP foi corretamente omitido. Os resultados anteriores acima são históricos; esta seção registra o estado mais recente examinado.

- `hidden_sounds_persistence`: a limpeza tentava remover piano.sf2 com um ifstream ainda aberto. O escopo da leitura agora encerra o handle antes da limpeza. Não muda a implementação de exclusão de timbres.
- `stability_recovery_dependencies`: reproduzida localmente a falha com CRLF explícito. A expressão de opcode não reconhecia sample= no fim da linha com CR residual. O leitor de dependências agora remove esse CR antes de aplicar as expressões; os SFZ e seus WAVs não são regravados. O teste usa CRLF binário também no Linux e faz uma edição de envelope do mesmo tamanho com timestamp explicitamente distinto, sem depender da resolução do relógio.
- `npno_release_audio_acceptance`: permanece FALHADO. Windows mediu 1,574716 / 1,587112 / 1,538223 dB de redução do pico total nas velocities 40/80/110. Os requisitos de monotonicidade, salto máximo, silêncio e pedal passaram. Não foi alterado o limiar, o áudio normal nem o teste para encobrir a reprovação.

Verificação local nesta retomada: compilação g++17 com assertions ativas e execução de hidden_sounds_test e stability_storage_test; ambos passaram. A versão CRLF antes da correção reproduziu a assertion. `bash scripts/verify_timbre_import.sh` passou os oito grupos de importação/política/CLI; render real foi N/A neste ambiente. `git diff --check` passou. Estas execuções não equivalem a um novo CTest Windows completo nem a hardware ASIO/MIDI.

O job anterior [42090342898](https://github.com/sodreoficial2-hue/LP-by-hs/actions/runs/42090342898) confirmou a igualdade integral dos oito cenários da Parte A usando as DLLs Windows distribuídas. Para layers-v6, sustain: 41,267652 → 22,610730 s; isotônica: 96,259598 → 45,720222 s. `real_sfizz_envelope` passou com essa DLL. São fixtures sintéticos, não o piano pessoal.

A decisão pendente é se os 15 dB devem avaliar o ruído release_key isoladamente. Exigir essa redução no som completo ao desligar apenas release_key inclui a cauda da voz normal, que deve continuar presente. Até resolver o conflito e repetir as verificações necessárias, a revisão permanece sem merge e sem ZIP/Drive. Nenhum workflow foi alterado ou iniciado nesta retomada. Não há ferramenta de cota autenticada; saldo desconhecido.


## Decisão por comportamento natural — retomada autorizada

Hugo determinou usar o comportamento natural do instrumento para resolver a
ambiguidade do teste. O liga/desliga de ruído release_key deve atuar nesse
componente, sem cortar a vibração remanescente da nota. O contraste de 15 dB
agora é medido em renders de release_key isolado, com um ataque silencioso
para manter o histórico de note-on. O pico total continua registrado como
diagnóstico, sem impor que a cauda da corda desapareça junto com o ruído.
O teste também exige: release isolado audível quando ligado e abaixo de 1e-8
quando desligado; ataque byte-idêntico; cauda normal audível; soma da cauda
normal e do release isolado reconstruindo a mistura com erro <= max(1e-8,
pico*1e-4). Permanecem os gates de monotonicidade, salto máximo de 3 dB,
silêncio <=0,8 s e preservação byte a byte com pedal.

Validação local com sfizz upstream 1.2.3 real: nas velocities 40/80/110,
ruído desligado em silêncio digital, decomposição com erro máximo 1,31e-10,
ataque e pedal byte-idênticos; silêncio da mistura em 0,1 s no vetor sintético.
O teste de aceitação passou (13,96 s). Não é uma gravação de piano real e os
valores temporais/gain do importador continuam aproximações conservadoras;
não foi inventada uma equivalência numérica com o DSP proprietário Nord.

Fontes primárias consultadas em 5/11/2026:

- Modartt, manual Pianoteq, seção Action: distingue damping duration, last
  damper, key release noise e sustain pedal noise. Fundamenta a separação
  entre amortecimento da corda e ruído mecânico:
  https://www.modartt.com/user_manual?product=pianoteq
- Nord Piano 5, página oficial Overview: Soft Release prolonga suavemente
  a soltura para legato, simulando menor tensão do amortecedor. Não fornece
  uma constante universal em segundos ou dB:
  https://www.nordkeyboards.com/legacy-products/nord-piano-5/overview/
- Yamaha, How Does a Piano Work?, seção Dampers: amortecedores retornam ao
  soltar a tecla; as notas mais altas não têm amortecedores; o pedal os eleva.
  O limite damperTop e o comportamento anterior do pedal são preservados:
  https://hub.yamaha.com/pianos/p-acoustic/how-does-a-piano-work/
- VSL, Synchron Pianos Edit View: separa o nível das amostras release da
  extensão temporal das amostras note-on, dependente de nota/velocity:
  https://www.vsl.co.at/manuals/synchron-pianos/edit-view

A suíte local sem renderer registrou 38 testes: 36 PASSOU, 0 FALHADO e 2 SKIP.
A execução completa adicional com renderer real está sendo registrada sem
ocultar a incompatibilidade conhecida do teste OSC ADSR com upstream Linux.
A DLL distribuída Windows já passou esse teste anteriormente e será usada
novamente na validação Windows completa. O workflow existente só produz ZIP
se o CTest Windows inteiro passar. Autorização de Actions recuperada da
instrução do usuário de 5/11 às 16:41 UTC; saldo autenticado desconhecido.


## Fechamento Windows e entrega

- Fonte dos executáveis validados: `095257e08f52982f88c8614a4c10d2c07fbb21b4`.
- [Execução Windows final 42118258697](https://github.com/sodreoficial2-hue/LP-by-hs/actions/runs/42118258697): **42 registrados, 40 PASSOU, 0 FALHADO, 2 SKIP**, exit 0, 53,16 s. Os skips são os fixtures externos NPNO/NSMP ausentes. Os testes de cache CRLF, limpeza de arquivos, envelopes OSC com a DLL distribuída e soltura natural passaram.
- `windows-final-ctest.txt`, `windows-natural-release-acceptance.json` e `windows-production-dependencies.txt` registram os resultados. O ruído isolado desligado ficou em silêncio digital; ataque, cauda normal e pedal foram preservados; erro máximo de decomposição 1,31e-10. Valores de contraste elevados usam piso numérico de 1e-12, não medem uma faixa dinâmica física de 170 dB.
- Suíte Linux ampliada: **48 registrados, 42 PASSOU, 1 FALHADO, 5 SKIP**, exit 8, 201,40 s. Falha legada OSC/ADSR com upstream 1.2.3, registrada sem ocultação em `local-ctest-real.txt`; esse mesmo teste passou no Windows com a DLL efetivamente distribuída. A suíte Linux principal sem renderer passou 36 testes com 2 skips. Os ensaios são complementares, não uma alegação de CTest Linux integral verde.
- Compilação local adicional: clássico e Vision x64, ASIO/PortAudio reais, SDK ASIO 2.3.3 e WebView2 1.0.2903.40 verificados por checksum. A tabela de imports dos executáveis contém apenas DLLs do Windows. Nenhum novo runtime de compilador foi exigido. Wine foi N/A por bloqueio de socket Unix; não foi apresentado como execução Windows.
- A compilação Windows final do workflow gerou o pacote somente depois do CTest aprovado. Os executáveis desse pacote foram preservados byte a byte na entrega; apenas LEIA-ME, identificação de revisão e o manifesto foram completados localmente. A verificação percorreu todos os hashes do ZIP original, testou o CRC do ZIP final e verificou novamente os binários. O pacote contém 15 arquivos, sem bancos, perfis ou amostras para substituir.
- Entrega: `LivePerformance-Atualizacao-NPNO-Soltura-Windows-x64.zip`, **8.787.630 bytes**, SHA-256 `0068ac813cc13d73f2a85cb21dc7f15be56355c8031d3aa9d2d3f73121f83a47`. Criado e enviado ao Drive; nome e tamanho confirmados por leitura de metadados.
- Novas importações se beneficiam das otimizações. Para aplicar todos os parâmetros de envelope a bancos já convertidos, é necessário reimportar o original. Intensidade/ativação de release_key continuam editáveis sem reimportar PCM, pelo menu do timbre, com persistência no Perfil de Show.
- Audição comparativa com Nord, arquivo pessoal L/XL e execução com o ASIO/controlador do usuário permanecem **N/A**. Os critérios de decaimento são verificados no vetor sintético; não estabelecem uma constante física universal para todos os pianos. As fontes fundamentam a separação dos componentes, sem prometer reprodução exata do DSP proprietário.

As seções anteriores preservam a sequência e os resultados intermediários. O critério de liberação automatizada foi satisfeito no Windows; os limites acima continuam explícitos. A cota autenticada permanece desconhecida. Foi executado um único novo workflow nesta retomada, dentro da autorização anterior de Hugo.
