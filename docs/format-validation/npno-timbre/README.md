# Reprodução — ajustes timbrísticos NPNO/NPMO, 04/11/2026

Base: `e88eac85f8223682580c4f139a54e491889b684a`. Todo processamento é offline.
A interface, AudioHost, criador SF2 e motores/DSP de palco não mudam.

```sh
export LP_REAL_SFIZZ_LIBRARY=/caminho/absoluto/libsfizz.so
export LP_IMPORT_TEST_OUTPUT=/tmp/lp-format-validation
bash scripts/verify_timbre_import.sh /lib/x86_64-linux-gnu/libsndfile.so.1
cmake -S . -B build-npno -DLP_TEST_ONLY=ON -DCMAKE_BUILD_TYPE=Debug \
 -DLP_SNDFILE_LIBRARY=/lib/x86_64-linux-gnu/libsndfile.so.1 \
 -DLP_REAL_SFIZZ_LIBRARY="$LP_REAL_SFIZZ_LIBRARY"
cmake --build build-npno --parallel 4
ctest --test-dir build-npno --output-on-failure
# Isolar a falha legada OSC/ADSR sem ocultá-la no resultado completo:
ctest --test-dir build-npno --output-on-failure \
 -E '^real_sfizz_envelope$'

# Pasta de saída deve ser nova; o binário e SHA-256 originais são preservados.
build-npno/lp_convert_timbre piano.npno /tmp/piano-convertido \
 /lib/x86_64-linux-gnu/libsndfile.so.1 --velocity-crossfade --crossfade-width=8 \
 --calibrate-sfizz="$LP_REAL_SFIZZ_LIBRARY"
# Compatibilidade com players sem região estéreo:
# acrescentar --stereo-mono-fallback
# Desligar crossfade do NPNO: --no-velocity-crossfade
# --velocity-crossfade=8 continua aceito por compatibilidade.
```

NSMP/NKI continuam com crossfade opt-in, usam os mesmos parâmetros, e aceitam a
calibração explícita. Sem `--calibrate-sfizz`, não ocorre renderização de calibração:
o sidecar registra N/A, mantém trims e ganhos iniciais. NPNO sempre declara
`amp_veltrack=0`; não há duas curvas de amplitude aplicadas em série.
A CLI falha em opções desconhecidas e largura fora de 1..16.

`LP_NPNO_V5_REAL_FIXTURE` e `LP_NPNO_V6_REAL_FIXTURE` são caminhos CMake opcionais
para módulos pessoais com versões internas 0x450 e 0x464. Os testes fazem skip 77
quando ausentes. O fixture genérico anterior `LP_NPNO_REAL_FIXTURE` permanece.

## Referência de calibração e alcance

O algoritmo mede RMS estéreo integrado em 4096 frames a 48000 Hz, em sintetizadores
privados em freewheeling (leitura offline síncrona). Limites inferiores de velocity
0 são convertidos para 1 antes da medição, preservando a semântica note-on.
Cada região resultante é medida nas teclas baixa/média/alta únicas de seu
alcance e nas velocities baixa/média/alta únicas. Para NPNO, a subdivisão por tecla
é realizada antes da medição: a cobertura do módulo atravessa os três registros,
mas uma região de uma única tecla só pode ter uma posição de tecla. Isso evita
tratar a duração reamostrada de um key-release como um erro único de toda a zona.
A subdivisão não duplica PCM, não cria layers/RR e é limitada a 65536 regiões.

Referência **de projeto**: RMS do PCM mais trim existente, sem tracking adicional;
as camadas descendentes são reunidas por regressão isotônica ponderada pela
largura original de velocity. Isso produz uma referência não decrescente entre
camadas em cada tecla. Os bancos 1/2 são comparados ao ataque normal na velocity
central de sua camada e recebem alvos relativos de -12/-18 dB. Estes números são
escolhas explícitas, não coeficientes de ressonância descobertos no CNSP.

