# Live Performance by Hugo Sodré

Reconstrução iniciada em 20/10/2026. O programa original continua preservado em `recovery/extracted/live`; a nova base já compila com saída ASIO, mas ainda não substitui o programa original em uso de palco.

## Material recuperado

- `recovery/extracted/live`: 185 arquivos do instalador Live Performance.
- `recovery/extracted/vision08`: 184 arquivos do protótipo Vision SFZ Native 0.8.
- `recovery/analysis`: hashes SHA-256, comparação das versões, strings, importações PE e desmontagem assembly.
- Licença e créditos das amostras Greg Sullivan preservados junto às bibliotecas.

Os arquivos foram extraídos sem executar os programas. Não foram encontrados fontes originais ou símbolos de depuração. DLLs e executáveis são cópias dos pacotes fornecidos pelo titular do projeto; sua presença não atribui nova licença a componentes de terceiros.

## Primeiro código reconstruído

C++17: catálogo SFZ, 20 slots de favoritos, parâmetros independentes por timbre, mapeamentos MIDI CC e formato novo de perfil. Program Change MIDI 0–19 corresponde aos números 1–20 exibidos nos controladores. Perfis novos **não são compatíveis ainda** com os INI originais.

Compilar: `cmake -S . -B build`, `cmake --build build`, `ctest --test-dir build --output-on-failure`.
Sem CMake: `g++ -std=c++17 -Isrc tests/session_test.cpp -o /tmp/lp_test` e executar `/tmp/lp_test recovery/extracted/live/samples`.

## Etapa 2

Integração dinâmica sfizz, roteador MIDI com MIDI Learn de CC, fila de eventos e primeira interface Win32 implementados. Nesta etapa histórica, a saída ASIO ainda estava pendente (implementada na etapa 4). Os parâmetros são armazenados no perfil. A aplicação de DSP foi implementada na etapa 3 descrita abaixo.

Detalhes e limites de validação: [docs/RECONSTRUCAO-ETAPA-2.md](docs/RECONSTRUCAO-ETAPA-2.md).

Testes Linux sem GitHub: `bash scripts/verify_local.sh`.
No Windows, o projeto CMake gera o alvo `LivePerformance`; a etapa 4 requer os diretórios de PortAudio e do SDK ASIO na configuração. As DLLs reais sfizz e dependências devem ficar junto ao executável. Não substituir o programa original por esta reconstrução ainda.

## Etapa 3

Processador estéreo conectado ao renderizador sfizz: volume, filtro passa-baixas, EQ de três bandas, compressor estéreo, tremolo, reverb, delay e chorus. MIDI Learn dos favoritos exclusivamente por Program Change: favorito 1 → PC 1 até favorito 20 → PC 20. O clique direito aprende o canal do PC correspondente; nota/CC não trocam favoritos. Perfis `.lphs` v3 preservam esses vínculos; leitura v1/v2 mantida, descartando apenas vínculos antigos de nota/CC nos favoritos.

Validação: testes locais com sinais numéricos e sfizz simulado; não são testes auditivos nem de hardware. Nesta etapa histórica, ASIO e painel ainda estavam pendentes (implementados na etapa 4). ADSR por voz e Envelope Filter dinâmico continuam pendentes. Ver [docs/RECONSTRUCAO-ETAPA-3.md](docs/RECONSTRUCAO-ETAPA-3.md).

## Etapa 4

Saída PortAudio/ASIO ligada ao motor sfizz, com driver, buffer, ASIO Control, pânico e persistência automática exclusiva de áudio. Fila MIDI e parâmetros atômicos conectados ao callback; preparação SFZ fora do callback. Enter no catálogo permite tocar sem favoritar. Compilação Windows x64 com backend ASIO real concluída aqui, sem Actions. Ver [docs/RECONSTRUCAO-ETAPA-4.md](docs/RECONSTRUCAO-ETAPA-4.md).

Nesta etapa foram quatro grupos de testes locais. Posteriormente, o usuário confirmou áudio ASIO, efeitos, volumes e comandos funcionando com fluidez no Windows; persistência na reabertura ainda precisava de correção. O pacote permanece experimental.

## Etapa 5

O usuário validou a fluidez do ASIO, efeitos e comandos da etapa 4 no Windows. Esta etapa reforça a reconexão automática do último ASIO e implementa ADSR nativo por região/voz do sfizz e Envelope Filter dinâmico. Perfis v4 guardam o estado ADSR, com leitura das versões anteriores. Detalhes e limites: [docs/RECONSTRUCAO-ETAPA-5.md](docs/RECONSTRUCAO-ETAPA-5.md).

Cinco grupos de testes locais, incluindo renderização real sfizz com as amostras recuperadas. As alterações da etapa 5 ainda exigem validação no Windows do usuário. Não foi publicado novo pacote: o próximo trabalho é o layout, conforme solicitado.

## Próximas etapas

Layout acessível, categorias e acabamento visual; teste integrado das alterações no Windows; compatibilidade dos INI originais e troca de timbres sem interrupção. Os executáveis originais e o pacote da etapa 4 permanecem preservados.
