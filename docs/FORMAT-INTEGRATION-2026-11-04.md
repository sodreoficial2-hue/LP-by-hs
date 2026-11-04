> **Última etapa NPNO/NPMO:** consulte “Atualização NPNO/NPMO: timbre, camadas e estéreo” ao final. Aceite amplo parcial; resultados e pendências explícitos.

> **Estado atual:** consulte a seção “Atualização: fechamento das lacunas P1–P6”. O registro inicial abaixo é histórico. O aceite amplo ainda depende de monólitos Kontakt não suportados.

# Consolidação dos importadores — 4 de novembro de 2026

Pedido autorizado: publicar no repositório `sodreoficial2-hue/LP-by-hs` as
integrações NKI, NSMP e variações, e NPNO. Base remota das funções anteriores:
`347e32176754c6d906e9b3456b41f39556c729b6` (criador SF2, VU e piano inicial).

O código foi recuperado do backup de fontes `88de6ef` incluído no pacote de
atualização NPNO/NPMO, comparado por hashes Git com a base remota e revalidado.
Não foi necessário reescrever conversores nem modificar motores, DSP, AudioHost,
criador SF2 ou interface Vision. As mudanças de interface nativa e catálogo
encaminham as novas entradas para o pipeline comum SFZ/WAV.

## Compatibilidade efetiva

- NKI: importação de WAVs externos, com mapa aproximado por nomes de nota e
  velocity; não é um parser binário integral do Kontakt. Scripts, grupos reais,
  monólitos, round robin e articulações do NKI não foram implementados.
- NSMP, NSMP3 e NSMP4: leitura binária CBIN/NWS/NSMP, decoder NW1, zonas,
  camadas de velocity, canais e loops. Recursos sem equivalência conhecida ficam
  no sidecar, com as limitações de ganho/taxa documentadas.
- NPNO e alias NPMO: CBIN/CNSP, PCM16, camadas, afinação, loops e bancos de
  ataque/pedal/soltura. Curvas de toque e envelopes têm aproximações explícitas.
- Originais, SHA-256, metadados e relatórios de auditoria são preservados.

## Revalidação nesta execução

Cinco grupos compilados diretamente com GCC/C++17, assertions ativas e
libsndfile real, todos aprovados:

1. `timbre_import_test`: seis fixtures Nord e treze strokes comparados com os
   vetores PCM independentes; SFZ/WAV, NKI externo, hashes, catálogo e rollback.
2. `npno_import_test`: demo sintetizada e layouts v5/v6, comparação de cada
   valor PCM, canais, loops, velocity 1–127, pedal/soltura, aliases e corrupção.
3. `instrument_catalog_test`: categorias, UTF-8, favoritos portáveis e rejeição
   de caminhos inválidos.
4. `catalog_edits_test`: persistência, ocultação, exclusão, arquivos compartilhados
   e recuperação de falhas.
5. `sample_builder_test`: decodificação, pitch, estéreo, dinâmica, loops, SF2 e
   falhas seguras do criador anterior.

O script `scripts/verify_timbre_import.sh` reproduz estes cinco grupos sem CMake.
Os vetores e o exemplo estão reunidos em um arquivo compacto versionado; o CMake
extrai somente no build. O uso de CMake não foi reexecutado nesta execução,
pois o programa não está instalado neste ambiente.

Não houve novo teste Windows/ASIO, audição comparativa, corpus pessoal Nord ou
NKI real com mapeamento binário. Os números 27/28 nos relatórios anteriores são
históricos e não representam testes repetidos hoje. Não foi gerado novo EXE.

A cota autenticada do GitHub permanece desconhecida porque não existe ferramenta
de consulta disponível. Publicação agrupada de fontes, documentação e vetores;
sem GitHub Actions, computação hospedada ou distribuição de executáveis.

---

## Atualização: fechamento das lacunas P1–P6 (4 de novembro de 2026)

**Estado atual: implementação e testes ampliados publicados; aceite integral ainda
não atingido para monólitos Kontakt de layout arbitrário.** A seção acima é o
registro histórico da primeira consolidação e permanece preservada. Os hashes
da linha de base estão em `format-validation/baseline-sha256.json`.

