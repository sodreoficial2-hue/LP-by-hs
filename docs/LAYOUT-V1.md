# Live Performance 1.0 — layout acessível

Implementação sobre `feature/sf2-engine` (70e7cbc), incorporando a negociação de sample rate da principal (c7db7b2). O código desta etapa está na ramificação `layout/accessible-v1`. Não foi gerado portátil, instalador ou workflow nesta etapa.

## Telas e acesso

- Abertura: retrato incorporado, Live Performance branco, assinatura dourada, Versão 1.0 e progresso baseado nas etapas de catálogo e nos bancos SF2 encontrados. O tempo restante é uma estimativa. Os SF2 são processados individualmente entre mensagens de janela. A barra representa a preparação da biblioteca; não promete pré-carregar todas as amostras na RAM.
- Início: retrato circular, marca, quatro botões superiores, categoria central grande com desenho vetorial e setas, teclado virtual com clique/arrasto e encerramento da nota ao perder captura/foco.
- Categorias: esquerda/direita e Enter. As 15 categorias existentes são preservadas; Favoritos é a 16ª opção.
- Timbres: desenho/nome à esquerda e lista grande à direita; setas verticais selecionam o som. Estrelas podem ser alternadas com Espaço ou clique. Favoritos preservam slots e Program Changes 1–20 (wire 0–19).
- Performance Editor: P; quatro seletores, modos Camadas/Dividir, limpar parte, nome, ativação e salvamento. Em Dividir, a primeira região termina em Si2 (MIDI 47; Dó central = MIDI 60), e a próxima começa em Dó3 (48). Limites superiores das regiões seguintes foram mantidos, ajustando o início da região adjacente para não deixar lacuna/sobreposição.
- Áudio/MIDI: A; seleção ASIO, buffer, ASIO Control, entrada MIDI e pânico. A taxa efetiva do driver aparece em Hz.
- Configurações: C; abrir/salvar perfil, importar SFZ/SF2 e pânico.
- Sobre: botão pelo mouse, sem atalho ou parada de TAB. Foto e trajetória com texto grande e rolagem.
- ESC volta ao início nas telas normais. F12 mantém pânico global. Campos de texto recebem P/C/E normalmente durante a digitação.

## Painel E e estado musical

E abre/fecha o painel. Durante uma Performance ativa, aparecem somente quatro volumes de camada, com MIDI Learn independente. Nos demais sons, aparecem os 14 parâmetros anteriores, Portamento e Master. O painel fecha para o início após 30 segundos sem interação de mouse/teclado ou CC mapeado. ESC apenas cancela o aprendizado enquanto o painel está aberto; não fecha o painel. Os botões de navegação ficam ocultos durante o painel para respeitar a saída exclusiva por E ou inatividade.

Volumes de Performance são preservados em memória na troca e gravados no perfil mediante Salvar. Master é ganho global após a soma, com CC próprio e estado no perfil. Atribuir um CC ao Master remove vínculos conflitantes; reaprender outro controle permite reutilizá-lo.

Portamento positivo habilita execução monofônica com prioridade da última nota e retorno à nota anterior ainda pressionada. O tempo varia de 5 ms a 2 s, com curva quadrática. A afinação desliza em blocos de até 32 amostras, pelo pitch wheel nativo de cada sintetizador, separado do efeito do pitch bend físico. Zero preserva a polifonia original. SFZ restaura a extensão de bend original ao desativar; SF2 retorna a ±2 semitons. A extensão interna de glide é ±96 semitons.

Perfil v7 adiciona portamento e Master, lendo perfis v1–v6 com valores neutros para os novos controles. Meus Sons permanece no catálogo próprio já existente. Configurações ASIO/MIDI mantêm a persistência anterior.

## Áudio e recursos

A negociação ASIO prepara SFZ, SF2 e todas as partes de Performance na taxa selecionada. Evita tentar abrir um arquivo SF2 no sfizz durante reconexão. Ganhos de partes continuam válidos após reiniciar ASIO. Não há fallback para áudio Windows.

A foto enviada em 23/10/2026 (`1000348604.jpg`) é idêntica, byte a byte (SHA-256 `d3cceb963063d4a21c792d202dc3e7340994a97e206ac9c709d7df19a509b9c1`), ao retrato de `recovery/extracted/live/assets/hugo.jpg` já incorporado. O projeto usa portanto exatamente a foto fornecida, sem duplicar recursos. Não houve alteração de fisionomia. O recurso JPEG é incorporado ao executável e compartilhado entre abertura, topo e Sobre. O ICO contém recortes do rosto em 16–256 px. Segoe UI e Segoe Script são fontes do sistema Windows.

