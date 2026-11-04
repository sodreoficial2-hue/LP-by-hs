# Importação npno / npmo

O parser e o adaptador estão implementados no código-fonte. A detecção exige `CBIN` + tag `npno` + corpo `CNSP` no offset indicado pelo envelope. `.npno`, `.NPNO`, `.npmo` e `.NPMO` usam o mesmo leitor. Uma extensão aceita não torna válido um arquivo com outra assinatura. A identificação por conteúdo também funciona sem extensão; o catálogo continua exigindo uma das extensões registradas.

O motor e o DSP não foram alterados. A saída passa por `WavWriter` e `writeSfzRegion`, os mesmos escritores usados pelos outros conversores; a adição de controles opcionais mantém a saída anterior dos conversores existentes. O parser devolve diretório, zonas, camadas e PCM. O adaptador grava um instrumento completo, não um SFZ por amostra.

## Estrutura e dados preservados

Consulte `ESTRUTURA.md` para offsets/tipos/fontes e divergências, `FIXTURES-ESTRUTURA.json` para valores de cada arquivo e `hexdumps/` para cada byte dos vetores. A documentação da string table de jogos não foi usada: é outro contêiner.

| Campo de origem | Uso no SFZ | Preservação / limite |
|---|---|---|
| Mapa tecla→root | `lokey`, `hikey`, `pitch_keycenter` | Teclas 255 permanecem descobertas, como na fonte |
| Afinação i8 por tecla | `tune`/`transpose` | 0,7 cents/unidade segundo referência; diferenças dividem regiões/grupos |
| Piso v5 / limiar v6 | `lovel`, `hivel` | Perfil de toque de referência aproximado; auditoria por grupo e banco cobre 1–127 |
| Loop início+comprimento | `loop_start`, `loop_end` inclusivo | Sem subtrair do início; fim = início+comprimento−1 |
| Trim e ganho global | `volume` | Escalas v5/v6 distintas; boost v6 limitado a 10,8 dB |
| Banco 0 / 1 / 2 | Ataque / `locc64` ou `hicc64` / `trigger=release_key` | Não interpretar ressonância como round robin |
| Estéreo | Dois WAV mono PCM16 com `pan=-100` e `pan=100` | Conserva ambos os canais; duplica vozes/regiões SFZ |
| Decaimentos, fades, release-level, dados desconhecidos | Não há tradução exata | Prefixo completo e registros completos em hexadecimal no sidecar; original integral mantido |
| Pan, round robin e detune global | Sem offset confirmado | Nenhum valor inventado; take ID não vira sequência. O escritor compartilhado aceita sequência quando outro parser a fornecer |

Os opcodes SFZ corretos são `lovel`/`hivel` e `loop_start`/`loop_end`; `vel_lo`, `vel_hi` e `loop_point` não são emitidos. O detune global não foi encontrado no schema consultado; não foi duplicado ou simulado com afinação por região.

O importador conserva o original como identidade do item da biblioteca e resolve seu `.converted.sfz` somente para tocar. O diretório contém original, `<original>.converted.sfz`, `Samples/`, `<original>.meta.json` e `<original>.audit.json`. Nomes WAV combinam prefixo ASCII do arquivo com hash e índice/canal. Tudo é relativo e relocável. A cópia do original é verificada por SHA-256; falhas removem apenas a pasta nova de conversão, e a publicação acontece por rename no pipeline já existente.

## Testes e compilação

Testes nativos, sem SDK ASIO:

