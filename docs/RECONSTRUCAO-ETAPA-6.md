# Etapa 6 — MIDI Learn dos faders de parâmetros

Hugo relatou que não conseguiu iniciar MIDI Learn com o clique direito no fader. A investigação encontrou a captura CC no roteador, mas a interface dependia do encaminhamento do menu de contexto do controle nativo até a janela principal. Não havia confirmação visível após a captura, e o primeiro CC somente gravava o vínculo, sem aplicar o valor ao som. Não foi reproduzido o clique no Windows do usuário; foram corrigidas essas lacunas identificadas no código.

## Comportamento implementado

- Cada um dos 14 faders e seus rótulos tem subclassificação Win32 explícita para clique direito/contexto. O gesto envia diretamente a solicitação de aprendizado do parâmetro. Não altera a posição do fader com o botão direito.
- Shift+F10 ou a tecla de menu de contexto, com foco no fader, inicia a mesma captura. Setas e Tab seguem com o processamento nativo.
- Para editar/memorizar os parâmetros é necessário selecionar um timbre nos Favoritos e abrir a entrada MIDI. Se faltar uma dessas condições, a interface informa a ação necessária em português. O clique na área de faders desabilitados também tenta apresentar essa orientação pela janela pai.
- O controle em captura mostra “aguardando CC”. Ao receber MIDI CC válido, mostra o número aprendido e a confirmação de que o controle está ativo; o primeiro valor recebido já atualiza parâmetro, fader e áudio.
- O mapeamento pertence ao favorito selecionado. A captura guarda o slot de origem e é cancelada se o usuário muda de timbre. ESC, fechar o painel, trocar/desligar a entrada MIDI ou abrir outro perfil também cancelam a captura pendente.
- Um CC controla apenas um parâmetro por favorito; reaprender esse CC para outro fader transfere o vínculo nesse favorito. Os CCs de knobs mantêm o comportamento anterior de aceitar os canais da entrada MIDI selecionada.
- Notas continuam musicais. Elas não capturam parâmetros. Program Change continua exclusivo para selecionar/aprender favoritos 1–20; não é interpretado como CC dos faders.
- Vínculos e ajustes são guardados pelo botão Salvar perfil e restaurados com Abrir perfil. O formato de perfil permanece v4; nenhum salvamento automático de show foi acrescentado.

## Sequência de uso

1. Selecione a entrada MIDI do teclado.
2. Selecione um timbre nos Favoritos e pressione E para abrir os parâmetros.
3. Clique com o botão direito no fader ou no nome do parâmetro.
4. Mova o fader/knob físico que envia MIDI CC.
5. Confira a mensagem “CC ... aprendido ...” e use Salvar perfil.

## Verificação

- Testes dos 14 parâmetros: captura CC, aplicação do primeiro movimento, extremos 0/127, movimentação posterior, cancelamento, reassociação e preservação do favorito de origem.
- Notas não completam a captura; Program Change para outro favorito cancela a captura de parâmetro.
- Salvar/Abrir mantém os vínculos; um CC restaurado altera o parâmetro esperado.
- Integração com host e sfizz simulados: o primeiro CC aprendido no volume altera as amostras renderizadas e o stream não é reiniciado.
- Cinco grupos de testes locais passaram, incluindo a regressão de renderização real sfizz da etapa anterior.
- ASan/UBSan nos testes de MIDI e host, com LeakSanitizer desativado conforme limitação já registrada.
- Compilação Windows x64 passou com a subclassificação dos controles.
- Ainda falta confirmar o clique físico e o feedback visual no Windows com o controlador do usuário. Testes de roteamento/compilação não substituem essa validação.

Referências Win32: https://learn.microsoft.com/pt-br/windows/win32/menurc/wm-contextmenu ; https://learn.microsoft.com/pt-br/windows/win32/controls/subclassing-overview .

Sem novo ZIP, instalador ou envio ao Drive. Nenhuma execução Actions. Saldo autenticado de API/billing/Actions continua desconhecido porque as ferramentas não o expõem. Próximo passo: layout, preservando este gesto e o uso exclusivo de Program Change para os favoritos.
