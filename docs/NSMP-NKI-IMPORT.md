# Reconhecimento e conversão NSMP/NKI para SFZ + WAV

Esta etapa substitui o bloqueio Nord e a saída SF2 da preparação anterior por um **decodificador C++ NW1 funcional e saída SFZ + WAV**. Base pública anterior: `347e32176754c6d906e9b3456b41f39556c729b6` (criador automático SF2/VU e demais funções aprovadas). Todo o motor e DSP continuam intactos.

| Entrada | Resultado |
|---|---|
| NSMP codec1/OG e codec2 | NWS/map9 ou map10, NW1 24 bits, WAV mono/estéreo e mapa SFZ |
| NSMP3 | NSMP/map12 ou map14, NW1 32 bits sample-interleaved; envelopes CBIN 0 e 1 |
| NSMP4 | NSMP/map21, NW1 word-interleaved por canal; zonas com múltiplos strokes |
| NKI com WAVs externos | Etapa A: WAVs reais validados/copiados; uma região SFZ por WAV, mapa previsível aproximado |
| NKI monolítico/sem WAVs | Diagnóstico explícito e seleção de pasta; sem inventar áudio |
| SFZ/SF2 existentes | Fluxos preservados; importador SF2 compartilha exatamente o corpo anterior |

Não se afirma identidade completa com o som de um Nord/Kontakt. Decodificação e mapeamento são efetivos, mas há parâmetros não decodificados e incertezas de ganho documentadas. Nenhum novo SF2 é gerado: os recursos sem equivalência fiel permanecem no original e sidecar, pois SF2 também não os resolve.

## Arquivos novos (em relação à base pública)

- `src/timbre_format.hpp`, `src/timbre_format.cpp`: detecção, SHA-256, sidecar e importação transacional.
- `src/nsmp/nsmp.hpp`: tipos, limites e APIs internas.
- `src/nsmp/cbin.cpp`: envelope, geração e raiz NWS/NSMP.
- `src/nsmp/nsmp_sections.cpp`: seções, mapas v9/v10/v12/v14/v21 e linhas por tecla.
- `src/nsmp/nsmp_codek.cpp`: NW1, localização do stream, canais e loops.
- `src/nsmp/nw1_dsp.cpp`: nível, detune e taxa de decay.
- `src/nsmp/bitstream.cpp`: campos MSB-first.
- `src/nsmp/checksum.cpp`: CRC32 e CRC16.
- `src/nsmp_to_sfz.cpp`: adaptador Nord → WAV/SFZ + metadados.
- `src/nki_to_sfz.cpp`: Etapa A NKI → WAV/SFZ.
- `src/sfz_conversion.hpp`, `src/sfz_conversion.cpp`: gravação WAV por libsndfile, regiões SFZ e preservação transacional.
- `src/timbre_convert_main.cpp`: conversor offline de linha de comando.
- `tests/timbre_import_test.cpp`: regressão e comparação diferencial com OpenNord.
- `tests/nsmp_real_fixture_test.cpp`: teste opcional de arquivo externo, explicitamente pulado quando ausente.
- `scripts/generate_nord_golden.mjs`: gerador reprodutível via os algoritmos originais TypeScript.
- `third_party/opennord/LICENSE`, `third_party/opennord/NOTICE.md`: licença completa, crédito e rastreabilidade.
- `docs/NSMP-NKI-IMPORT.md`, `docs/NSMP-NKI-IMPORT.patch`: relatório e diffs dos arquivos existentes.
- `tests/fixtures/nord/README.md`, `manifest.json`, `golden.txt`.
- Seis instrumentos: `workshop-og.nsmp`, `workshop-v2.nsmp`, `workshop-v3.nsmp3`, `workshop-v4.nsmp4`, `workshop-v3-format0.nsmp3`, `workshop-v3-map12.nsmp3`.
- Treze vetores PCM: `.stroke0.pcm` e `.stroke1.pcm` para cada instrumento acima, mais `workshop-v4.nsmp4.stroke2.pcm`, todos sob `tests/fixtures/nord/`.

Os antigos `src/nsmp_to_sf2.cpp` e `src/nki_to_sf2.cpp` existiam apenas na etapa local anterior e foram substituídos. Não eram parte da base pública aprovada.

## Diffs dos existentes

