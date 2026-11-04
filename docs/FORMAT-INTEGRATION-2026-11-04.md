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
