# Etapa 2 — sfizz, MIDI e interface Windows

## Código implementado

- `sfizz_engine`: carregamento dinâmico da API real sfizz, validação de funções, notas/CC/pânico e renderização estéreo por blocos. SFZ candidato é carregado em sintetizador separado; falha não substitui o timbre anterior.
- `midi.hpp`: decodificação de mensagens curtas WinMM, roteamento de Program Change 0–19, Note On com velocity zero como Note Off, MIDI Learn de CC por parâmetro/favorito. A captura usa um mapeamento por CC para evitar edição dupla.
- Fila SPSC fixa de eventos MIDI com indicação de overflow para que a futura callback solicite pânico. A integração com callback de áudio ainda está pendente.
- `win32_app`: primeira interface nativa, fonte grande, fundo escuro, catálogo SFZ, favoritos, parâmetros de perfil, importação SFZ, entrada MIDI WinMM, ESC, E, Tab, setas e Espaço no catálogo. Botão direito sobre controle inicia MIDI Learn.
- Diálogos nativos Abrir/Salvar de perfil `.lphs`. Formato próprio v1, sem compatibilidade com os INI antigos.
- Salvamento valida os dados, grava temporário exclusivo e substitui o perfil somente após concluir e descarregar a gravação. Falha de validação mantém o perfil existente.

## Contrato do motor

Não chamar load/unload/destrutor no callback de áudio. A API do motor nesta etapa exige uma única thread proprietária ou stream parado durante alterações. O próximo estágio precisa integrar PortAudio/ASIO e estabelecer a troca segura entre threads. O código não promete troca sem interrupção ou caudas preservadas.

A interface mostra explicitamente que a saída ASIO está em reconstrução. Os controles guardam valores de perfil, mas não aplicam ainda ADSR, filtro ou DSP ao motor. Renderizar um bloco com sfizz não abre uma saída de áudio no Windows.

## Validação local

- Compilação C++17 Linux com `-Wall -Wextra -Werror`.
- Testes de persistência, valores inválidos, manutenção do show diante de perfil corrompido e preservação do arquivo após tentativa inválida de salvar.
- Biblioteca sfizz simulada (somente teste) para verificar ABI, chamadas estéreo, silêncio, notas, pânico e preservação do sintetizador anterior ao falhar o carregamento. Isso não comprova que as amostras reais soem.
- Testes MIDI Learn, Program Change e fila concorrente de 100.000 eventos em ordem.
- ASan/UBSan local com verificação de leaks desativada: LeakSanitizer não consegue ler `/proc` neste ambiente. Não foi alegada validação de leaks.
- Compilação cruzada Windows x64 da interface e do motor com MinGW. O EXE gerado apenas para verificar build não é um instalador homologado.
- Não foram realizados teste visual no Windows, áudio real, latência ASIO nem teste com controlador físico.

## Uso do GitHub

Verificação da conexão: endpoint autenticado `/rate_limit` indisponível; ferramentas não expõem saldo de uso/billing. Saldo restante desconhecido. Nenhuma execução Actions foi solicitada. Compilações e testes executados neste ambiente local. Publicação prevista: um commit, apenas fontes e documentação, reaproveitando todos os arquivos recuperados existentes.

Referências primárias: https://sfz.tools/sfizz/api/sfizz.h/ ; https://learn.microsoft.com/windows/win32/api/mmeapi/nf-mmeapi-midiinopen ; https://docs.github.com/en/rest/using-the-rest-api/best-practices-for-using-the-rest-api .
