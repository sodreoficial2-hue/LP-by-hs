VISION SFZ NATIVE 0.8 - PROTOTIPO DE TESTE (WINDOWS x64)

APLICATIVO NOVO:
- Interface Win32 nativa, sem Electron, JavaScript ou sintetizador SF3.
- Motor sfizz carregado por DLL ao lado do executável; PortAudio com ASIO e WASAPI.
- 3 bibliotecas SFZ de teste de Greg Sullivan (licença CC BY 3.0, arquivo LICENSE das amostras).
- Seletor de driver ASIO / WASAPI, taxa fixa inicial 48000 Hz, buffer configurável.
- Entrada MIDI WinMM; favoritos, MIDI Learn, 14 controles e perfis de show locais.
- Teclas: ESC retorna as categorias; ESPACO marca favorito; E foca painel.
- Se o Windows nao identificar sua placa/driver, a interface deve informar a falha, sem fallback.
- Aviso: o painel da versao 0.8 e um prototipo. Os controles de envelope (Attack,
  Decay, Sustain, Release) sao persistidos, mas ainda nao modulam diretamente cada
  regiao SFZ. O prototipo 0.8 restaura o mesmo driver ao trocar timbre.
O botao TESTAR SOM gera uma nota C4 sem teclado MIDI, permitindo verificar a saida.
O indicador MIDI RECEBIDO confirma a chegada de notas do controlador.
A certificacao para palco exige testes com o usuario e drivers reais.

PERFIS:
%APPDATA%\Vision SFZ Native\settings.ini
Perfis de show: pasta Shows (extensao .vshow.ini).
DIAGNOSTICO:
%APPDATA%\Vision SFZ Native\diagnostico.txt
Relatorios nao sao enviados automaticamente nesta versao. Nao cole tokens
do GitHub na aplicacao nem envie dados sensiveis.

LIMITACOES:
- O carregamento SFZ interrompe brevemente o stream para manter seguranca de threads.
- Nem todos os comandos de MIDI Learn e efeitos foram homologados em hardware real.
- Apenas Windows x64; 3 bibliotecas demonstrativas, sem promessa de cobertura profissional completa.

CORRECAO 0.8:
- O TESTAR SOM C4 abre uma janela explicando por que nao pode executar, mesmo se o SFZ nao estiver carregado.
- A selecao visual da lista e promovida automaticamente a instrumento carregado antes do teste.
- Falhas no carregamento SFZ (arquivo ausente, amostras/regioes vazias, erro de sfizz) mostram janela grande e detalhada.
- O programa nunca confunde uma linha destacada com um instrumento pronto.
- O tom de 440Hz foi comprovado no ASIO pelo usuario, mas as amostras SFZ ainda exigem validacao no Windows dele.

VERSAO 0.8 - SEGURANCA DA TROCA DE INSTRUMENTOS:
- O novo SFZ e validado num sintetizador temporario; se falhar, o timbre anterior e o fluxo ASIO continuam funcionando.
- Somente apos validar amostras e regioes o aplicativo interrompe o fluxo, troca o sintetizador e reabre o mesmo driver.
- Se o driver nao reiniciar, o erro e exibido diretamente na janela. Nenhum fallback DirectSound e autorizado.
- Teste de regressao inclui SFZ ausente, arquivo invalido, preservacao das amostras e retorno do audio SFZ em C4.
- Compilacao automatizada nao substitui validacao no hardware real de ASIO4ALL / TASCAM.

VERSAO 0.8 - REPARO DOS CAMINHOS SFZ:
- A codificacao UTF-8 e ativada pelo manifesto do Windows 10/11.
- O proprio aplicativo passou por testes de CP80, Wurlitzer EP200 e Pianet T em pasta de instalacao contendo acentos e caracteres Unicode.
- O motor continua SFZ exclusivamente, usando ASIO ou WASAPI; nenhum fallback para DirectSound.
- Se tocar o TOM 440Hz e nao o C4, verificar o diagnostico, em particular o caminho SFZ e o numero de regioes.
- Ainda requer teste manual com ASIO4ALL e A-PRO no computador do usuario.

ETAPA PALCO 0.8 - CONTROLES PARA TESTES EM HARDWARE:
- 14 controles: Volume individual, ADSR por voz, filtro, reverb, delay, chorus,
  tremolo, compressor e equalizacao de grave, medio e agudo.
- O ADSR aplica Attack/Decay/Sustain/Release separadamente a cada nova nota MIDI.
- Mudar o ADSR pela primeira vez prepara a versao SFZ de envelopes em memoria;
  o audio ASIO deve voltar automaticamente apos essa preparacao.
- Depois disso, os 4 knobs modificam o envelope das proximas notas sem
  desmontar o driver a cada movimento. SFZ originais permanecem intactos.
- SFZ com #include ainda nao aceitam ADSR externo; a interface informa o erro.
- MIDI Learn por controle e botoes de favorito; botoes reservados nao geram nota.
- Favoritos e perfis salvos localmente e recuperados ao abrir o aplicativo.
- F1 mostra atalhos de acessibilidade e valores dos controles aparecem em texto.
- Driver permitido somente ASIO ou WASAPI; nunca DirectSound/MME.
- Esta e uma versao experimental. Testes automatizados nao substituem o teste
  fisico com ASIO4ALL e controlador MIDI, sobretudo latencia e sessoes longas.
- Configuracoes pessoais: %APPDATA%\Vision SFZ Native\ (nao apagadas pelo instalador).
