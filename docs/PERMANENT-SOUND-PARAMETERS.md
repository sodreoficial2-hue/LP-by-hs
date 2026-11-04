# Memória permanente dos parâmetros dos timbres — 4 de novembro de 2026

Hugo autorizou a integração do PR #1 à `main` e solicitou que qualquer ajuste de
um timbre fosse automaticamente memorizado, até uma nova alteração, sem salvar
perfil e conservando os valores depois de fechar e reabrir.

## Comportamento implementado

- Volume, Attack, Decay, Sustain, Release, Envelope Filter, Reverb, Delay, Chorus,
  Tremolo, Compressor, Bass, Mid, Treble, Portamento e Phaser: todos os 16 controles,
  incluindo ativação do ADSR e MIDI Learn dos knobs, ficam na memória do timbre.
- O mesmo instrumento compartilha seus últimos ajustes entre sua categoria,
  Meus Sons e qualquer slot dos Favoritos, independentemente do perfil aberto.
  Remover um favorito, mover de categoria ou renomear a apresentação no catálogo
  não apaga os ajustes do instrumento.
- SF2 distingue arquivo, banco e preset. Alterar um piano não altera os outros
  instrumentos do mesmo SF2. SFZ e os instrumentos convertidos usam sua identidade
  original de catálogo. Caminhos internos ao aplicativo são relativos para
  conservar a identidade ao mover a pasta portátil.
- Nas Performances, cada camada tem uma memória própria dentro daquela montagem.
  Dois pianos na mesma Performance podem ter valores diferentes. Esses ajustes
  não modificam o piano avulso nem outra Performance. Mixes fora dos Favoritos
  também ficam permanentes, substituindo a antiga regra de mix temporário.
- O Master e seu controle MIDI são memorizados globalmente. O ganho recuperado
  também é aplicado ao motor na inicialização, antes da preparação do piano.
- Mouse, controles nativos, comandos Vision, teclado e MIDI seguem os mesmos pontos
  de atualização. Não há botão novo nem necessidade de Ctrl+S para os parâmetros.
- Abrir um perfil antigo preserva os últimos parâmetros memorizados. Perfis ainda
  guardam o repertório, a distribuição dos Favoritos e seus Program Changes;
  ajustar somente knobs/Master não provoca a pergunta para salvar perfil.
- A criação e a estrutura de novas Performances continuam sob o comando Salvar
  Performance. O rascunho do editor permanece automático; parâmetros de montagens
  já salvas têm memória independente do arquivo de perfil.

## Armazenamento e encerramento

O arquivo é `%APPDATA%/Live Performance by Hugo Sodre/sound-parameters.settings`.
O formato versionado `LPHS_SOUND_PARAMETERS 1` contém os controles por identidade,
seus mapeamentos e o Master, sem copiar Program Changes dos slots.

A alteração fica imediatamente na memória de execução. Um timer de 100 ms
encaminha o último snapshot a `BackgroundStore`; a fila substitui snapshots
pendentes pelo mais recente. O callback de áudio e o callback MIDI não gravam
arquivos, não fazem leituras de disco e não criam buffers de áudio adicionais.

O fechamento esvazia a fila e verifica falhas antes de prosseguir; a destruição do
App também drena alterações pendentes. A gravação usa o mecanismo atômico existente,
com flush e substituição do arquivo, além de uma cópia anterior `.backup`.
Arquivo corrompido é preservado para recuperação. Sem uma memória válida, o programa
informa a falha e não substitui dados ilegíveis silenciosamente por valores padrão.

A prioridade de leitura é: memória permanente, quando existente, seguida dos
valores do perfil/Performance ou defaults do instrumento. Somente os controles
são copiados: identidade, categoria, nome de apresentação e botão Program Change
continuam independentes.

## Validação

- `permanent_sound_parameters`: PASSOU em Linux com assertions ativas. Abrange os
  16 parâmetros, ADSR, MIDI Learn, último valor após 101 alterações assíncronas,
  reabertura sem perfil, sobrescrita, prioridade sobre perfil antigo, PC do slot
  preservado, presets SF2 separados, isolamento das partes de Performance, Master,
  recuperação de corrupção e falha de gravação preservando o arquivo anterior.
- CMake/CTest completo: **37 aprovados, 1 falha legada, 4 skips, 42 registrados**.
  A falha continua `real_sfizz_envelope / Real OSC ADSR readback`; os quatro skips
  continuam fixtures pessoais NPNO/NSMP ausentes. Não são regressões atribuídas à
  memória de parâmetros. [Log completo](parameter-memory-validation/ctest-full.txt).
- `win32_parameter_memory_test`: usa o App nativo real sem host de áudio, altera
  parâmetros por callback de mouse/MIDI, destrói o App sem salvar perfil e cria
  outro App para conferir restauração e sobrescrita. Compilação Windows conferida;
  execução não concluída neste ambiente: `wineserver: socket: Operation not permitted`.
  [Diagnóstico](parameter-memory-validation/wine-runtime.txt).
- Os harnesses Win32/Vision e os executáveis de produção foram compilados com
  MinGW x64, PortAudio/ASIO, zlib e Expat reais. A execução dos controles no Windows
  do usuário e a confirmação com hardware continuam pendentes.
- `node --check vision-ui/vision.js` e `git diff --check`: PASSOU.

Reprodução nativa:

```sh
cmake -S . -B build-parameters -DLP_TEST_ONLY=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-parameters --target lp_sound_parameters_test --parallel 4
ctest --test-dir build-parameters -R '^permanent_sound_parameters$' --output-on-failure
```

Para a suíte completa com os mesmos gates anteriores, configurar também
`LP_SNDFILE_LIBRARY` e `LP_REAL_SFIZZ_LIBRARY` conforme o relatório de formatos.
No Windows, compilar `lp_win32_parameter_memory_test`, os harnesses e os targets
`LivePerformance`/`LivePerformanceVision` com a toolchain e SDKs documentados.

Motores, DSP, ASIO, formatos de perfil e reprodução SF2/SFZ preservados. Não foi
publicado ZIP de atualização nem usado GitHub Actions. A cota autenticada GitHub
permanece desconhecida, pois não há ferramenta de consulta autenticada exposta.
