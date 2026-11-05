> **Toques suaves:** [correção da dinâmica dentro da camada pianissimo](NPNO-SOFT-VELOCITY-2026-11-05.md). A curva local nova substitui o tracking plano na saída final; medições abaixo descrevem a etapa de sustain anterior.

# NPNO/NPMO: sustain — 5 de novembro de 2026

Estado aditivo sobre `38c24d3b7736a9dcfff41523693b9473bc3c9e7f`, que já
contém a integração baseada em e88eac8 e a memória permanente de parâmetros.
Resultado da validação completa: **38 PASSOU, 1 FALHADO legado OSC/ADSR e
5 SKIP**, 44 testes CTest, exit 8; **10 grupos PASSOU** no script sem CMake,
exit 0, incluindo os cinco grupos originais. Os skips são NPNO pessoal geral,
NSMP real, NPNO v5, v6 e L/XL pessoais. Os gates opcionais FluidSynth não foram
registrados porque esse backend não está configurado; isso não conta como PASSOU.
A falha OSC/ADSR tem fonte e motor idênticos à base e não foi ocultada.

Esta etapa modifica somente importadores, política/escritor offline, CLI,
fixtures de teste e documentação. Não estabelece equivalência auditiva com Nord.

| Item | Estado | Evidência e limite |
|---|---|---|
| T1: banco 0 canônico, heranças rastreáveis | PASSOU | `shared_format_policy`: mutação v6 com 3 contra 2 camadas; matriz com 8 contra 4; auditoria de classes 1..127 por tecla/root; classe alterada ou herança sem registro gera exceção |
| T2: RMS pedal ON/OFF sem ressonância extra | PASSOU sintético | 24 regiões, 3 posições por zona e velocity central; offsets próximos de +6,0206 dB; erro máximo <0,00002 dB; `synthetic-calibration.json` |
| T3: grupo de ressonância de ataque | PASSOU sintético | 44 pares de render: OFF 1 voz, ON 2; incremento +1,797..+1,799 dB; ressonância termina antes de 8 s; não é simulação física de cordas |
| T4: aproximação de loopDecay | PASSOU como aproximação | q23 cru preservado, fórmula abaixo; sustain do ataque 25%; cauda medida abaixo do pico; envelope principal de release permanece 0,2/2,0 s |
| T4: unidade temporal original e paridade de envelope Nord | FALHADO | Nenhuma captação A/B nem evidência independente confirma que o multiplicador atue uma vez por frame; fórmula é política de projeto, não campo de tempo decodificado |
| T5: headroom de dez notas | PASSOU na grade sintética | Pior pico medido na grade −3,20 dBFS; acorde adicional −4,14 dBFS; todos os ganhos nos campos SFZ; PCM16 intacto |
| T5: alvo em fontes muito baixas sob teto +10,8 dB | FALHADO quando limitado | Golden v6 sem calibração isotônica: headroom residual abaixo do alvo; nível e limite registrados. Ganho extra não viola o teto para forçar aprovação |
| T5: teto para qualquer acorde/caudas ilimitadas | N/A | Grade finita de acordes; não há prova sobre todas as combinações, nem limitação de pico em tempo real |
| T6: matriz real libsfizz | PASSOU sintético | 11 teclas × 4 velocities × 2 estados; teclas 67/72/73 atravessam limite de amortecimento sintético 72; envelope, acorde, ressonância finita e soltura/bouncing |
| T6: conjunto pessoal real L/XL | N/A | `LP_NPNO_LXL_REAL_FIXTURE` ausente; teste registrado com retorno 77 |
| Vozes com crossfade | PASSOU com custo explícito | Matriz estrita executa sem crossfade. Sweep v5/v6 mantém crossfade padrão: até 2 vozes OFF / 4 ON na janela, 1 OFF / 2 ON fora dela; uma voz por componente/camada |
| Estéreo, original, hashes, PCM e sidecar | PASSOU | Testes anteriores preservados; original completo, SHA-256, prefixo e registros completos; uma região estéreo por componente, fallback mono disponível |
| NKI/NSMP | PASSOU regressão | Caminho comum mantém auditoria de cobertura/RR/keys; auditoria entre bancos somente para regiões com `canonicalBand>=0` produzidas por NPNO |
| P1.6: monólitos Kontakt | FALHADO | Permanece sem suporte, conforme relatório anterior |
| Windows/ASIO e audição no teclado original | N/A | Validação desta etapa é local Linux/libsfizz 1.2.3, sem hardware Nord |

## Partição e ganhos

O parser primeiro obtém a partição legada do banco 0, ordenada por velocity
crescente. Na banda de índice zero-based k, o banco 1 usa
`min(N1-1, round(k*N1/N0))`. Vizinhas com o mesmo registro são fundidas na
representação física do parser. O emissor volta a separá-las nas bandas
canônicas quando necessário para permitir offsets diferentes por banda, sem
copiar PCM nem criar sobreposição antes do crossfade. Cada banda registra root,
limites, registro normal, registro pedal, classe física e `inherited` no
`source_metadata.canonical_velocity_bands`. Heranças também geram avisos.

