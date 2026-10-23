# Biblioteca em grade — 23 de outubro de 2026

## Comportamento final

A tela inicial mostra as 16 categorias em uma grade fixa de quatro colunas e
quatro linhas. Todos os cartões cabem na mesma área, inclusive na janela mínima
suportada de 1024 × 600. O tamanho dos cartões se adapta à altura disponível.
Em janelas maiores, ícones ficam acima dos nomes; em janelas compactas, ficam
ao lado dos nomes, preservando a fonte de 24 pixels.

- Clique único no cartão abre a categoria correspondente.
- Abrir uma categoria apenas monta a lista: não carrega nenhum instrumento.
- Clique, Enter ou setas cima/baixo na lista continuam carregando o timbre selecionado.
- Setas na tela inicial movem somente o foco entre os cartões; Enter abre o cartão.
- Tab também percorre os cartões e controles. ESC retorna à biblioteca.
- Não há carrossel nem troca automática de categoria pelas setas na lista de timbres.
- O teclado virtual continua disponível pelo botão “Teclado · K” ou atalho K.
- E, MIDI Learn, favoritos 1–20, Performance e ASIO mantêm seus fluxos existentes.

## Aparência

Ícones vetoriais próprios para as 16 famílias, desenhados com antialiasing,
traço e escala consistentes; cartões grafite, detalhes dourados, cantos suaves,
resposta ao ponteiro e foco visível. Cabeçalho compacto na biblioteca e navegação
fixa. O teclado virtual tem uma tela própria para dedicar toda a área inicial
às categorias.

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