## Validação e limites

- Sete suítes CTest passam: sessão, ABI/MIDI do sfizz, DSP, host de áudio, catálogo, Performance/MIDI e layout/glide/perfil.
- O host foi testado com PortAudio/sfizz simulados, incluindo Master após soma de quatro partes, troca de Performance para timbre comum, 44,1 kHz e retenção do volume individual na troca de taxa.
- Interface e teste nativo Win32 compilam com MinGW x64, `-Wall -Wextra -Werror`; o recurso de foto/ícone também foi compilado.
- `tests/win32_layout_test.cpp` é um teste de integração adicional que pode capturar telas em Windows, usando PortAudio simulado e runtime sfizz disponível junto ao executável. Foi compilado e vinculado; sua execução gráfica não foi validada neste ambiente. Wine não conseguiu iniciar seu servidor local aqui. Nenhuma captura resultante foi apresentada como validação.
- Não houve teste novo de hardware Windows/ASIO nem audição de portamento em teclado real. Isso fica para a próxima versão de teste solicitada pelo usuário.
- O conector não expõe saldo de cota autenticada/billing; saldo restante desconhecido. Nenhuma execução remota foi acionada. A nova ramificação não corresponde aos gatilhos dos workflows existentes.

## Complemento: seleção automática e rolagem por setas

- A janela agora admite 1024 × 600 px; em larguras menores, os quatro botões superiores ocupam uma segunda linha. O cabeçalho e o rodapé permanecem fixos, e uma área de conteúdo recorta os controles para impedir sobreposição.
- Páginas com funções que excedem a altura disponível têm barra vertical sincronizada às setas ↑/↓, Page Up/Page Down e roda do mouse. TAB revela o controle em foco, sem perder o foco durante a rolagem.
- Listas de timbres/favoritos e caixas de seleção mantêm as setas nativas para mudar itens. Listas e texto Sobre se ajustam à altura visível e usam a própria rolagem interna. Os controles de parâmetros preservam suas setas para edição de valores.
- Ao abrir uma categoria, o primeiro timbre destacado já é carregado. Um clique sobre um item, inclusive o que já estava destacado, carrega o som sem Enter; mudanças de seleção por setas também carregam automaticamente. A seleção atual já carregada é reutilizada, preservando ajustes e evitando reinícios de áudio duplicados. A regra inclui Favoritos e Performance.
- A foto recebida foi conferida localmente contra o recurso já existente e corresponde ao retrato da abertura, topo, Sobre e ícone. Nenhum tratamento de fisionomia foi aplicado.
- Testes portáveis passaram para limites de rolagem, redimensionamento, exibição do foco e os sete grupos de regressão existentes. A interface e o teste de integração Win32 foram compilados/vinculados com MinGW x64 e avisos tratados como erro. Os cenários nativos adicionais verificam clique, setas, caixas de seleção e página de 600 px; sua execução gráfica no Windows continua pendente.
- Nenhum portátil ou workflow remoto foi gerado. O saldo autenticado de GitHub continua desconhecido, pois não há ferramenta de consulta disponível.

## Nomes de timbres na interface

O repositório possui o catálogo dos instrumentos incluídos em `banks/curated/catalog.txt`. Os bancos SF2 externos (inclusive a pasta Red Pianos do usuário) são descobertos no computador durante a abertura; os respectivos arquivos comprados não foram publicados no repositório.

A lista de categorias, os seletores de Performance e os Favoritos usam nomes de apresentação: retiram o prefixo Red, códigos conhecidos de categoria/número de pacote, a combinação repetida nome de arquivo/preset e a indicação técnica `(banco:programa)` das importações antigas. Repetições adjacentes do mesmo nome também são removidas. Modelos e variantes diferentes, como DX7 EP 1 e DX7 EP 2, são preservados. Nomes criados pelo usuário para Performances permanecem intactos.

A alteração é somente de apresentação; nomes de arquivo, referências SFZ/SF2, presets, categorias, Program Changes e dados dos perfis não são renomeados. Não é necessário modificar a pasta de timbres do computador. O tratamento também se aplica ao catálogo Meus Sons já salvo por versões anteriores. Os testes incluem nomes em português, repetições, variantes, favoritos SF2 e preservação da referência original. Sete suítes locais passaram e a interface compilou para Windows com avisos tratados como erro. Nenhum portátil ou processamento remoto foi acionado; a cota autenticada continua desconhecida.
