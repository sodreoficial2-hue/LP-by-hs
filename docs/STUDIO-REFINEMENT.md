# Revisão do acabamento de todas as telas

## Escopo

Refinamento visual da aplicação nativa, da abertura às páginas internas,
posterior à integração das fotografias. A avaliação estética é subjetiva;
a implementação não representa uma comprovação de nota 10/10.

| Tela | Acabamento implementado |
| --- | --- |
| Abertura | Composição responsiva com retrato, hierarquia tipográfica, progresso, percentual e estimativa de tempo preservada |
| Categorias | Grade única com as 16 categorias, fotos incorporadas, nomes em faixa própria e foco visível |
| Timbres e favoritos | Fotografia da categoria, orientação separada da lista, linhas destacadas e estado vazio desenhado no próprio controle |
| Parâmetros | Colunas com títulos, cartões de altura consistente, faders acessíveis e espaço para valores/mapeamentos |
| Mix da Performance | Mesma linguagem de cartões e faders para os quatro volumes |
| Áudio e MIDI | Painéis de conexão, driver/buffer alinhados, entrada MIDI separada, ações distribuídas e área de frequência/segurança |
| Perfis | Quatro cartões de ação com hierarquia de título, finalidade e descrição |
| Performance | Quatro partes numeradas, seletores alinhados, nome e ações em área própria |
| Teclado virtual | Painel de instrumento com título, orientação e teclado preservado |
| Sobre | Retrato com identificação do artista e painel de biografia/créditos mais amplo |

## Padrão comum

Cabeçalho e navegação fixos a 156 pixels em todas as páginas. Nome e assinatura
posicionados de maneira consistente; título da página e retorno Biblioteca
na mesma faixa. Rodapé com estado do áudio e texto de 20 pixels, conservando
24 pixels nos controles principais e 18 pixels nas orientações secundárias.

Os seletores continuam sendo comboboxes Win32 com a interação, teclado e
semântica nativos. Itens, seleção e seta recebem acabamento escuro, com borda
e foco explícitos. A navegação superior permanece disponível nos parâmetros.
As páginas extensas mantêm rolagem por setas, mouse e foco; apenas a biblioteca
exige todas as categorias visíveis simultaneamente.

O desenho foi separado em `src/ui_studio.inc`; os fluxos e a navegação ficam em
`src/ui_app.inc`. As fotografias existentes e os recursos não foram reenviados.
Motor de áudio, DSP, persistência e mapeamento MIDI não foram alterados.

## Validação efetiva

- Passaram os cinco grupos de `scripts/verify_local.sh`.
- Passaram os testes de estado da interface e de integração MIDI/Performance.
- A interface compila com MinGW x64, `-Wall -Wextra -Werror`.
- O teste nativo completo foi compilado e vinculado com as fotografias/créditos
  incorporados e PortAudio de teste. Inclui capturas futuras de teclado e dropdown,
  verificação do cabeçalho uniforme e do tamanho dos seletores.
- Foram revisados estudos estáticos da composição das oito páginas e da abertura,
  com as imagens reais e medidas equivalentes. Esses estudos usam substituição
  tipográfica e não são capturas da aplicação Windows.
- A execução nativa e a validação de hardware continuam pendentes. Foi tentada
  uma execução local com Wine, mas o ambiente recusou o socket necessário ao
  wineserver (`Operation not permitted`). Não foram contornadas essas permissões.
- Não foi gerado pacote portátil, instalador ou build hospedado.

## Uso do serviço

O conector continua sem ferramenta de cota autenticada ou faturamento; saldo
restante desconhecido. Publicação em um commit agrupado de arquivos de texto,
sem Actions, envio de imagens repetidas ou upload de samples.