No fixture sintético 8/4: classes `[0,1,1,2,2,3,3,3]`; bandas herdadas 2, 4,
6 e 7 em cada root 36/60/84 (índices zero-based). A mutação v6 3/2 verifica o
mesmo algoritmo sobre registros CNSP efetivamente parseados. Todos os registros
inalcançáveis continuam extraídos e preservados.

Para cada par normal/pedal são renderizadas as posições grave, central e aguda
da zona, com a velocity central da banda. A média das três diferenças em dB é
somada a `volumeDb` do pedal, limitada a [-144,+10,8]. O erro é medido novamente
após o ajuste. `pedal_offset_db` e `pedal_residual_db` identificam cada região;
o maior residual consta em `preserved_features`. Em zonas de uma só tecla as
três posições coincidem, sem inventar teclas fora da região. Déficit por teto
é registrado como limitação. `amp_veltrack=0` continua explícito.

A camada extra copia o ataque pedal calibrado, usa −12 dB relativos, decay no
máximo 2 s, sustain zero e release 4 s. São constantes de projeto; não houve
medição A/B de teclado para substituí-las. Em amostra coerente duplicada,
−12 dB correspondem aproximadamente a +1,95 dB instantâneos; o RMS integrado
medido com o envelope fica próximo de +1,80 dB. Só note-on com CC64≥64 cria
essa voz. Não há `on_locc64`, disparo em note-off ou ativação em notas antigas.
O banco 2 continua `trigger=release_key`, release 0,2 s e trim inicial −18 dB.

## Aproximação de loopDecay

Para inteiro q com `0<q<8388608`, a referência de projeto é:

`decay_seconds = clamp(log(0.001) / (35002 * log(q / 8388608)), 0.15, 4.0)`.

Isso interpreta provisoriamente o Q23 como multiplicador por frame e estima o
tempo até −60 dB. O parâmetro do sfizz é um estágio exponencial aproximado, e o
sustain do ataque é 25%; portanto isso não afirma que o áudio alcance −60 dB
no fim do decay. O teste verifica queda e patamar, não igualdade com Nord.
Zero usa 0,15 s; unidade Q23 exata usa 4 s (limite contínuo saturado da
fórmula quando q tende à unidade). Valor acima da unidade usa envelope conservador sem
novo decay, com aviso incluindo registro e valor cru. A voz extra sempre tem
envelope finito próprio, inclusive quando o coeficiente original não se aplica.

| loopDecay cru | Decay emitido, segundos |
|---:|---:|
| 0 | 0,150000 |
| 8380000 | 0,192225 |
| 8388000 | 2,722793 |
| 8388500 | 4,000000 |
| 8388600 | 4,000000 |
| 8388608 (unidade Q23) | 4,000000 (limite de projeto) |

Não se atribuiu significado novo à tabela de fades, nem aos outros bytes.
Ausência de confirmação das unidades é FALHADO explícito na tabela acima;
valores incompatíveis conservam o comportamento anterior. A unidade Q23
exata não é classificada como layout incompatível: recebe explicitamente o
limite de projeto 4 s, sem alegação de que isso seja o tempo original do Nord.

## Headroom e execução offline

Primeiro estima-se dez ataques coerentes a partir do pico PCM, ganhos e camada
extra; crossfade acrescenta margem de amplitude sqrt(2). Depois medem-se até
cinco acordes (grave, centro, agudo, distribuído e acorde do teste) nas
velocities 10/40/80/127 e nas fronteiras canônicas. Janela de pico: 48128 frames
por acorde a 48 kHz. O refinamento só ocorre se a grade estiver mais de 2 dB
abaixo do alvo ou acima do teto. O ganho comum é limitado também pelo teto
final +10,8 dB de cada região e a grade é renderizada novamente. Toda redução
ou recuperação de ganho é registrada; limite que impeça o alvo gera FALHADO.
No golden v6 muito baixo sem calibração isotônica, a reprodução nativa ficou em
−18,89 dBFS e o fallback em −15,87 dBFS: o teto +10,8 dB das regiões impede
atingir o alvo. Isso é FALHADO visível, e não clipping nem medição de piano real.
Layouts podem receber ganhos de módulo diferentes por causa desse teto; o
teste estéreo remove somente o ganho de módulo registrado antes de comparar
a lei de pan e mantém a correlação de fase independente.

Essa grade mede acordes novos de dez notas; não limita acumulação ilimitada
com pedal pressionado e não substitui controle de polifonia do instrumento.