A nova etapa altera somente importadores, escritor/políticas SFZ, build de seus
testes, fixtures e documentação. Motores, DSP, AudioHost, criador SF2 e interface
Vision não foram reescritos. Cota autenticada do GitHub desconhecida; execução
local, sem Actions. Um teste verde de fixture sintético não comprova equivalência
com todas as bibliotecas comerciais ou com o DSP de um teclado.

### Estado por formato e recurso

| Formato | Parser efetivo | Áudio e taxa | Zonas/velocity/loops | RR | Limites restantes |
|---|---|---|---|---|---|
| NKI XML binário | Scan zlib 0..255 e gzip; K2_Container; todos os programas | WAV/AIFF externos; RIFF/FORM embutido inequívoco; taxa original | Campos XML reais; defaults/descarte; ganho linear; loops 0/1/2; partição de sobreposição | `seqLength`/`seqPosition` explícitos e aliases suportados | Outros contêineres monólitos e NCW não implementados; scripts/parâmetros sem equivalência preservados no XML |
| NKI fallback | Apenas se não houver payload reconhecido | WAVs companheiros; comportamento anterior mantido | Aproximação por nomes, com aviso explícito | Sem inferência de RR | Não é leitura do mapa original; WAVs desconhecidos podem sobrepor; não usar como prova de fidelidade |
| NSMP codec1/2 | CBIN/NWS; map9/10 e NW1 separados | Mono/estéreo float32; taxa do stroke | Zonas e loops do layout; auditoria de camadas | Campo de RR não confirmado: não inventar | Ganho físico/normalização proprietária, ADSR/filter/style sem tradução comprovada permanecem no sidecar |
| NSMP3 | CBIN/NSMP; map12/14 e NW1 | Mono/estéreo, taxa original interpretada | Idem; rejeita layout desconhecido | Mesma política | Defaults ADSR/filter existentes na origem permanecem raw quando sem campo decodificado confiável |
| NSMP4 | CBIN/NSMP; map21, packing por canal | Mono/estéreo, taxa original interpretada | Multi-stroke tratado como camadas quando o mapa declara velocity | Mesma política | Não chamar todo stroke de round robin |
| NPNO/NPMO | Assinaturas CBIN/tag/CNSP; internos 0x450/0x464 | PCM16; referência de replay 35002 Hz; estéreo L/R preservado | Famílias 5/6, bancos de pedal/soltura; auditoria 1..127 | Identificadores de take não provam RR | Touch/envelopes/gain/DSP não emulados integralmente; nenhum outro bit depth é afirmado |

### Relatório único por subitem

PASSOU significa que o comportamento descrito foi exercitado pelo teste indicado.
N/A identifica ausência de recurso/corpus ou execução fora deste ambiente;
**FALHADO permanece visível e impede afirmar o aceite integral**.

