# Etapa 7 — banco curado inicial e categorias

66 presets SFZ instalados no protótipo local em 11 categorias. O catálogo explícito evita exibir arquivos auxiliares, keyswitches internos e placeholders como novos timbres. Os 20 favoritos e o contrato PC 1–20 continuam separados das categorias.

| Categoria | Presets |
|---|---:|
| Pianos acústicos | 4 |
| Pianos elétricos | 7 |
| Órgãos | 4 |
| Cordas | 7 |
| Metais | 10 |
| Leads | 8 |
| Pads | 4 |
| Percussão | 9 |
| Bateria | 1 |
| Acordeons | 1 |
| Outros | 11 |

São 4 desenhos de lead, cada um com variante mono e poly. Não são 8 instrumentos acústicos diferentes. O baixo sintetizado usa amostras do Surge e filtro próprio. O FM Clavisynth é um sintetizador TX81Z, não uma gravação de Clavinet eletromecânico. Os órgãos drawbar, percussivo e rock são emulações amostradas. O pad de coro também é sintetizado.

## Integração

- Novo seletor de categorias com controles nativos grandes e navegação por teclado.
- Enter/double-click carrega o timbre; Espaço adiciona aos favoritos. Importações SFZ aparecem em Outros.
- Novos favoritos guardam `bank://id-estavel`; mover a pasta do aplicativo junto com soundbank preserva a referência. Perfis antigos e SFZ importados mantêm seu formato de caminho anterior.
- Um banco ausente permite o catálogo legado; um banco instalado porém inválido produz erro explícito.
- Todas as fontes, licenças, créditos e o inventário SHA-256 acompanham o banco. As amostras não foram reamostradas.
- O montador resolve includes e macros em sequência, preservando as 16 camadas do Salamander e os nomes originais. Os cabeçalhos globais de três patches CC0 foram explicitados para sfizz. Os leads mono recebem choke de grupo; kits de percussão usam o número real de round-robins de cada dinâmica.

## Montagem local

As amostras ficam fora do Git. Restaurar o banco salvo colocando a pasta `soundbank` junto ao executável portátil. Os arquivos ZIP de dados têm caminhos complementares e devem ser extraídos no mesmo diretório. Não contêm executável ou instalador.

Para remontar a partir das fontes licenciadas, extrair os arquivos em subpastas conforme `scripts/assemble_soundbank.py`. O subconjunto VCSL está fixado no commit e nos arquivos de `banks/curated/inputs.lock.json`. Os checksums dos arquivos de origem estão no mesmo inventário; os respectivos projetos estão em `SOURCES.json`.

```sh
python3 scripts/assemble_soundbank.py /caminho/fontes /caminho/soundbank --recovered recovery/extracted/live/samples
cmake -S . -B build-windows -DLP_SOUNDBANK_ROOT=/caminho/soundbank
```

O build Windows também requer MinGW/MSVC, PortAudio e ASIO SDK documentados nas etapas anteriores. `LP_SOUNDBANK_ROOT` copia o banco ao lado do aplicativo; não baixa amostras durante o build.

```sh
LP_REAL_SFIZZ_LIBRARY=/caminho/libsfizz.so LP_BANK_TEST_ROOT=/caminho/soundbank bash scripts/verify_local.sh
```

## Validação e limites

Testes locais de sessão, motor/MIDI, DSP, ASIO simulado, ADSR em sfizz real, catálogo e portabilidade, renderização do banco e contagem de vozes mono/poly. Cada preset é renderizado em três dinâmicas; o Salamander em todas as 16 camadas e os kits em diversas teclas. Os dados de teste são exportados em `banks/curated/render-validation.tsv`.

Compilação Windows com o banco local concluída. A renderização usa sfizz real no Linux, em modo offline. Isso não substitui avaliação musical, latência real, streaming sob carga, audição de todas as teclas e testes Windows/ASIO no teclado do usuário. Não foi executado GitHub Actions, não foi gerado pacote de teste do aplicativo nem enviado arquivo ao Drive.

A franquia autenticada de GitHub permanece desconhecida: o conector não oferece ferramenta de saldo, faturamento ou rate-limit autenticado. Apenas uma atualização agrupada de fontes e metadados desta etapa; nenhuma amostra ou executável enviado ao GitHub.

## Curadoria ainda aberta

Faltam amostras adequadas, licenciadas e verificadas de Clavinet eletromecânico, viola solo e pandeiro brasileiro. Presets específicos de DX7 e Mid Grand ainda não foram incluídos; o banco FM atual é TX81Z. Não substituir esses instrumentos por ondas simples ou por outro instrumento com nome incorreto. A qualidade final e os níveis relativos devem ser avaliados pelo usuário ao tocar, sem inflar a seleção com variações redundantes.
