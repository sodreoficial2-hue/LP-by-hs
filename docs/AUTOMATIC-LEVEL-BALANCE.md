# Equilíbrio automático e permanente de volumes

Implementado sobre `28ad0f5cade138736be3a5312cc5479e13804d76`.

## Comportamento

Na primeira abertura, depois da descoberta do catálogo e de Meus Sons, o
programa mede os timbres SFZ e cada preset SF2 separadamente. A tela de abertura
mostra o número de timbres processados e continua respondendo enquanto um
worker usa sintetizadores privados, sem abrir dispositivo de áudio ou enviar
notas à entrada MIDI. A reconexão automática ASIO aguarda a conclusão.

Cada resultado é salvo imediatamente em
`%APPDATA%/Live Performance by Hugo Sodre/volume_calibration.cache`, com flush
e substituição atômica. O cache é independente dos perfis de show. Fechar o
programa, reiniciar o Windows ou desligar o computador não apaga os resultados.
Uma interrupção preserva os timbres cuja análise já foi concluída e gravada.

Nas aberturas seguintes, o programa reutiliza os resultados. Só sintetiza testes
para entradas novas ou para arquivos SF2/SFZ cujo tamanho/data de modificação
mudaram. A identidade inclui caminho normalizado, banco e programa SF2. Um
arquivo SF2 com vários presets possui compensações independentes. O algoritmo
usa uma referência absoluta fixa; adicionar outro instrumento não redefine
os ganhos anteriores.

Os timbres importados durante uma sessão entram nessa análise na próxima
abertura. A análise percorre um instrumento/banco por vez; não mantém o PCM
de toda a biblioteca em memória. Os presets de um mesmo SF2 reutilizam o
sintetizador de teste enquanto esse banco está sendo analisado.

## Medição e reprodução

O teste offline usa 48 kHz, duas intensidades MIDI (88 e 120), diferentes
registros e a nota de teste do catálogo. Contrabaixos e kits recebem sequências
apropriadas. Notas fortes/acordes adicionais em velocidade 127 limitam o ganho
calculado. Parâmetros pessoais, ADSR e efeitos do show não são alterados.

O medidor usa energia estéreo em janelas de 250 ms, um detector com remoção de
DC/graves muito baixos e o quartil superior das janelas não silenciosas. A
referência é -24 dBFS de RMS do detector. Trata-se de uma estimativa para
aproximar volumes, não medição certificada de loudness/LUFS. O ganho positivo
é limitado a aproximadamente 12 dB. Os picos observados na sequência de teste
limitam a compensação a uma referência de -9 dBFS, deixando margem.

Durante a reprodução, aplica-se apenas uma multiplicação fixa, igual nos dois
canais, por instrumento/parte e antes da soma de camadas. Nenhuma medição,
leitura de arquivo, espera ou alocação de calibração ocorre no callback ASIO.
Não há compressor, limitador ou buffer adicional. A dinâmica da execução,
os volumes manuais, o Master e os mixes salvos continuam funcionando.

## Falhas e limites

- Arquivos ausentes, inválidos ou que falham na medição mantêm o ganho original
  e geram aviso, sem impedir a abertura dos demais timbres. Serão tentados de
  novo na próxima abertura. Uma falha de gravação também é informada.
- Um teste sem sinal não recebe aumento de ganho. Esse resultado é salvo e
  identificado no aviso; pode indicar um instrumento com notas ou condições
  de disparo especiais que a sequência genérica não cobre.
- Um cache corrompido é preservado como `.invalid` e reconstruído. Se já existe
  esse backup, a falha é informada em vez de sobrescrevê-lo.
- O ganho acompanha reinícios/trocas do driver e as caudas de transição.
- Camadas, acordes fora da sequência de teste, efeitos e alterações pessoais
  de volume ainda podem produzir picos altos. O medidor de saída existente
  permanece ativo; o equilíbrio não constitui proteção absoluta contra clipping.
- Mover arquivos para outro caminho inicia uma calibração para essa identidade.
  A invalidação atual observa o arquivo SFZ/SF2 principal, não o conteúdo de
  samples/includes substituídos isoladamente sem modificar o SFZ principal.

## Verificação realizada

- 18 grupos CTest aprovados em Linux, com otimização e assertions habilitadas
  (`-O2 -UNDEBUG`), incluindo os novos testes e as regressões existentes de
  perfis, favoritos, MIDI, volumes, camadas, troca de áudio, glide e reverb.
- Cache: reabertura, adição incremental, alteração do arquivo, presets
  independentes, escrita com falha e recuperação de corrupção.
- Worker: medição sem interferir em um stream de teste já em execução,
  cancelamento com join e isolamento de falhas por instrumento.
- Reprodução: compensação estática, volume manual, camadas e reinício do driver.
- Áudio real FluidSynth 2.3.4, com SF2 sintético: diferença de 9,6 dB reduzida a
  0 dB no detector. Também verificado no áudio do AudioHost com FluidSynth real
  e PortAudio simulado; nenhuma validação de dispositivo físico é alegada.
- SFZ: integração de calibração executada com sfizz simulado; não foi obtida uma
  biblioteca Linux real de sfizz completa nesta etapa. O fluxo usa o suporte
  offline/freewheeling já existente no motor SFZ.
- Compilação cruzada Windows x64 dos executáveis Vision e clássico, usando
  PortAudio 19.7.0 e SDK ASIO real, e de ambos os harnesses Win32. Os harnesses
  agora aguardam a conclusão assíncrona da abertura.
- Sintaxe JavaScript e regressão da busca aprovadas, mantendo a correção anterior.
- Sem execução em Windows/WebView2, controlador MIDI físico, hardware ASIO ou
  banco completo instalado do usuário. Ainda requer validação nesse ambiente.
- Sem Actions, portátil, instalador, upload de binários ou redistribuição de
  bibliotecas/timbres. A cota autenticada restante do GitHub é desconhecida.