[NSMP-NKI-IMPORT.patch](NSMP-NKI-IMPORT.patch) contém os diffs precisos sobre `347e321` de:

| Arquivo | Mudança |
|---|---|
| `src/instrument_catalog.hpp` | Extensões aceitas ampliadas; original mantém identidade; resolvedor entrega `.converted.sfz` somente para tocar |
| `src/win32_app.cpp` | Filtro aditivo; conversão antes do cadastro; diálogo Samples; seleção/favoritos/camadas usam o resolvedor; cópia de caminhos tocáveis para calibração |
| `CMakeLists.txt` | Biblioteca offline com os novos módulos, ligada às duas interfaces e harnesses; CLI; testes por libsndfile |
| `src/catalog_edits.hpp` | Exclusão manual também reconhece os originais novos, com as guardas já existentes |

Header `LPHS_BANK 1`, caminhos relativos/sem `..`, duplicidades e demais validações permanecem. O motor, AudioHost, SF2, DSP, gravador SF2 antigo e biblioteca de timbres não são reescritos. A mudança de calibração é uma cópia de referências no chamador Win32; o algoritmo de calibração não mudou.

## Referência e atribuição

Fonte única do codec: [OpenNord](https://github.com/simonflore/opennord), commit **67ba1b657ff65a803d568f0ba1fba08598fdc830**, AGPL-3.0-or-later. Veja [NOTICE](../third_party/opennord/NOTICE.md) e [licença completa](../third_party/opennord/LICENSE). Os arquivos portados preservam SPDX e referências às funções originais. Crédito não substitui os termos da AGPL; o pacote inclui fonte correspondente, alterações de build e gerador dos testes.

Correções de nomes/papéis na solicitação: o arquivo upstream é `nsmp-codec.ts`, não `nsmp-codek.ts`; `nw1-dsp.ts` contém matemática de nível/afinação, enquanto o decoder preditivo está em `nsmp-codec.ts`. Não havia `docs/NSMP-CODEC.md` na árvore fixada. A antiga cadeia de blocos de **programas** não é usada para amostras.

## Tabela NSMP: campos e fontes

Todos os offsets a seguir vêm do commit fixado. “Validado” abaixo significa comparação com os arquivos de workshop gerados pelo encoder original e decodificados pelo TypeScript original; **não significa confirmação em hardware ou amostra externa independente**.

| Seção/campo | Offset / encoding | OpenNord: arquivo.função | Estado C++ |
|---|---|---|---|
| CBIN magic | `0x00`, ASCII `CBIN` | clavia/cbin.hasCbinMagic | Validado; rejeita ausência |
| formatType | `0x04`, u32 LE; 0/1 | clavia/cbin.cbinFraming | Validado; >=2 recusado |
| Tipo de arquivo | `0x08`, 4 caracteres, padding NUL/espaço | clavia/cbin.fileTypeTag | Exige tag minúscula `nsmp`, como upstream |
| bank / slot | `0x0C` / `0x0E`, u16 LE | clavia/cbin.readCbinHeader | Lidos; sem alterar slots do LP |
| Categoria / versão | `0x10` / `0x14`, u32 LE | clavia/cbin.readCbinHeader | Categoria preservada em bytes; versão determina codec |
| CRC32 | `0x18`, u32 LE, sobre `[44:EOF]` | clavia/checksum.verifyNs4Checksum | Verificado; mismatch registrado, não bloqueia |
| Registro NSM | `0x2C`, até EOF | clavia/cbin.cbinFraming | Limites estritos |
| Registro legado | `0x18`, até EOF-2 | clavia/cbin.cbinFraming | CRC16 LE final sobre arquivo sem trailer |
| CRC16 | Poly 0x1021, init 0xFFFF | clavia/crc16.verifyTrailerChecksum | Validado com arquivo gerado upstream |
| Raiz NWS / NSMP | Em recordOffset; 3/4 bytes ASCII | clavia/sample-identify.nsmpLayout | Determina cabeçalho de seção 9/12 bytes |
| Codec | Versão low-u16 /100; versões 8/0xFFFF → família OG | clavia/sample-identify.identifyNsmp | Gerações 1/2/3/4; extensão contraditória recusada |
| Cabeçalho seção 3/4 | tag u32 BE; version u32 BE; size u32 BE | ns4/nsmp.parseNsmpSections | Percorre exatamente o registro, com bounds |
| Cabeçalho seção OG/2 | tag u24 BE; version u16 BE; size u32 BE | ns4/nsmp.parseNsmpSections | Mesmo percurso, exclui trailer CRC16 |
| hdr: nome | Corrida ASCII imprimível do payload | ns4/nsmp.readName | Nome no sidecar/catálogo |
| hdr: flag fábrica | payload+8, 2 bytes | ns4/nsmp.hdrFactoryFlag | Bytes preservados; não modifica o áudio |
| cat | payload+0 / +1, categoria/subcategoria u8 | ns4/nsmp.readSampleCategory | Raw preservado no sidecar; categoria LP não sobrescrita |
| map: global | +0 nível u24 / +3 detune s24 BE | ns4/nsmp.readGlobalLevelDetune | Nível Q20; detune via dsp2Detune |
| map: por tecla | Global+6; 128 linhas de 6B (codec3), 10B (codec4); legado+15 quando tabela contada | ns4/nsmp.readPerNoteRows | Regiões divididas por tecla quando nível/afinação mudam |
| Zonas map9 | `[gid:u24][level:u24][keyHigh:u32][strength:u16]`, 12B | ns4/nsmp.parseLegacyZoneRecords | Root vem do stroke; low deriva por tiling |
| Zonas map10 | Tabela após `15+128*6`, count u24; record 15B, gid+0, gain+3, detune+6, high+9 | ns4/nsmp.parseCodec2ZoneRecords | Layout contado e fallback upstream |
| Zonas map12 | Record 11B; high+3; marcador+6; gid u32+7 | ns4/nsmp.parseCodec3V12ZoneRecords | Maximal-run + desempate pelo final, como upstream |
| Zonas map14 | `6+128*6+1` no payload; count anterior | ns4/nsmp.readNsmpZones | Record 16B abaixo |
| Zonas map21 | `6+128*10+32`; count anterior | ns4/nsmp.parseCodec4ZoneRecords | Layout simples, múltiplos strokes e fallback upstream |
| Record simples | root+0, high+1, low+2, mode+3, playback+4, oneShot+5 | ns4/nsmp.readZoneRecord | Condições MIDI; mode1 → pitch_keytrack=0 |
| Stroke / velocity do record | gid u32+8; lovel+14; hivel+15 | ns4/nsmp.readZoneRecord | IDs completos, sem truncar ao low byte |
| Record multilayer | Cabeça 8B com count u16+6; entradas 8B `[gid:u32][strength:u16][velMin][velMax]` | ns4/nsmp.walkMultiStrokeZones | Uma região por camada, sem perder zonas extras |
| stk id/root/rate/canais | id u32+0; root u8+5; rate u16 BE+6; canais u8+8 | ns4/nsmp.strokeGlobalId/strokeSampleRate/strokeChannelHint | Rate fallback 35002; estéreo preservado |
| Cabeçalho NW1 | count bits0..13; order14..17; decay18; width19..22 +1; linear23 | ns4/nsmp-codec.readBlockHeader | Ordens 0–7; stream24 para OG/2, 32 para 3/4 |
| Residuais/predição | MSB-first signed, Pascal alternado, ring32 por canal | ns4/nsmp-codec.decodeStroke | Igualdade inteira com todos os PCM golden |
| Empacotamento codec4 | Streams por canal intercalados por palavra32 | ns4/nsmp-codec.decodeStroke | Sentinela final sem residual tratada explicitamente |
| Início do stream | Busca guiada por headers, ranges e consumo da seção | ns4/nsmp.decodeStrokeSection/findStrokeStopAt | Mesmos candidatos do upstream, com rejeição de truncamento |
| Ponteiros de loop | stk+0x12/+0x1B/+0x24/+0x2D, u32 BE, palavras comprimidas | ns4/nsmp.readStrokeRegions/strokeLoop | Converte por marcas de segmentos; não confunde com frames |
| Loop decay | stk+0x28, s24 / envelope rate21700 | ns4/nsmp.loopDecayDbPerSecond | Valor no sidecar; sem envelope inventado |
| sty / meta / demais bytes | Payload original das seções | ns4/nsmp + fonte original | Hex com tag/versão/offset no sidecar |
| Normalização do áudio | Pico compartilhado entre canais de cada stroke | ns4/nsmp-audio.normalizeChannels | Usada na saída; ganho proprietário do header ainda TODO |

## NKI Etapa A e pendências

| Dado | Regra aplicada | Pendência/limite |
|---|---|---|
| Detecção | `.nki`, primeiros bytes registrados | `signature_confirmed=false`; TODO com arquivo real, sem bloquear WAVs |
| Amostras | Samples junto do NKI/INSTRUMENTS ou pasta indicada | Referências internas NKI não lidas; Samples compartilhada pode conter outros instrumentos |
| Notas | C4=60, A#3/Bb1, `29_F1_P`, midi60/note60/key60 | Convenção diferente pode deslocar oitavas |
| Teclas | Ponto médio entre raízes conhecidas | Não representa splits Kontakt reais |
| Velocity | Nome vNN/velNN quando houver; pontos médios por root | Sem nome: faixa inteira; não inventa camadas dinâmicas |
| Nota desconhecida | Uma região full-range por WAV, `pitch_keytrack=0` | Sem afinação inferida; regiões podem soar juntas |
| Áudio | Validação por libsndfile, cópia byte a byte | WAVs originais preservados, sem normalizar/quantizar/regravar |
| Caminho no SFZ | `Samples/sample-N.wav`, relativo e ASCII | Origem/nome real/hash no sidecar |
| NKI monolítico | Mensagem “NKI sem amostras externas localizadas; decodificação integral pendente” | Sem NCW/NKX/NKC/criptografia |
| Etapa B | TODO explícito | Regiões, loops, tune, scripts KSP, grupos, keyswitches, round robin, articulação, envelopes, filtros, efeitos e modulações reais |

A ausência de assinatura confirmada não transforma a Etapa A em parser Kontakt. Ela toca **os WAVs companheiros**, com decisões explícitas. Não se afirma que todos pertencem ao NKI selecionado nem que a articulação é fiel.

## Arquivos preservados e fluxo

Uma importação bem-sucedida prepara e só então publica uma pasta dedicada:

```text
<sha256-original>-<sequência>/
  MeuTimbre.nsmp4
  MeuTimbre.nsmp4.converted.sfz
  MeuTimbre.nsmp4.meta.json
  Samples/
    stroke-0000-1.wav
```

O catálogo/Meus Sons aponta para o **original**. O resolvedor entrega o SFZ derivado para seleção, favoritos e Performances; não passa o NSMP/NKI para o motor. `bank://id` continua estável quando original, sidecar e derivados são movidos juntos. O teste salva/reabre um favorito e move esse banco para comprovar a resolução.

Os conversores diretos exigem uma pasta nova e não sobrescrevem uma saída existente. A importação Win32 usa diretório `.staging`; erro de decode, gravação, hash ou metadata não cadastra item parcial. O sidecar possui nome, versão/codec, checksum, SHA-256 do original e WAVs, zonas, loops, ganhos, afinação, decisões/limitações, origem dos WAVs NKI e bytes opacos preservados.

Renomear/mover/ocultar continuam pela identidade original. Deletar exige a confirmação existente e remove o original selecionado; não apaga amostras/sidecars em cascata. Sem original, o resolvedor recusa tocar o derivado órfão. Reimportar é necessário para reconverter alterações no original; não há SHA-256 por nota nem trabalho novo no callback.

## Testes e força da evidência

1. **Framing por construção:** CBIN mínimo + seção NSMP, tags/versions/sizes/offsets e leitura/escrita de bits MSB.
2. **Comparação diferencial:** seis fixtures geradas pelos writers originais OpenNord, com PCM/zonas/loops esperados extraídos pelo decoder TypeScript original, sem usar o C++ como seu próprio oráculo.
3. **Áudio:** cada inteiro decodificado é comparado; WAV float32 lido pela libsndfile real é comparado à escala de referência, incluindo estéreo e loops.
4. **NKI Etapa A:** pacote mock com nomes C4/v20/v100/F1 e WAV desconhecido; uma região por amostra, caminhos relativos, byte preservation, SHA e rollback.
5. **Regressões LP:** catálogo, favorito portátil, perfis, MIDI, cache/transições, DSP e testes existentes.
6. **Fixture externa real:** teste separado e explicitamente `Skipped` quando `LP_NSMP_REAL_FIXTURE` não está definido.

Foi localizado [jRhodes4d-st.nsmp3](https://github.com/jlearman/nord-jRhodes3d/blob/main/jRhodes4d-st.nsmp3), comunidade, 7.156.460 bytes. A conexão retornou conteúdo vazio no pedido base64 e erro UTF-8 no blob binário. Não houve acesso aos seus bytes nem redistribuição. Os exemplos reais do próprio OpenNord são git-ignored. Veja [README dos fixtures](../tests/fixtures/nord/README.md). Assim, os seis fixtures workshop são versionados e validados, mas a validação com **arquivo externo real/hardware** permanece pendente, conforme o fallback autorizado na solicitação.

## Compilar e executar

A libsndfile neste checkout é carregada dinamicamente pelo ABI existente, não presumida como link transitivo de sfizz. `WavWriter` resolve `sf_wchar_open`/`sf_open`, `sf_writef_float`, `sf_close`; `sampling::Decoder` valida os WAVs. O CMake liga `${CMAKE_DL_LIBS}` nos sistemas necessários e mantém a cópia de `sndfile.dll` no Windows. Não precisa de headers libsndfile nem altera o motor.

Linux, testes sem ASIO físico:

```sh
cmake -S . -B build -DLP_TEST_ONLY=ON -DCMAKE_BUILD_TYPE=Debug -DLP_SNDFILE_LIBRARY=/lib/x86_64-linux-gnu/libsndfile.so.1
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

Para incluir arquivo externo quando disponível:

```sh
cmake -S . -B build -DLP_NSMP_REAL_FIXTURE=/caminho/MeuTimbre.nsmp3
ctest --test-dir build -R nsmp_external_real_fixture --output-on-failure
```

Para testar conversão independente da interface:

```sh
build/lp_convert_timbre tests/fixtures/nord/workshop-v4.nsmp4 /tmp/lp-sfz-novo /lib/x86_64-linux-gnu/libsndfile.so.1
build/lp_convert_timbre /biblioteca/INSTRUMENTS/x.nki /tmp/lp-nki-novo /lib/x86_64-linux-gnu/libsndfile.so.1 /biblioteca/Samples
```

Windows, dependências locais do LP já preparadas:

```powershell
cmake -S . -B build-win -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DLP_PORTAUDIO_SOURCE=C:/deps/portaudio -DASIOSDK_ROOT_DIR=C:/deps/asiosdk -DLP_WEBVIEW2_SDK_ROOT=C:/deps/webview2 -DLP_ENABLE_VISION_UI=ON
cmake --build build-win --config Release --target LivePerformance LivePerformanceVision lp_convert_timbre
```

A opção de compatibilidade é para o PortAudio antigo no CMake 4. Mantenha as DLLs compatíveis do portátil, inclusive sndfile/sfizz/FluidSynth e dependências. Para executar os testes Windows, configure `LP_SNDFILE_LIBRARY` com o caminho de `sndfile.dll` e use `ctest -C Release` no próprio Windows; os executáveis MinGW não foram executados neste Linux.

## Importação e audição

1. Abra o LP compilado com suas DLLs existentes. Use o importador geral de timbres; o filtro aceita `.sfz`, `.sf2`, `.nsmp`, `.nsmp3`, `.nsmp4`, `.nki`.
2. Escolha um fixture workshop ou NSMP real. Após conversão, Meus Sons mantém o nome do original e toca o derivado SFZ.
3. Inspecione `.meta.json` para codec, checksum, zonas, loops, ganhos, afinação e limitações. Não confunda `signature_confirmed=true` CBIN com fidelidade de todos os parâmetros proprietários.
4. Para NKI, mantenha a árvore de WAVs ou selecione a pasta pedida. Um monolito sem WAVs dá diagnóstico; cancelar não cria um timbre falso.
5. Teste extremos das zonas, notas-raiz e velocities nos dois lados de cada limite. Compare loop contínuo, one-shot, estéreo, pitch e volumes com os WAVs e, quando possível, com o Nord/Kontakt original.
6. Salve/reabra favorito e Performance; renomeie/mova/oculte em Meus Sons. Faça importação SFZ e SF2 multipreset anteriores e teste sustain/pitch/MIDI/ASIO no Windows.
7. A importação é offline e síncrona na UI Win32: arquivos grandes podem ocupar a interface durante o preparo, embora nada seja acrescentado ao callback. Importe antes do show.

## Limitações conscientes

- Normalização Nord segue a exportação/audição OpenNord por pico por stroke, compartilhado entre canais. O significado integral de gain/exponent/peak proprietários do header ainda não foi recuperado; o volume entre strokes pode diferir do teclado. Todos os bytes e o pico utilizado ficam preservados.
- Rate vem do pitch-base do stroke; 35002 Hz é o fallback da referência, cuja interpretação física o próprio upstream não afirma ter comprovado em hardware.
- Alternate start e decay apenas dentro do loop ficam no sidecar; não são substituídos por envelope global que mudaria o ataque. Loops que não resolvem em marcas recebem diagnóstico/bytes, sem ponto inventado.
- SampleUnison, random-stroke, Tru-Vibrato, parâmetros de estilo/FX e flags sem equivalência conhecida ficam no sidecar como valores/hex com origem. Isso preserva dados para uma extensão futura, sem afirmar que já estão audivelmente ativos.
- NKI é reconstrução por WAVs externos, sem scripts, grupos, round robin ou regiões reais. Raiz desconhecida toca sem transposição em faixa inteira; múltiplas regiões podem somar.
- SFArk é identificado como família SoundFont, mas **não é descomprimido**. O leitor de presets SF2 recusa entrada comprimida; nenhum dado sfArk é encaminhado como PCM.
- A conversão Nord não recebe os limites 16-bit do SF2: WAV é float32. Existe arredondamento float32 após a normalização; o original e os golden integer PCM são preservados.
- Limites de implementação: 256 MiB por original Nord; 2048 seções; map até128 KiB; metadata por seção até1 MiB; 512 strokes; 4096 zonas de origem; 16 milhões de valores PCM por stroke e 64 milhões total. O localizador segue o upstream e exige ~100 frames; instrumentos muito curtos podem ser recusados.
- NKI: até256 WAVs/256 MiB, profundidade8/20mil entradas, mono/estéreo, 8–192 kHz, até120s e 32 milhões de valores por WAV/64 milhões total. Entrada fora dos limites falha visivelmente, sem truncamento.
- Afinação é decomposta em `transpose` e `tune`, com limite explícito da implementação ±127 semitons. Fonte fora disso é recusada em vez de produzir afinação silenciosamente limitada.
- Não foi executada audição em hardware Windows/ASIO, nem renderização do SFZ pelo sfizz real neste ambiente. Os testes de motor existentes usam seus doubles; a comparação PCM usa o decoder original e libsndfile real.

## Publicação

O texto abaixo registra a validação da preparação original. Em 4 de novembro de 2026, Hugo autorizou explicitamente a integração destes formatos ao repositório. Veja FORMAT-INTEGRATION-2026-11-04.md para a revalidação e consolidação do código. A cota autenticada permanece desconhecida; esta etapa não usa GitHub Actions.

## Resultado final desta execução

- CTest: **27 testes executados e aprovados; 1 explicitamente pulado**, sem falhas (27,73 s). O pulado é exclusivamente o arquivo externo real ausente.
- `timbre_import_validation`: seis instrumentos workshop, treze strokes, igualdade de cada inteiro PCM com OpenNord, WAVs lidos pela libsndfile real, mapas/velocities/loops e chave portátil de favorito. Passou.
- Compilação local MinGW concluída para `LivePerformance.exe`, `LivePerformanceVision.exe`, ambos os harnesses Win32 e `lp_convert_timbre.exe`. Executáveis de produção inspecionados como PE32+ x64; não executados no Windows.
- Sidecars de NSMP3/NSMP4/NKI analisados com parser JSON; hashes dos instrumentos e PCM versionados conferidos.
- Diferença nula nos arquivos do motor sfizz, motor SF2, DSP, AudioHost e no criador SF2 aprovado, contra a base pública indicada.
- Fonte, patch, fixtures, resultados e licença preparados para revisão; não é um ZIP de atualização instalado/testado em palco e não foi enviado ao Drive ou publicado no GitHub.