```sh
cmake -S . -B build -DLP_TEST_ONLY=ON -DCMAKE_BUILD_TYPE=Debug -DLP_SNDFILE_LIBRARY=/lib/x86_64-linux-gnu/libsndfile.so.1
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Em Windows, substitua o caminho da libsndfile pelo caminho absoluto de `sndfile.dll`, rode os comandos em um prompt de desenvolvimento C++ e execute `ctest --test-dir build -C Debug --output-on-failure` para um gerador com múltiplas configurações. Não apontar a variável para uma biblioteca de importação `.lib`; o pipeline carrega a DLL em runtime.

Compilação do aplicativo com as dependências que o projeto já exige:

```powershell
cmake -S . -B build-win -A x64 -DLP_PORTAUDIO_SOURCE=C:/deps/portaudio -DASIOSDK_ROOT_DIR=C:/deps/ASIOSDK -DLP_ENABLE_VISION_UI=ON -DLP_WEBVIEW2_SDK_ROOT=C:/deps/webview2 -DLP_SNDFILE_LIBRARY=C:/LP/sndfile.dll
cmake --build build-win --config Release --target LivePerformance LivePerformanceVision lp_convert_timbre
```

Os SDKs não foram adicionados ao pacote de fontes. Preserve as DLLs e os recursos da instalação atual ao preparar seu executável. Esta entrega contém fontes/diffs/testes/exemplo, não um novo executável gráfico instalado no computador do usuário.

Conversão independente, usando pasta de saída que ainda não existe:

```powershell
.\build-win\Release\lp_convert_timbre.exe .\tests\fixtures\npno\tine-demo.npno C:/Temp/piano-convertido C:/LP/sndfile.dll
```

Em um gerador de configuração única, o executável fica diretamente no diretório de build. O comando em Linux é `./build/lp_convert_timbre INPUT OUTPUT /lib/x86_64-linux-gnu/libsndfile.so.1`. O diretório de saída deve ser novo; saídas existentes são preservadas.

Para acrescentar um arquivo pessoal ao gate:

```sh
cmake -S . -B build -DLP_NPNO_REAL_FIXTURE=/caminho/arquivo.npno
cmake --build build --parallel
ctest --test-dir build -R npno --output-on-failure
```

Para o teste de renderização real, configure também `LP_REAL_SFIZZ_LIBRARY` com o caminho absoluto de `sfizz.dll`/`libsfizz.so`, build do alvo `lp_npno_sfizz_test` e rode `ctest -R npno_real_sfizz_render`. Esse teste carrega o SFZ pelo motor existente, confere a contagem de regiões e mede áudio em velocities 1/30/80/127, com pedal aberto/fechado e soltura. Ele gera `preview.wav` da demo sintetizada. Sua mera compilação não significa que a audição comparativa foi concluída.

## Importação na interface

1. Compile e abra a versão atualizada do aplicativo.
2. Use **Importar**, no mesmo fluxo de NKI/NSMP. Selecione `.npno` ou `.npmo` pelo filtro de timbres.
3. Acompanhe leitura, extração e zonas/camadas. Checksum inconsistente é aviso; assinatura, versão, diretório, loop ou PCM inválidos cancelam a importação com diagnóstico.
4. Ao concluir, o instrumento aparece em **Meus Sons** e carrega o SFZ derivado. O arquivo original permanece junto aos artefatos para futura reconversão.
5. Abra o `.audit.json` junto ao original. Compare zonas, registros de amostra, WAVs e bytes PCM; `divergences` deve ser zero. Em estéreo, cada registro produz dois WAV mono. Grupos podem dividir uma zona de root quando há afinação por tecla ou mudança de abafamento. Contagens de registros e de regiões não são a mesma grandeza.
6. Teste notas em cada zona, velocities fracas/médias/fortes, CC64 e soltura. Compare com gravação do original quando disponível. Leia as limitações do `.meta.json` antes de usar em apresentação.

## Evidência e limitações conscientes

Na execução de validação, 28 testes passaram; dois testes de arquivos externos foram sinalizados como `Skipped` porque nenhuma entrada pessoal foi fornecida. O gate de importação compara **cada valor PCM** de 39 registros com um decodificador independente, verifica WAV PCM16 sem normalização, loops, afinação, bancos, cobertura de velocity, aliases, catálogo, hashes e rollback de dados corrompidos. A CLI/conversores e o teste de renderização compilaram para Windows x64; o arquivo Win32 da interface passou por verificação de compilação. As alterações do formato não mexem em SF2, SFZ, NSMP ou NKI existentes.

O teste com sfizz real não foi executado aqui: o runtime Windows necessário encontrou bloqueio de criação de socket neste ambiente. Não existe gravação de referência fornecida para comparação auditiva. O arquivo público tem áudio sintetizado. As revisões diferentes de 5.4 foram exercitadas por layout sintético, e não por um corpus pessoal/factory. Portanto **não alegamos que o critério de fidelidade auditiva em arquivos pessoais foi concluído**.

Curva Touch 0 é aproximada, e o perfil de outro teclado pode selecionar as camadas em velocities diferentes. Amplitude segue o tracking de velocity padrão do SFZ. Fades usam release conservador de 0,2 s, ou 2 s nas teclas não abafadas; isso não reproduz a curva do DSP original. Decaimento exclusivo do loop e escada por transposição são conservados no sidecar, sem emulação exata. Pan/round robin/detune global não foram inventados. Camadas inalcançáveis são avisadas e extraídas, mas não recebem regiões artificiais; as camadas alcançáveis continuam cobrindo 1–127. Trim negativo em v5 vira -144 dB, aproximação de silêncio. Limites defensivos: entrada 512 MiB, 8192 registros, 16 milhões de frames por registro, 256 milhões de valores PCM e 65536 regiões SFZ.

Zero divergências no relatório significa consistência da extração e do mapeamento, não equivalência sonora de recursos ainda aproximados. O relatório diz explicitamente `audio_fidelity_verified:false`. Créditos e licenças das referências abertas estão em `third_party/npno/NOTICE.md`.