| Item | Estado | Evidência / defeito corrigido ou limite |
|---|---|---|
| P1.1 | PASSOU | `nki_reader.cpp`, `nki_binary_test`: offsets 0/17/170/255, candidato falso no header e gzip; antes só havia heurística por extensão/nome |
| P1.2 | PASSOU no layout XML | 2 programas do fixture `multi.nki`, saídas nomeadas por programa; default carrega o primeiro. Defaults e zonas inválidas/sem sampleFile exercitados; XML inteiro preservado |
| P1.3 | PASSOU | `zoneVolume=0.5` → -6.020599913 dB; ganho <=0/NaN omitido com aviso e piso registrado -144; não era convertido de campo XML |
| P1.4 | PASSOU com decisão SFZ | 0→`no_loop`, 1→`loop_sustain`, 2→`loop_continuous`; pontos inteiros exatos. Os aliases solicitados `off`/`note` não são valores SFZ padronizados |
| P1.5 | PASSOU pela alternativa permitida | Aviso + partição de roots vizinhos em 70/71; testes estruturais e Sfizz real nas teclas 68..71 confirmam 1 voz por nota. Fades originais ficam no XML |
| P1.6 | **FALHADO no escopo amplo; PASSOU na variante suportada** | `monolith.nki` extrai RIFF/FORM simples após o stream, com uma referência inequívoca; WAV/AIFF externos relativos funcionam e falta de sample nomeia o arquivo. Não implementados FileContainer/NKS/NIS/NCW nem tabelas embutidas arbitrárias; o fixture simples não prova essas variantes |
| P1.7 | PASSOU | WAV `São Paulo A4.wav`, AIFF estéreo com acentos, e carregamento real de `Samples/áudio com espaço.wav`; aliases de áudio emitidos são relativos e ASCII |
| P1.8 | PASSOU nos campos conhecidos; N/A nos semânticos não documentados | pan -20/0; zoneTune ratio 1 e offset 0.5 cent; seqLength/seqPosition explícitos. Não se afirma que esses nomes cobrem RR de todo NKI. Campos desconhecidos preservados integralmente no XML |
| P1.9 | PASSOU | `tests/fixtures/nki/generate.py`: 18 arquivos NKI determinísticos, incluindo multi-programa, fallback, RIFF embutido, falta de sample, gzip, corrupção, expansão e DTD; WAV/AIFF sintetizados. São vetores da especificação, não arquivos exportados pelo Kontakt |
| P2.1 | PASSOU na política; N/A onde não há campo confirmado | `sfz_policy.cpp`: somente sequência explícita, sem combinar `seq_` e `_rand`; sem expansão por nota, sem adivinhar RR de strokes de velocity/pedal. Alternativas ambíguas sem sequência são recusadas; consultar decisão abaixo |
| P2.2 | PASSOU | Sfizz: fixture RR tem 4 regiões; uma nota→1 voz, duas notas→2 vozes. Duplicação exata/posição faltante falha; cada slot seleciona uma camada por nota |
| P3.1 | PASSOU | `ConversionOptions.velocityCrossfade=false` por padrão nos 3 fluxos; CLI `--velocity-crossfade=1..16`; auditoria ocorre antes do crossfade e a opção registra custo de vozes. Gates de exportação verificam que NSMP/NPNO realmente passam pela política comum |
| P3.2 | PASSOU | Piano sintético de 88 teclas × 6 camadas × 2 RR = 1056 regiões, acordes de 8 notas com CC64 e limite 128. Limite atingido no acorde 16 (1.6 s simulados) OFF, 8 (0.8 s) ON; métricas reais em JSON |
| P4.1 | PASSOU para ausência de resample; N/A para ganho físico de hardware desconhecido | Nenhum resampler nos importadores/WavWriter. NKI 44100/48000 Hz; amostras binárias 35002 Hz nesta referência. Cents de resample=0. A normalização por pico NSMP anterior é mantida e declarada; equivalência de loudness com o teclado NÃO foi comprovada |
| P4.2 | PASSOU | Sete vetores independentes de seno 440 Hz: NKI, NSMP OG/2/3/4 e NPNO 5/6; erro absoluto máximo observado <0.21 cent, tolerância de teste <3 cents; roots reais armazenados |
| P5.1 | PASSOU no escopo PCM16; N/A para outro bit depth inexistente no corpus | Dois layouts internos testados; 500/520/530/540 e 600/610/620/630 exercitados por mutação sintética de revisão, não corpus factory. Versão desconhecida nomeada; cabeçalho residual inválido nomeia versão/PCM16/largura/ordem; não há campo de bit depth independente inventado |
| P5.2 | PASSOU | Checksum divergente gera aviso e segue, mantendo bounds/validação de áudio; testes de importação e de política |
| P5.3 | PASSOU | Auditoria comum por root/tecla/trigger/CC64/lane/posição RR para mapas com camadas; gap e overlap falham em NKI e política comum, além da auditoria NPNO anterior. Velocities 0 de entrada são convertidas para 1 como limite tocável, sem criar uma nota-on MIDI zero |
| P6.1 | PASSOU CMake; N/A EXE novo | CMake 4.4.4 instalado e usado nesta execução; configure/build GCC Debug locais. Sem toolchain Windows/SDK ASIO preparado, não foi gerado novo EXE nem usado workflow hospedado |
| P6.2 | PASSOU | `verify_timbre_import.sh`: 5 grupos anteriores + NKI binário + política + render real opcional; Python stdlib restaura fixtures, sem CMake/rede na reprodução dos testes |
| P6.3 | PASSOU render dos fixtures; N/A comparação auditiva com originais | Cada um dos 3 formatos: 128 teclas × 3 velocities (1/64/127) × 2 estados CC64 = 768 notas não silenciosas e energia crescente em 1/64/127; pedal mantém/solta loop; previews e instrumentos completos gerados. Falta arquivo pessoal + gravação de referência Nord/Kontakt + perfil de toque para equivalência ao original |
| P6.4 | PASSOU | Novos NKI: zlib corrompido/truncado, XML incompleto, DTD, limite de expansão 16 MiB, sample faltante, extensão trocada e MIDI inválido; mensagem específica, rollback e nenhum crash/dump |

