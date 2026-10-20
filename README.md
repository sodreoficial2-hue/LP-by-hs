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

Integração dinâmica sfizz, roteador MIDI com MIDI Learn de CC, fila de eventos e primeira interface Win32 implementados. A interface apresenta explicitamente que a saída ASIO está pendente. Os parâmetros são armazenados no perfil; aplicação de DSP ainda pendente.

Detalhes e limites de validação: [docs/RECONSTRUCAO-ETAPA-2.md](docs/RECONSTRUCAO-ETAPA-2.md).

Testes Linux sem GitHub: `bash scripts/verify_local.sh`.
No Windows, o projeto CMake também gera o alvo `LivePerformance`. As DLLs reais sfizz e dependências devem ficar junto ao executável. Não substituir o programa original por esta reconstrução ainda.

## Próximas etapas

Integrar stream PortAudio/ASIO, seleção/persistência de driver e ASIO Control; aplicar DSP/ADSR; finalizar categorias, perfis e MIDI Learn de botões. A nova base ainda não abre saída de áudio, não inclui instalador novo e não foi homologada em hardware Windows. Os executáveis originais permanecem separados dela.
