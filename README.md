# Live Performance by Hugo Sodré

Reconstrução iniciada em 20/10/2026. O programa original continua preservado em `recovery/extracted/live`; esta base nova ainda não o substitui.

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

Integração dinâmica sfizz, roteador MIDI com MIDI Learn de CC, fila de eventos e primeira interface Win32 implementados. A interface apresenta explicitamente que a saída ASIO está pendente. Os parâmetros são armazenados no perfil. A aplicação de DSP foi implementada na etapa 3 descrita abaixo.

Detalhes e limites de validação: [docs/RECONSTRUCAO-ETAPA-2.md](docs/RECONSTRUCAO-ETAPA-2.md).

Testes Linux sem GitHub: `bash scripts/verify_local.sh`.
No Windows, o projeto CMake também gera o alvo `LivePerformance`. As DLLs reais sfizz e dependências devem ficar junto ao executável. Não substituir o programa original por esta reconstrução ainda.

## Etapa 3

Processador estéreo conectado ao renderizador sfizz: volume, filtro passa-baixas, EQ de três bandas, compressor estéreo, tremolo, reverb, delay e chorus. MIDI Learn por nota ou CC para selecionar favoritos, com canais separados e consumo das teclas reservadas. Perfis `.lphs` v2 preservam os botões; leitura dos perfis v1 da reconstrução mantida.

Validação: testes locais com sinais numéricos e sfizz simulado; não são testes auditivos nem de hardware. ADSR por voz, Envelope Filter dinâmico, stream ASIO e painel do driver continuam pendentes. Ver [docs/RECONSTRUCAO-ETAPA-3.md](docs/RECONSTRUCAO-ETAPA-3.md).

## Próximas etapas

Integrar stream PortAudio/ASIO, seleção/persistência de driver e ASIO Control; aplicar ADSR por voz e filtro de envelope; finalizar categorias e compatibilidade dos perfis INI originais. A nova base ainda não abre saída de áudio, não inclui instalador novo e não foi homologada em hardware Windows. Os executáveis originais permanecem separados dela.
