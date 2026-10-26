# Live Performance — melhorias de uso e continuidade (26/10/2026)

Base: 73ae3a81a20a6f605ff859d843d5d79dc4fac599, incluindo Ctrl+D.

## Recursos

- O perfil compara o estado persistível com a última gravação. Selecionar notas,
  timbres e favoritos não gera falso aviso; parâmetros/MIDI Learn, Master,
  favoritos, mix de favorito e biblioteca de Performances geram alterações.
  Fechar ou abrir outro perfil oferece salvar, descartar ou cancelar. Salvar
  continua manual. Alterações válidas do editor entram na biblioteca em memória
  ao sair, permanecendo pendentes de Salvar perfil; rascunhos inválidos podem
  ser corrigidos ou descartados explicitamente.
- Ctrl+D mantém a confirmação e exclusão apenas do catálogo. Perfis oferece
  Restaurar timbres removidos: restaurar individualmente ou todos, com gravação
  atômica. Arquivos, favoritos e referências salvas permanecem intactos.
- Busca por nome nas categorias por Ctrl+F, sem carregar áudio ao digitar.
  A busca ignora caixa e acentos portugueses. No editor Vision há busca por
  parte; o fallback nativo oferece um filtro comum, preservando os instrumentos
  selecionados e seus IDs. Setas continuam selecionando/carregando resultados.
- Carregamento prepara uma nova geração fora do callback e a publica com
  ponteiro atômico. O ASIO não é fechado/reaberto por troca de som. Duas gerações
  antigas, no máximo, recebem note-off, sustain, rodas e controles, sem novos
  note-ons, com fade de até dois segundos. Splits/transposição antigos são
  preservados nos releases. O callback não aloca ou destrói motores; a thread
  da interface recolhe os motores retirados. F12/overflow remove todas as caudas.
  Underflow descarta caudas antigas para priorizar o instrumento atual.
  A preparação ainda é síncrona na thread de interface: bibliotecas muito
  grandes podem atrasar comandos de interface/MIDI WinMM durante essa preparação.
  Isso não é uma promessa de transição ilimitada ou de ausência de xruns em
  qualquer hardware; gerações simultâneas elevam uso de CPU/RAM.
- Medidor de pico estéreo após Master, com leitura em dBFS e aviso visual
  por dois segundos ao atingir 0 dBFS. Não foi adicionado limitador automático
  nem alterada a dinâmica aprovada; reduza os volumes ao receber o aviso.
- Perfil v9 guarda arquivos internos relativamente à pasta do aplicativo,
  e arquivos junto do perfil relativamente à pasta do perfil. IDs bank:// e
  índices/bancos/programas SF2 permanecem. Leitura v1–v8 mantida. Abra perfis v9
  nesta versão ou posterior. A opção Localizar timbres ausentes e o fluxo de
  abertura permitem escolher arquivos, manter ausentes ou cancelar. Um SF2
  substituto deve conter o mesmo banco/preset; cancelar preserva o show atual.
  A reparação cobre arquivos SFZ/SF2 raiz, não relocaliza samples internos do SFZ.

## Validação e limites

Os onze grupos existentes passaram. Os novos grupos cobrem referências relativas,
relocação, banco/programa SF2, proteção de estado, acentos/case, restauração
individual/todos e falhas de gravação. O teste de áudio contínuo cobre notas
antigas/novas, pedal, split transposto, transições rápidas e carga concorrente
com callback, sem reabrir o driver. A busca com acentos maiúsculos inicialmente
falhou e foi corrigida antes da repetição aprovada.

Verificações com dublês de PortAudio/sfizz não equivalem a hardware. AddressSanitizer
+ UndefinedBehaviorSanitizer passou no teste concorrente com detecção de vazamentos
desligada: LeakSanitizer não funciona neste ambiente de execução. JavaScript
passou na verificação de sintaxe. A instalação do Chromium falhou por download
incompleto/proxy; não há validação visual ou de renderer novo nesta etapa.
Os alvos Windows Vision e os dois harnesses nativos foram compilados; os harnesses
não foram executados. Windows/WebView2, diálogos reais, ASIO e controlador exigem
teste integrado no computador de Hugo.

Nenhum Actions foi iniciado. Não há ferramenta de saldo/quota autenticada do
GitHub; saldo desconhecido. A entrega é uma atualização de EXE + interface,
para extrair ao lado do executável existente, preservando DLLs e bancos.