### Medições de áudio e polifonia

Resultados brutos: [`format-validation/metrics.json`](format-validation/metrics.json).
Sfizz 1.2.3 real, fonte `4e70dc0bef53b41f2853ed46e26f5911114c92d0`, compilado
localmente; saída 48000 Hz / blocos 256 / orçamento 128 vozes. Não é teste ASIO.

| Medição | Resultado |
|---|---|
| NKI 440 Hz, fonte 44100 Hz | -0.202652 cent |
| NSMP OG, fonte 35002 Hz | -0.159306 cent |
| NPNO v5, referência 35002 Hz | -0.159306 cent |
| NSMP 2/3/4 e NPNO v6 | aproximadamente +0.0011 cent cada |
| RR: uma / duas notas | 1 / 2 vozes vivas; sem disparo duplicado por alternativa |
| Crossfade OFF / ON | 128 vozes no acorde 16 / 8, respectivamente |

O limite é atingido por acumular acordes com pedal continuamente pressionado;
esses números não significam que um acorde sozinho esgote 128 vozes. O tempo
em segundos é o tempo musical simulado até o orçamento de vozes ser atingido,
não a latência do equipamento. O tempo de CPU em milissegundos está no JSON e
varia por máquina/carga. A comparação com os primeiros blocos de uma fixture
com ruído e loop curto mostrou que zero-crossings não seria uma referência de
afinação adequada; o gate final usa senos puros produzidos independentemente.

### Decisões

1. **Loops válidos em SFZ.** O formato usa `no_loop`, `loop_sustain`,
   `loop_continuous` e `one_shot`, conforme
   <https://sfzformat.com/opcodes/loop_mode/>. `off`/`note` seriam aliases
   não padronizados. NKI modo 2 foi escolhido como loop contínuo inclusive na
   fase de release, sem ignorar note-off com um one_shot. Os ends XML são
   interpretados como índices inclusivos; internamente +1, no escritor -1.
   Essa convenção é exercitada nos fixtures; outros produtores exigem corpus.
2. **RR sem aleatoriedade combinada.** Preferimos apenas `seq_length` e
   `seq_position`, explicitamente declarados pela origem. Não cruzamos `seq_`
   com `lorand/hirand`: a divergência de comportamento entre players descrita em
   <https://sfzlab.github.io/sfztest/sfizz-part-1-seq_-and-_rand/> torna essa
   mistura uma decisão de portabilidade ruim. Strokes de velocity e bancos
   de pedal/release não são alternativas RR. Regiões iguais sem sequência
   explícita são recusadas, em vez de dispararem juntas ou ganharem RR inventado.
3. **Crossfade OFF é o padrão.** ON é opção offline; mantém contagem de regiões,
   expande apenas a janela de velocity com `xfin_*`/`xfout_*` e usa largura
   limitada à metade das camadas vizinhas. O gate de cobertura disjunta roda
   antes dessa expansão intencional. Polifonia finita não pode ser garantida
   para pedal infinito; o teste mede a diferença de custo.
4. **Sobreposição de teclas.** A opção permitida de aviso é usada, com partição
   dos roots vizinhos; não implementamos fades de tecla proprietários sem
   semântica/curva confirmada. Fades e ranges originais permanecem no XML.