A resolução da biblioteca é: `--sustain-sfizz=LIB`, depois
`--calibrate-sfizz=LIB`, variável `LP_REAL_SFIZZ_LIBRARY`, ou `sfizz.dll` /
`libsfizz.so` ao lado de libsndfile. A calibração pedal é automática quando o
backend está presente, independentemente da calibração isotônica opcional.
Biblioteca explicitamente informada que não existe gera exceção e rollback.
Sem backend detectável, o fallback seguro mantém banco 0 nos dois estados,
marca banco 1 `muted` sem eliminar seus dados e declara calibração,
ressonância e headroom N/A/FALHADO. Não há ganho medido inventado.

`--no-pedal-resonance` desativa somente a voz adicional. NPNO mantém o
crossfade padrão; `--no-velocity-crossfade` permite o contrato estrito 1/2 voz
em todas as velocities, e `--stereo-mono-fallback` dobra vozes por componente.
Nenhuma dessas opções altera NKI ou NSMP indevidamente.

## Evidência reproduzível

Medições compactas em `format-validation/npno-sustain/synthetic-matrix.csv` e
`synthetic-calibration.json`; não contêm nem redistribuem uma biblioteca Nord.
Probes adicionais dos goldens binários, sem crossfade nem calibração isotônica:

| Fixture sintetizado | Pares ON/OFF | Ganho ON total | Erro RMS máximo entre bancos | Pico do acorde adicional |
|---|---:|---:|---:|---:|
| layers-v5 | 44 | +1,804..+1,863 dB | 0,012379 dB | −29,286 dBFS |
| layers-v6 | 44 | +1,804..+1,862 dB | 0,005451 dB | −28,312 dBFS |

Esses probes passaram contagem, classe, nível relativo, ressonância finita,
patamar estável (variação <0,5 dB) e soltura/bouncing. O nível absoluto desses
goldens muito baixos continua **FALHADO para o alvo T5**, limitado por +10,8 dB;
"PASS" do executável testa o teto sem clipping e não deve ser lido como aceite
do alvo absoluto. CSVs e offsets completos em `v5-matrix.csv`, `v6-matrix.csv`
e respectivos `*-measurements.json`. A unidade Q23 recebe explicitamente o limite de projeto 4 s.

Os goldens v5/v6 com calibração isotônica geram 1536 regiões (1152 primárias
e 384 extras), reutilizando os mesmos 18 WAVs, sem duplicar PCM. A subdivisão
por tecla não acrescenta vozes simultâneas além do componente de ressonância.

A memória permanente de parâmetros está coberta pelo teste existente e seus
arquivos não foram alterados. Resultados completos de testes são registrados
em `format-validation/npno-sustain/test-results.txt`.

```sh
export LP_REAL_SFIZZ_LIBRARY=/tmp/lp-real-sfizz-build/library/lib/libsfizz.so
export LP_IMPORT_TEST_OUTPUT=/tmp/lp-import-sustain
bash scripts/verify_timbre_import.sh /lib/x86_64-linux-gnu/libsndfile.so.1
cmake -S . -B /tmp/lp-sustain-cmake -DLP_TEST_ONLY=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DLP_SNDFILE_LIBRARY=/lib/x86_64-linux-gnu/libsndfile.so.1 \
  -DLP_REAL_SFIZZ_LIBRARY="$LP_REAL_SFIZZ_LIBRARY" \
  -DLP_NPNO_LXL_REAL_FIXTURE=""
cmake --build /tmp/lp-sustain-cmake -j4
ctest --test-dir /tmp/lp-sustain-cmake --output-on-failure
```

Nesta execução, `cmake`/`ctest` correspondem a `/root/.local/bin/cmake` e
`/root/.local/bin/ctest`; o backend foi compilado antes com:

```sh
git clone --recursive --depth 1 --branch 1.2.3 https://github.com/sfztools/sfizz.git /tmp/lp-real-sfizz
cmake -S /tmp/lp-real-sfizz -B /tmp/lp-real-sfizz-build \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DSFIZZ_SHARED=ON -DSFIZZ_JACK=OFF -DSFIZZ_RENDER=OFF \
  -DSFIZZ_TESTS=OFF -DSFIZZ_BENCHMARKS=OFF -DSFIZZ_USE_SNDFILE=OFF
cmake --build /tmp/lp-real-sfizz-build -j4
```

As saídas SFZ antigas não são reescritas automaticamente. Reconverter o original
em uma pasta nova aplica esta política; não substitui áudio nem reabre a
biblioteca antiga silenciosamente. Não foi produzido novo EXE/ZIP nesta etapa.

O backend usado é sfizz 1.2.3, commit
`4e70dc0bef53b41f2853ed46e26f5911114c92d0`, compilado localmente sem plugins,
JACK ou Actions. Nenhuma quota autenticada de GitHub está exposta; saldo
permanece desconhecido. Publicação somente de fontes e métricas compactas.
