# Biblioteca em grade — 23 de outubro de 2026

## Comportamento final

A tela inicial mostra as 16 categorias em uma grade fixa de quatro colunas e
quatro linhas. Todos os cartões cabem na mesma área, inclusive na janela mínima
suportada de 1024 × 600. O tamanho dos cartões se adapta à altura disponível.
As fotografias ficam acima dos nomes, preservando a fonte de 24 pixels e
reservando toda a largura do cartão para a legenda em uma faixa escura própria.

- Clique único no cartão abre a categoria correspondente.
- Abrir uma categoria apenas monta a lista: não carrega nenhum instrumento.
- Clique, Enter ou setas cima/baixo na lista continuam carregando o timbre selecionado.
- Setas na tela inicial movem somente o foco entre os cartões; Enter abre o cartão.
- Tab também percorre os cartões e controles. ESC retorna à biblioteca.
- Não há carrossel nem troca automática de categoria pelas setas na lista de timbres.
- O teclado virtual continua disponível pelo botão “Teclado · K” ou atalho K.
- E, MIDI Learn, favoritos 1–20, Performance e ASIO mantêm seus fluxos existentes.

## Aparência

Fotografias reais para as famílias de instrumentos, com proporções preservadas e
margens consistentes, sem esticar nem cortar instrumentos. Os 16 cartões usam
13 fotografias: Performance, Meus Sons e Favoritos reutilizam imagens de
instrumentos com marcas funcionais discretas. A fotografia também aparece na
tela interna da categoria.

Cartões grafite, detalhes dourados, nomes em alto contraste, cantos suaves,
resposta ao ponteiro e foco visível. As fotos são armazenadas em um único atlas
JPEG de aproximadamente 666 KiB, incorporado no recurso 4 do executável. O
atlas é decodificado uma vez; desenhar cartões não abre arquivos nem usa rede.

Créditos e licenças individuais estão em `assets/category-photo-credits.txt`,
incorporados no recurso 5 e apresentados na tela Sobre. A proveniência e os
hashes estão em `assets/category-photos.json`. Foram selecionadas fotografias
da Wikimedia Commons; os autores e as licenças específicos constam no registro.
Não foram usadas imagens geradas por IA.

## Resposta da navegação

O fluxo anterior chamava `audition()` ou `activate()` ao abrir uma categoria.
Essas chamadas podem carregar samples de forma síncrona na thread da interface.
Foram retiradas do fluxo de abertura. Agora clicar em categorias não altera o
instrumento que está tocando, nem inicia leitura de samples. Não foi reproduzido
o travamento relatado na máquina do usuário: é uma correção de um bloqueio
potencial identificado no código, pendente de confirmação no Windows.

## Validação

- Passaram os cinco grupos de `scripts/verify_local.sh`: perfis, motor/MIDI,
  DSP, host ASIO com dublê de PortAudio e catálogo.
- Passaram os testes locais de estado da interface, incluindo limites,
  ausência de sobreposição dos 16 cartões em cinco tamanhos e foco espacial.
- Passou o teste local de integração MIDI/Performance.
- A interface e o teste nativo compilaram com MinGW x64,
  `-Wall -Wextra -Werror`.
- Teste nativo ampliado para clique único, notificações sem clique, navegação sem
  carga de áudio, teclado virtual, lista e todos os cartões visíveis em 1024 × 600.
- Execução do teste nativo, capturas da janela e validação de hardware pendentes.
- Nenhum portátil, instalador, workflow ou build hospedado gerado nesta etapa.

## Uso remoto

Não há ferramenta de cota autenticada ou faturamento no conector disponível.
Saldo desconhecido. Publicação agrupada em uma árvore de texto, um commit e uma
atualização de referência, sem Actions e sem upload de samples.

## Verificação da integração fotográfica

A unidade da interface compila com MinGW x64 e avisos como erros. O teste
nativo completo foi compilado e vinculado localmente, com os recursos de
fotografias e créditos incorporados e PortAudio de teste. O teste nativo foi
ampliado para verificar a presença do atlas de 2048 × 1536 e dos créditos.
A execução Windows e a inspeção visual da janela continuam pendentes. Foram
inspecionadas visualmente as fotografias selecionadas e a composição do atlas;
essa inspeção não equivale a um teste da janela Windows.

O envio remoto desta etapa inclui um único blob binário novo para o atlas,
agrupado com fonte, créditos e manifesto em um commit. Sem uploads repetidos de
fotografias, sem samples e sem build hospedado.

## Revisão posterior de todas as telas

O cabeçalho e o rodapé foram unificados; a abertura e todas as páginas internas
receberam revisão de composição, controles e hierarquia. Consulte
`docs/STUDIO-REFINEMENT.md` para o escopo final e a validação desta etapa.
