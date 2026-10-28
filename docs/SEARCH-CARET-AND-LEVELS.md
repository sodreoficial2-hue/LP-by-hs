# Busca, MIDI Learn e viabilidade de nivelamento — 28/10/2026

Base: `3255e3898d4c9731db73732b90df97df4f437be5`, ramo
`improvements/stage-readiness`, que reúne as melhorias mais recentes.

## Alteração implementada

O renderer Vision reconstruía o campo de busca ao alterar a lista filtrada,
restaurando apenas o foco. Agora conserva texto local, posição do cursor,
início/fim e direção da seleção antes de reconstruir a interface. A mesma
correção cobre as quatro buscas da Performance e campos de texto focados.
Não há alteração no protocolo nativo, motor de áudio ou formato dos perfis.

## MIDI Learn dos favoritos

Confirmada a ligação completa no código: clique direito/Shift+F10 na interface
Vision, comando `learnFavorite`, `MidiRouter::learnFavorite`, captura do PC e
retorno de confirmação pelo dispatcher. Favorito N aceita PC N no controlador,
valor N-1 no protocolo MIDI, e guarda o canal no perfil salvo manualmente.
PC incorreto mantém a espera; notas e CC não atribuem favoritos. Permanecem os
testes existentes para PC 1/20, canal, cancelamento e captura indevida.

## Áudio: avaliação, sem alteração aplicada

Volume 100%/ganho 0 dB não uniformiza instrumentos gravados com níveis distintos.
Picos iguais em 0 dBFS também não garantem volume percebido igual; acordes,
camadas e efeitos podem ultrapassar o limite. A saída atual apenas mede o pico
e sinaliza excesso, sem normalização por instrumento.

O caminho viável é uma compensação estática por preset, calculada com uma
sequência de referência em diferentes registros e velocidades, preservando a
resposta à dinâmica. É necessário medir o banco efetivamente instalado,
incluindo os presets da expansão SF2, e validar o equilíbrio auditivo. Os
arquivos recuperados deste repositório não representam todo esse banco.
Performances exigem conferir também a soma das quatro partes e efeitos.
Um limitador final pode conter picos, mas sozinho não nivela os timbres e não
substitui a calibração. Não foi acrescentado ganho automático ou compressor
permanente sem essa avaliação, nem alegado nivelamento que não foi medido.

## Verificação

- Sintaxe JavaScript: aprovada.
- `tests/search_caret_test.cjs`: aprovada executando a função `render` de
  produção com DOM simulado, inclusive digitação sequencial, acentos, seleção
  reversa, busca vazia e rolagem. Não equivale a execução em WebView2.
- Regressão C++ de sessão/perfis: aprovada.
- Regressão C++ motor/MIDI: aprovada com biblioteca sfizz simulada. A compilação
  usou `-Wno-error=misleading-indentation` para avisos preexistentes em
  `sf2_engine.cpp` e `dsp.cpp`; o script estrito original para nesses avisos.
- Navegador real indisponível: Chromium não instalado; tentativa de download
  retornou arquivo inválido. Fixture Playwright não executada.
- Não compilado nem executado no Windows nesta etapa; nenhum teste físico de
  controlador MIDI/ASIO é alegado.
- Nenhum portátil, instalador ou Actions gerado. Cota autenticada do GitHub
  desconhecida; nenhuma ferramenta disponível expõe o saldo.