O resultado é aplicado somente ao `volumeDb`, limitado a [-144,+10.8] dB.
Cada região registra offset e erro máximo medido; RMS é renderizado novamente
após compensar. Tolerância sintética: 0,5 dB. Módulos acima dela ganham limitação
explícita; não se afirma aprovação timbrística. Região silenciosa não recebe
boost adivinhado: a calibração falha, preservando rollback. A seleção de camadas
usa a curva Touch registrada no source_metadata; **a curva contínua de amplitude
do DSP de origem não foi recuperada**. Isso limita o aceite amplo de T3.1.

O fallback mono recebe -3,01029995664 dB por lane para igualar a lei de pan
estéreo do sfizz; o teste permite 0,02 dB de diferença numérica e exige
correlação L/R maior que 1-1e-8. Não modifica PCM.

O erro mede regiões isoladas, antes do crossfade. Não mede equivalência auditiva,
energia de misturas de fases arbitrárias, nem ressonância simpatética. Abrir dois
samples numa janela de crossfade não garante ausência de cancelamento/coloração.

## Confronto independente de campos

Consultados em 04/11/2026:

- [nord-format 0.8.0, mod.rs](https://docs.rs/nord-format/0.8.0/src/nord_format/formats/npno/mod.rs.html), também recuperado por HTTPS local: `DIRECTORY_AT`, `RECORD`, `REC_*`, `Stroke::layer/trim/decay/ladder`, `Library::damper_top` e enum `Bank`.
- [OpenNord nsp-audio.ts, c86b22a](https://github.com/simonflore/opennord/blob/c86b22a8014afcc9d6dc96409992f10f3c004f57/src/lib/ns4/nsp-audio.ts); também HEAD consultado `19b5f59d4511f5a66ebaae86e0c8cfa29f5a68a3`. Comparação com `readStroke`, `strokeFor` e `touchAttenuation`.
- `NSP-FORMAT.md`: não localizado nas árvores consultadas de OpenNord. URL solicitada no GitHub não recuperável. Não se presume conteúdo desse documento.
- Tentativa adicional `https://jeroenvanzutphen.nl/nord/FORMAT`: indisponível. Não usada como evidência.

| Campo | Decisão |
|---|---|
| CNSP, versões 0x450/0x464, keymap +0x8c, fine tune +0x18c | Paridade estrutural com nord-format |
| Diretório +0x732, registro 0x76, bank +4, layer +5 | Paridade estrutural com ambas as fontes |
| loopStart +0x1c, length +0x20, decay +0x2e | Paridade de offsets com OpenNord; coeficiente não convertido em release global |
| trim +0x34 | nord-format descreve u16 em dB; v5 corrigido para unsigned. v6 conserva legado signed/tenths: divergência não resolvida, bytes preservados |
| threshold +0x74 | Preservado cru no registro; v5 usa midpoint em layer×5, v6 byte direto. OpenNord descreve midpoint; nord-format usa outra regra por layer |
| Touch | Tabela legada mantida: fontes divergem. `threshold-comparison.json` compara 201 limiares com a aproximação log do OpenNord e documenta até 20 passos de diferença; não é erro medido contra hardware |
| damperTop +0x40d | Confirma limite de amortecimento; não confirma ausência de releases acima dele. Não silenciar regiões sem prova |
| fade +0x72/+0x73 e decay ladder | Sem unidade/semântica suficiente para converter em segundos: 0,2/2,0/0,2 s conservadores registrados |

A incerteza legada declarada de dois passos MIDI não resolve a divergência entre
fontes. O sidecar passa a dizer que essa estimativa **não é confirmação independente**.
O banco 1 é documentado no nord-format como substituto de ataque com pedal:
agrupar seus samples não o transforma em DSP de ressonância de notas já soando.

Cota autenticada GitHub desconhecida: nenhuma ferramenta de quota autenticada
exposta. Build/testes locais; sem Actions, computação hospedada ou upload de EXE.
