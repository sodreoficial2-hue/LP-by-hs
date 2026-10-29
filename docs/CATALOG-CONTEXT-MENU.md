# Menu de timbres — 29/10/2026

Base: b8fd6ac4583a33ef8a5f3eb02fad401e1632cdf4, incluindo calibração de volumes.

Botão direito em um timbre de instrumento abre Renomear, Mover (submenu com categorias),
Ocultar e Deletar permanentemente. Shift+F10/tecla de menu também abre as opções.
Vision encaminha a solicitação ao mesmo menu nativo da interface clássica; não
carrega o timbre ao abrir o menu. O clique direito nos favoritos mantém MIDI Learn
por Program Change. Performances compostas continuam sendo alteradas no editor.

Renomear muda somente o nome exibido, inclusive favoritos e listas do editor.
Mover muda a categoria do catálogo sem mover arquivos nem mudar referências.
Os dois são gravados automaticamente em catalog_edits.settings, ao lado das
preferências permanentes de áudio, independentemente de salvar um perfil.
Os nomes originais, IDs, parâmetros e compensações de volume são preservados.
Meus Sons mantém a identidade de importação mesmo depois de mudar de categoria.

Ocultar mantém o comportamento reversível do Ctrl+D, inclusive a restauração
individual ou de todos em Perfis. A opção do menu também usa a mesma lista
permanente hidden_sounds.catalog.

Deletar exige confirmação, com o nome e o caminho exatos capturados antes do
loop modal. A confirmação avisa que Favoritos/Performances podem ficar sem o
som e que a reprodução atual será interrompida. ASIO é parado, os sintetizadores
são descarregados, o arquivo é removido e ASIO reconecta em modo automático.
O arquivo removido não é recuperável pela lista de ocultos.

Limites deliberados: SF2 só é apagado se a enumeração real do banco confirmar
um único preset. Bancos com vários presets são bloqueados, sem apagar outros
sons. Não há reescrita destrutiva de SoundFonts. SFZ remove sua definição, mas
preserva as amostras de áudio, que podem ser compartilhadas. Referências por
#include e entradas duplicadas são verificadas antes da exclusão. Inclusões
dinâmicas que não possam ser verificadas bloqueiam a exclusão; links simbólicos
também são recusados. A checagem de definições é feita fora do callback de áudio.
Um registro de exclusão durável permite ignorar entradas SFZ apagadas no catálogo
original ao reiniciar, inclusive se o último timbre tiver sido removido. Um
arquivo recolocado passa novamente pela descoberta normal. Nenhuma exclusão
real foi feita nos timbres do usuário durante o desenvolvimento.

Validação: 16 grupos CTest passaram em Debug com asserts ativos; incluem o novo
regresso de persistência/renomeação/categoria, identidade portátil, ocultação e
restauração, exclusão real em pasta temporária, referências SFZ compartilhadas,
reinício com catálogo vazio após exclusão e falhas de gravação. Sintaxe JavaScript,
preservação do cursor de busca e teste do renderizador real com DOM simulado
(menu, teclado, favorito MIDI Learn e restauração) passaram. As duas unidades de
interface Windows (clássica e Vision) e os recursos/diálogo compilaram em objetos
x64 com MinGW/WebView2. Não houve link de um novo executável de produção nem
execução em Windows/WebView2/ASIO ou teste de hardware. O script antigo
verify_local.sh usa -Werror e para em avisos de indentação preexistentes em
sf2_engine.cpp e dsp.cpp; a suíte CMake completa passou sem alterar esses arquivos.

GitHub não expõe ferramenta de quota autenticada nesta sessão; saldo restante
continua desconhecido. Publicação agrupada de fontes, sem Actions ou binários.