5. **Paths.** Caminhos externos são relativos ao NKI. Referências geradas usam
   aliases ASCII `Samples/sample-N.wav`, barras `/`, sem drive ou `..`.
   Espaços em `sample=` são literais no parser SFZ; percent-encoding ou
   backslash de shell alterariam o nome. O gate real confirma UTF-8/espaços.
6. **Unidades ambíguas.** zoneTune usa ratio (1=unidade) e zonePan -100..100;
   o XML completo é guardado para não perder outra interpretação. Ganhos
   positivos convertem com 20 log10 e piso -144 dB; zero/negativo/NaN omitem a
   região com aviso. Não se inventam campos do formato binário para completar
   defaults ADSR/filter ainda desconhecidos.
7. **Monólitos.** RIFF/FORM simples após XML só é associado quando existe uma
   referência e uma amostra inequívocas. Isso não é um parser de NCW,
   FileContainer, NKS ou NIS. A documentação XML de referência
   <https://github.com/JefroB/Multisample-format-docs/blob/main/nki.md>
   não define essas tabelas. Permanecem explicitamente fora do suporte.
8. **Taxa e ganho físico.** Não há resampling offline. NPNO adota a interpretação
   de replay 35002 Hz da referência; não contém um campo de sample rate que
   pudesse simplesmente ser copiado. NSMP mantém a normalização por pico
   anterior e os valores de origem no sidecar: o teste de seno confirma
   afinação e transporte numérico, não a loudness do DSP original.

### Reprodução e evidência

Dependências: GCC C++17, headers/bibliotecas zlib e Expat, libsndfile em runtime,
Python 3 e tar. O caminho com CMake também requer CMake >=3.16; Sfizz é opcional
na suíte rápida e obrigatório para reproduzir as medições de áudio.

```sh
# Sem CMake: conserva saídas completas para inspeção.
LP_IMPORT_TEST_OUTPUT=/caminho/absoluto/validation \
LP_REAL_SFIZZ_LIBRARY=/caminho/absoluto/libsfizz.so \
bash scripts/verify_timbre_import.sh /caminho/absoluto/libsndfile.so.1

# Build local independente, assertions ativas.
cmake -S . -B build-format -DLP_TEST_ONLY=ON -DCMAKE_BUILD_TYPE=Debug \
 -DLP_SNDFILE_LIBRARY=/caminho/libsndfile.so.1 \
 -DLP_REAL_SFIZZ_LIBRARY=/caminho/libsfizz.so
cmake --build build-format --parallel 4
ctest --test-dir build-format --output-on-failure \
 -E 'real_sfizz_envelope|real_performance_controls'
```

Os dois testes legados excluídos nesse comando exigem o banco sonoro recuperado,
que não faz parte deste backup compacto; não são substituídos por doubles nesta
validação. Os gates de arquivos externos pessoais podem indicar `Skipped`
quando `LP_NPNO_REAL_FIXTURE`/`LP_NSMP_REAL_FIXTURE` não são fornecidos.
A suíte dos importadores preserva todos os cinco grupos anteriores; há gates
novos separados para parser, política, famílias/afinação e render real.

Exemplos efetivamente gerados e instruções para obter SFZ+WAV completos:
[`format-validation/examples/`](format-validation/examples/README.md).
Os geradores e os vetores comprimidos independentes estão no repositório;
a execução normal dos testes não baixa fontes nem precisa de Node.

### Pendências que impedem o aceite amplo

- P1.6: implementar e validar o(s) layout(s) de monólitos comerciais exigidos,
  com amostras embutidas e tabelas reais, incluindo NCW quando aplicável.
- Obter arquivos pessoais/exports independentes de versões relevantes e gravações
  originais para comprovar unidades, curvas de toque, ganho físico, envelopes,
  filtros, pedal e semântica de RR não documentada.
- Compilar/executar a nova interface em Windows com as dependências zlib/Expat e
  runtime ASIO corretos. O sucesso do conversor Linux não equivale a essa etapa.

O aceite sintético (parser K2 real, suíte verde, afinação/dinâmicas/pedal/vozes)
é demonstrado. **Não se declara cumprido o critério “nenhum P1–P5 FALHADO”
porque P1.6, no sentido amplo de qualquer monólito, permanece FALHADO.**

