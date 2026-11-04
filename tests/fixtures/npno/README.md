# Vetores npno

Todos os vetores têm áudio sintetizado, sem material de fábrica. Nenhum arquivo pessoal foi fornecido.

| Arquivo | Origem | Verificação |
|---|---|---|
| `tine-demo.npno` | Demo pública de drawbar, commit b680c5b0b9ec9b8b04da951c6684c39aad9afe48, `crates/nord-format/tests/fixtures/demo/drawbar-tine.npno` | Cópia binária exata; três roots/ataques; versão externa 540 e interna 0x450, envelope tipo 1 |
| `layers-v5.npno` | Encoder sintético independente, commit c86b22a8014afcc9d6dc96409992f10f3c004f57, `src/lib/ns4/nsp-synth.testing.ts` | Envelope 0, mono, dois roots, três bancos e três camadas por banco, loops, afinação de uma tecla |
| `layers-v6.npno` | Mesmo encoder independente | Envelope 1, estéreo, limiares v6, trim negativo, mesmos bancos e camadas |
| `*.pcm` | Decodificador independente `src/lib/ns4/nsp-audio.ts` na revisão acima | PCM16 LE em ordem canal→frames; comparação integral de cada inteiro, sem normalizar |

`manifest.json` contém os SHA-256 e dimensões. `golden.txt` é consumido pelo teste C++. `generate.mjs` usa Node 24 ou superior e um checkout da referência na revisão indicada; importa os leitores/encoders da referência, não o parser C++ sob teste. A preparação do mapa e dos campos de banco/limiar é explícita no script.

Reprodução:

```sh
node tests/fixtures/npno/generate.mjs CAMINHO_PARA_CHECKOUT_DA_REFERENCIA
python tests/fixtures/npno/inspect.py
```

O segundo comando gera relatório estrutural JSON e hexdumps completos em `docs/npno/`. Os nomes dos projetos e as licenças constam em `third_party/npno/NOTICE.md`; o encoder de referência é AGPL-3.0-or-later, e a demo pública é BSD-3-Clause.

Estes vetores demonstram interoperação com produtores independentes e decodificação verificável; **não** são corpus real de todas as versões 5.x/6.x. Para validar um arquivo pessoal, configure `LP_NPNO_REAL_FIXTURE`. A audição comparativa exige o arquivo e uma gravação de referência.
