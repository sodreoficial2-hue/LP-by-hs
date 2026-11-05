# NPNO/NPMO: resposta ao toque suave — 5 de novembro de 2026

Base: `ba91ac6df8981d0118101c35d3b84d62e9a7969e` (sustain).
O usuário relatou que toques muito fracos não resultavam em volume proporcional.

A auditoria encontrou um defeito concreto: `amp_veltrack=0` preservava o nível
PCM/trim das camadas, mas também deixava o volume constante dentro de cada
camada. A camada mais suave dos goldens v5/v6 cobre velocities 1..61; velocities
10 e 40 tocavam a mesma amostra, com o mesmo volume. O sweep anterior aceitava
energia não decrescente, inclusive igualdade, e não detectava esse patamar.

A correção aplica uma curva local a todos os componentes NPNO da mesma raiz:

`amplitude(v) = min(v / soft_velocity_ceiling, 1)`

O teto é o maior velocity da camada canônica mais suave do banco 0, registrado
por região em `soft_velocity_ceiling`. SFZ emite `amp_veltrack=100` com pontos
**explícitos** `amp_velcurve_0=0`, `amp_velcurve_<teto>=1` e
`amp_velcurve_127=1` quando teto<127. A curva customizada substitui a lei
quadrática implícita do sfizz; ela não é somada a um tracking global duplicado.
O código primário do sfizz 1.2.3 confirma essa ordem em
`src/sfizz/RegionStateful.cpp::velocityCurve`.

A amplitude cresce linearmente somente até o teto suave. Acima dele permanece
unitária, preservando o equilíbrio PCM/trim existente das demais camadas. Não se
redefine a curva inteira do teclado, nem se declara equivalência com o DSP Nord.
Velocities médias/fortes ainda dependem das camadas e da calibração existentes.

A aplicação ocorre **depois** da calibração isotônica opcional, para que uma
nova normalização não elimine a resposta suave. Banco normal, banco de pedal,
região extra de ressonância e soltura recebem a mesma curva por raiz, inclusive
nos crossfades. A calibração entre bancos mede ambos com a mesma curva.
Originais, WAVs PCM16, limites de zonas, releases e agrupamento são preservados.
Não há alteração em motores, DSP, AudioHost, interface, memória permanente,
NSMP ou NKI. Sem backend sfizz offline, o fallback também conserva a curva suave.

## Medição A/B com libsfizz real

Mesmos PCM, volumes, ganhos de módulo, envelopes, afinação e SFZ. A variante
“antes” remove somente os pontos customizados e restaura `amp_veltrack=0`.
Nota 48, render RMS estéreo de 4096 frames a 48 kHz. Os arquivos v5/v6 são
fixtures sintetizados por encoder independente; não são gravações de um Nord.

| Fixture | CC64 | Delta velocity 10→40 antes | Depois | Maior mudança em 61/80/127 |
|---|---:|---:|---:|---:|
| v5 | 0 | 0 dB | +12,0412 dB | 0 dB |
| v5 | 127 | 0 dB | +12,0412 dB | 0 dB |
| v6 | 0 | 0 dB | +12,0412 dB | 0 dB |
| v6 | 127 | 0 dB | +12,0412 dB | 0 dB |

Em uma mesma amostra suave, dobrar velocity aumenta o nível em +6,0206 dB.
Isso diferencia pianissimo de um toque menos leve, sem aumentar artificialmente
o volume dos toques fortes. O valor é uma política de projeto documentada,
que deverá ser conferida pelo músico com seu controlador; não é uma curva Nord
decodificada nem uma promessa de proporcionalidade psicoacústica exata.

Evidências: `format-validation/npno-soft-velocity/ab-rms.json` e
`soft-velocity.csv`. O CSV de regressão inclui antes/depois, velocities
1/2/4/8/10/16/32/40, v5/v6 e pedal OFF/ON, com assert de inclinação em ±0,02 dB.
A matriz de sustain também verifica 1/2/4/8/16 na camada sintética 1..16.

## Reprodução e limites

```sh
export LP_REAL_SFIZZ_LIBRARY=/tmp/lp-real-sfizz-build/library/lib/libsfizz.so
export LP_IMPORT_TEST_OUTPUT=/tmp/lp-soft-script
bash scripts/verify_timbre_import.sh /lib/x86_64-linux-gnu/libsndfile.so.1
cmake --build /tmp/lp-sustain-cmake -j4
ctest --test-dir /tmp/lp-sustain-cmake --output-on-failure
```

Configuração do backend e CMake: `FORMAT-INTEGRATION-2026-11-05.md`.
Validação: **10 grupos do script PASSOU**, incluindo os cinco originais.
CTest completo: **38 PASSOU, 1 falha legada e 5 SKIP**, 44 registrados (exit 8).
Após ampliar o A/B para 64 pontos, o render compartilhado foi reexecutado com
os dois geradores de fixture: **3/3 PASSOU**, exit 0. Os arquivos protegidos
continuam idênticos à base. O caminho MIDI também foi conferido: AudioHost e
motores repassam a velocity recebida, sem fixá-la em 64 ou 127.

Resultados exatos desta execução em `format-validation/npno-soft-velocity/`.
A falha legada OSC/ADSR, o alvo de headroom não alcançado em fontes muito baixas,
a falta de fixture pessoal L/XL, de gravação A/B de teclado e de execução
Windows/ASIO continuam com o estado publicado na etapa anterior. P1.6 Kontakt
continua FALHADO. Não se atribuem essas pendências à correção do toque suave.

SFZs já convertidos mantêm a política anterior; é necessário reconverter o
original após instalar um executável compilado com esta atualização. Não foi
gerado novo EXE/ZIP nesta etapa. Nenhuma Action ou computação hospedada foi usada;
cota autenticada de GitHub permanece desconhecida.