### Resultado final dos comandos desta etapa

- **Sem CMake:** os 5 grupos antigos, os 2 grupos novos (NKI/política) e o gate
  de render real passaram. A asserção adicional de posição RR ausente também
  passou após recompilar somente esse teste.
- **CMake/CTest:** configure e build locais aprovados; 38 testes registrados,
  34 executados e aprovados, 2 de arquivos pessoais marcados `Skipped` e 2
  legados de banco recuperado explicitamente excluídos pelo comando documentado.
  Nenhuma falha funcional nos gates aprovados. Houve reexecução por permissões de binários gerados que o workspace revertia; o script agora restaura o bit de execução imediatamente antes de cada gate. Os subdiretórios privados de cada render também são limpos para permitir reexecução independente.
- Todos os sidecars gerados foram analisados como JSON; `git diff --check`
  aprovado. Hashes dos arquivos protegidos/sem mudanças conferidos.
- Não foi produzido novo EXE. O repositório contém exemplos SFZ reais e todos
  os geradores/vetores necessários para reproduzir as saídas offline.

A última execução CTest concluída retornou **0**, com os 34 testes disponíveis aprovados e os 2 fixtures pessoais ausentes explicitamente pulados; as 2 exclusões legadas permanecem as documentadas acima.

---

## Atualização NPNO/NPMO: timbre, camadas e estéreo (4 de novembro de 2026)

**Estado: melhorias implementadas e medidas; aceite amplo ainda parcial.**
Os registros anteriores permanecem históricos. P1.6 (monólitos Kontakt) continua
**FALHADO**. Não houve comparação auditiva com teclado Nord nem novo EXE Windows.

[Reprodução, referência de calibração, decisões e fontes](format-validation/npno-timbre/README.md).
[Métricas desta execução](format-validation/npno-timbre/metrics.json).
[Arquivos alterados](format-validation/npno-timbre/files-changed.txt).
[Resultados completos](format-validation/npno-timbre/test-results.txt).
[Confronto dos 201 limiares](format-validation/npno-timbre/threshold-comparison.json).

| Aspecto NPNO atual | Estado e evidência |
|---|---|
| Parser efetivo | PASSOU estrutural: CBIN/CNSP 0x450/0x464, diretório 0x732 × 0x76; `npno_import_test`, referências confrontadas. Trim v5 agora u16; interpretação v6 permanece divergente |
| Áudio/taxa | PASSOU: PCM16 35002 Hz sem normalização/resampling, todos os valores L/R conferidos contra PCM independente |
| Zonas/velocity/loops | PASSOU cobertura exata 1..127 antes do crossfade; limites de loop conservados. FALHADO para paridade da curva Touch com hardware: referências conflitantes |
| RR | N/A: não há campo NPNO confirmado, takes não viram RR inventado; política comum de sequência preservada |
| Crossfade | PASSOU: padrão ligado NPNO, largura 8 (1..16 configurável), duas regiões apenas na janela; NKI/NSMP opt-in |
| Calibração de ganho | PASSOU na referência de projeto PCM+trim/isotônica com sfizz real; N/A sem biblioteca explícita; FALHADO para curva contínua/DSP de origem não recuperados |
| Estéreo | PASSOU: um WAV intercalado por stroke, uma região por stroke/zona, uma voz por camada; fallback mono com compensação da lei de pan |
| Release | PASSOU: constantes conservadoras 0,2/2,0/0,2 s registradas; banco 2 só note-off e sem retrigger por bouncing. N/A conversão de fades desconhecidos |
| Limites restantes | DSP simpatético, Touch por modelo/perfil, trim v6, corpus pessoal e equivalência auditiva não comprovados; volume [-144,+10.8] dB, 65536 regiões, original/sidecar preservados |

### Relatório aditivo por subitem

