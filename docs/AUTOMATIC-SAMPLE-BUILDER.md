# Criador automático de timbres — 2 novembro 2026

Base: e40861452fd669fb660ffc2bf81dfc601e702e4b (VU de saída e piano inicial).

Em Perfis, “Criar timbre de áudio” abre um assistente nativo compartilhado pelas
interfaces Vision e clássica. Selecione um ou vários arquivos; a análise acontece
em uma tarefa separada, sem modificar o callback de áudio, ASIO ou MIDI. Crie um
SF2 e escolha onde salvá-lo; o preset é registrado permanentemente em Meus Sons.
O ZIP é uma atualização do portátil existente, não uma distribuição completa.

## Análise automática e edição

- O sndfile.dll já presente no portátil decodifica WAV, FLAC, OGG e AIFF mono ou
  estéreo. “Todos os arquivos” permite tentar outros formatos que esse runtime
  suporte; MP3 depende da versão. WavPack (.wv) não é prometido. WAV é recomendado.
- Pausas de pelo menos 180 ms separam notas em uma gravação. Trechos com menos de
  40 ms são ignorados. Não separam acordes, instrumentos misturados ou notas
  sobrepostas; use gravações isoladas de um mesmo instrumento, com ganho constante.
- Nomes como Piano_C4_v40.wav e Piano_C4_v110.wav fornecem nota e intensidade.
  C4 corresponde a MIDI 60; note60_vel40.wav também funciona. Metadados do nome
  prevalecem; uma nota no nome vale para todos os trechos desse arquivo.
- Sem nota no nome, YIN estima a fundamental e o desvio de afinação. Áudio estéreo
  usa o canal mais forte para análise, evitando cancelamento por oposição de fase.
  Notas não identificadas aparecem com “?” e impedem a exportação até correção.
- Amostras da mesma nota são agrupadas. Sem intensidade explícita, a energia dos
  primeiros 300 ms ordena as camadas e distribui seus centros entre 1 e 127.
  Os limites são os pontos médios entre centros, sem lacunas ou sobreposições.
  Valores explícitos em nomes têm prioridade; centros duplicados exigem edição.
- Isso estima a ordem das intensidades: áudio não revela a velocidade MIDI real
  da gravação. Ganho, compressão, ruído e alterações de timbre podem afetar a
  classificação. A interface identifica valores estimados.
- As regiões de teclas usam as notas vizinhas mais próximas. Transposição muito
  distante de uma única amostra pode soar artificial; mais notas melhoram a fidelidade.
- Loops automáticos só são habilitados para trechos estáveis cuja energia e
  correspondência das bordas atendam aos critérios. Sons com decaimento são
  preservados. O editor pode desativar/reutilizar um loop encontrado, corrigir
  nota e intensidade, e recalcular o mapa antes de exportar.
- Um ganho comum apenas reduz picos que ultrapassariam o PCM. Não se normaliza
  cada camada separadamente, preservando diferenças de volume e timbre.

## Exportação e segurança dos dados

O escritor cria SF2.01 PCM de 16 bits com regiões de nota/velocidade, afinação,
envelope de saída e loops. Estéreo mantém amostras L/R vinculadas e suas zonas.
Gravação atômica substitui o destino apenas depois de concluir e sincronizar o
arquivo temporário. O diálogo pede confirmação para substituir um SF2 existente;
outras extensões são rejeitadas. Os áudios originais nunca são modificados.
Cancelar interrompe a análise entre arquivos/blocos/notas e impede a publicação
antes da escrita. Uma escrita atômica já iniciada termina com o arquivo íntegro.
Falhas de análise mantêm o último projeto utilizável; falhas de exportação
devolvem o projeto para correção. Projeto de edição permanece disponível enquanto
o aplicativo está aberto; o SF2 e seu registro em Meus Sons ficam persistentes.

Limites: 256 arquivos, 512 notas, 120 s por arquivo, 8–192 kHz, 1–2 canais,
32 milhões de amostras PCM por arquivo e 64 milhões no conjunto. Processamento
pesado deve ser feito na preparação do repertório, não durante um show.

## Validação

- 28 grupos CTest passaram: novo decodificador/segmentador, notas graves/agudas,
  estéreo em oposição de fase, camadas, silêncio, cancelamento, arquivos inválidos,
  loop/decay e as regressões existentes de perfis, MIDI, transições e catálogo.
- Um teste novo com FluidSynth 2.3.4 real carrega o SF2 produzido e mede diferenças
  espectrais nas velocidades 20/110, transposição, sustentação e pânico.
  O primeiro teste detectou uma exigência de tamanho par no INAM; o escritor foi
  corrigido e os testes de exportação/renderização passaram depois disso.
- O teste opcional preexistente real_sf2_glide apresentou falhas intermitentes
  de processo em cenários diferentes com a biblioteca Linux disponível. Um cenário
  isolado passou; a suíte de glide foi excluída da contagem de 28 testes. Nenhum
  arquivo do motor SF2/glide foi modificado por esta atualização.
- Testes DOM existentes de catálogo, Master/Phaser, VU, Performance, busca e
  estabilidade passaram. O botão novo foi acionado no renderer de produção e
  seu comando validado no protocolo. Playwright não executou, pois não há
  Chromium instalado. Não se afirma verificação visual em navegador.
- Vision e clássico Windows x64 foram compilados localmente com PortAudio
  v19.7.0, ASIO SDK 2.3.3 e o WebView2 SDK fixado. O harness Windows foi compilado.
  Não há execução do assistente em Windows, teste ASIO físico, leitor de tela
  ou validação das gravações reais de Hugo neste ambiente.

O saldo autenticado de GitHub é desconhecido: não há ferramenta de quota ou
faturamento exposta. Sem Actions, builds hospedados ou upload de binários no
GitHub; publicar uma única alteração agrupada de código na conta atual
sodreoficial2-hue/LP-by-hs, sem alterar a versão estável anterior.