| Item | Estado | Evidência, decisão ou pendência |
|---|---|---|
| T1.1 | PASSOU | CLI `--velocity-crossfade`, `--crossfade-width=N` padrão 8; três formatos; alias antigo mantido; `timbre_conversion_cli` |
| T1.2 | PASSOU | NPNO liga por padrão, `--no-velocity-crossfade` desliga; sidecar `velocity_crossfade=on; width=N`; auditoria anterior permanece |
| T1.3 | PASSOU nos fixtures | `shared_format_real_render`: 128 teclas × 127 velocities × 2 pedais × 2 versões = 65024 combinações; janela e opcodes conferidos; uma voz fora, duas na janela inclusive endpoints de ganho zero |
| T2.1 | PASSOU | Grupos por programa/banco/trigger/CC64, ordem de grupos por primeira ocorrência e ordem de regiões mantida dentro deles; controles repetidos evitam herança acidental |
| T2.2 | PASSOU na referência de projeto | Constantes iniciais -12/-18 dB e calibração RMS contra ataque normal por tecla/camada. `shared_format_real_render` verifica diferenças dentro de 0,5 dB |
| T2.3 | PASSOU pela alternativa conservadora | `damperTop` confirma amortecimento, não ausência de banco 2: regiões superiores mantidas; dúvida explícita no sidecar |
| T2: ressonância simpatética real | FALHADO no escopo amplo | Banco 1 continua substituto exclusivo do ataque com pedal, como documenta nord-format; grupos/ganho não implementam DSP de notas já soando |
| T3.1 | PASSOU na aproximação por camada; FALHADO na curva contínua de origem | RMS real, 3 velocities únicas por faixa, referência PCM+trim isotônica. NPNO subdivide por tecla para compensar duração de release transposto; cada região resultante tem uma tecla possível. Curva contínua Nord desconhecida, não inventada |
| T3.2 | PASSOU para compensação explícita | `amp_veltrack=0`, offset e erro residual por região, rerender pós-ajuste; erro máximo sintético 0,00000252628 dB. Calibração é explícita `--calibrate-sfizz=LIB`; ausência registrada N/A |
| T3.3 | PASSOU | PCM16 de cada valor e canal idêntico ao oracle; compensação apenas `volumeDb`; bounds verificados |
| T4.1 | PASSOU parcial; N/A documentos indisponíveis | nord-format 0.8.0 e código OpenNord consultados; `NSP-FORMAT.md` não localizado nas revisões consultadas, análise Jeroen indisponível. URLs/revisões no README de reprodução |
| T4.2 | PASSOU para offsets; FALHADO para semântica integral | Diretório, mapa, fine tune, bancos, loop e decay confrontados; regras de velocity e trim divergem entre fontes |
| T4.3 | PASSOU na correção trim v5; FALHADO para conflito Touch/v6 | v5 +0x34 agora unsigned. Não se escolhe nova curva nem unidade v6 sem resolver a divergência. Comparação legada/log OpenNord mostra até 20 passos MIDI; não é erro medido contra Nord |
| T4.4 | PASSOU | Nome, 21 pares threshold/MIDI, interpolação, regra v5/v6, estimativa legada e conflito registrados em cada `source_metadata.velocity_curve` |
| T5.1 | PASSOU | PCM16 estéreo nativo exato; fase L/R preservada e uma voz estéreo real. `npno_import_validation` e `shared_format_real_render` |
| T5.2 | PASSOU | `--stereo-mono-fallback`; par mono hard-pan e -3,01029995664 dB por lane para lei de pan; caminho registrado no sidecar; erro de comparação permitido <0,02 dB |
| T5.3 | PASSOU | Regiões estéreo únicas aceitas pela auditoria; pares antigos -L/-R continuam aceitos, duplicatas ambíguas continuam falhando |
| T6.1 | PASSOU pela alternativa conservadora | Sem unidade confirmada dos fades, constantes 0,2/2,0/0,2 s preservadas e decisão gravada; coeficientes de loop não convertidos arbitrariamente em ampeg_release |
| T6.2 | PASSOU nos fixtures | Banco 2 isolado tem zero voz no note-on, uma no note-off, uma após alternar 127/0/127/0 em CC64; nenhum novo ataque criado pelo pedal |
| T7.1 | PASSOU no escopo medido | Varredura completa v5/v6 com contagem no ataque e note-off, janelas de crossfade, RMS dos bancos, PCM/fase e polifonia estéreo. Não constitui teste perceptivo contra teclado |
| T7.2 | N/A | Dois testes pessoais v5/v6 com variáveis CMake independentes e skip 77; nenhum módulo pessoal fornecido |
| T7.3 | FALHADO na suíte completa; PASSOU nos gates desta tarefa | Resultado exato e falha legada OSC/ADSR abaixo. A falha não foi apagada/excluída da execução completa |
| T7.4 | PASSOU | P1.6 continua FALHADO; nenhuma equivalência auditiva ou Windows/ASIO afirmada |
| T8.1 | PASSOU | Tabela acima e relatório aditivo preservam estado e evidências anteriores |
| T8.2 | PASSOU | Limitations atualizadas: tracking adicional removido, estéreo novo e dúvidas reais explicitadas |
| T8.3 | PASSOU no registro | Arquivos, métricas, fontes, reprodução e resultados brutos anexados nesta seção/diretório de evidências |

### Medições e custo

- Erro RMS máximo sintético da referência de calibração: **0,00000252628 dB**, tolerância 0,5 dB, sem alteração dos WAVs. Os offsets calculados podem ser maiores que 10,8 dB; o **ganho final**, não o offset, é limitado a [-144,+10.8] dB.
- Fora da janela: uma voz de ataque por nota; dentro: duas. Note-off acrescenta somente a(s) camada(s) de release documentada(s), separadas da cauda do ataque. Bouncing do pedal não acrescenta instâncias de release.
- Estéreo: uma voz por camada contra duas no fallback mono. No piano lógico de 88 teclas × 6 camadas × 2 RR: **1056 regiões estéreo contra 2112 mono**, 8 contra 16 vozes no primeiro acorde; 128 vozes no acorde 16 contra 8 com sustain. O antigo número 1056 já era o número de regiões lógicas mono do teste, não 2112 canais de um arquivo Nord real.
- Calibração por tecla dos fixtures v5/v6 gera 1152 regiões por módulo, reutilizando 18 WAVs. Conversão sem calibração gera 36 regiões nativas nesses fixtures (72 no antigo estéreo em pares mono). Subdivisão não aumenta as vozes por nota.
- Crossfade no teste lógico mantém 1056 regiões e antecipa o teto de 128 vozes do acorde 16 para o 8. Não é latência do áudio e não elimina o custo de pedal continuamente pressionado.
- Divergência Touch: até **20 passos MIDI** entre a tabela legada e a aproximação logarítmica OpenNord no sweep 0..200; não há referência de hardware para declarar erro ≤2.

### Resultado final e escopo preservado

CMake configurou e compilou localmente. CTest completo: **36 PASSOU, 1 FALHADO,
4 N/A/Skipped**, 41 testes registrados (exit 8). A falha é o teste legado
OSC/ADSR descrito abaixo; todos os testes de importação, render e CLI desta
etapa passaram. Os quatro skips são fixtures pessoais ausentes. O script sem
CMake aprovou os cinco grupos originais, NKI binário, política comum, render real
e CLI (exit 0); detalhes em `format-validation/npno-timbre/test-results.txt`.
Há 14 hashes de arquivos protegidos em `protected-sha256.json`: motores, DSP,
AudioHost, criador SF2 e Vision idênticos à base. O original, SHA-256, prefixo,
registros completos e XML NKI continuam preservados. PCM não é normalizado por
esta etapa. Auditorias permanecem com exceção explícita e rollback.

A falha legada é **`real_sfizz_envelope`: `Real OSC ADSR readback`**, antes de seu
trecho de banco recuperado. O teste e o motor são idênticos à base; não se atribui
essa falha à ausência de banco nem se modifica o DSP fora do escopo. O teste
`real_performance_controls` passou nesta execução, apesar de ter sido excluído
em relatório histórico anterior.

Pendências visíveis: conflito de Touch/trim v6, curva contínua do DSP, ressonância
simpatética, fixtures pessoais, comparação auditiva, teste legado OSC/ADSR e
P1.6 amplo. Sem novo EXE Windows, sem Actions; quota autenticada desconhecida.
