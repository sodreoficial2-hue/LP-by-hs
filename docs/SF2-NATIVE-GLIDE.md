# Portamento SF2 nativo — investigação e correção

## Falha reproduzida

O usuário informou que o knob Portamento ainda não produzia a transição suave esperada na última compilação com visual Vision. A base investigada é `28005ed58306550afaaa4f9ea3dd2b56b7380d08`, que preserva as quatro correções anteriores.

O host usava `MonoGlide` para emitir uma rampa de pitch wheel com escala de ±96 semitons. Entretanto, `fluid_synth_pitch_wheel_sens` exige 0–72 semitons no código oficial do FluidSynth 2.6.1. Pedir 96 retorna -1; o retorno era ignorado. A faixa real continuava ±2 semitons, mas os valores enviados estavam calculados para ±96. A nota seguinte começava praticamente no destino, em vez de partir da altura anterior.

A confirmação não foi só por inspeção: a API real retornou -1 para 96 e 0 para 72. O áudio do código anterior falhou no teste de trajetória. Em A4 → A5, knob 90%, 243,15 ms após o segundo Note On, esperávamos aproximadamente 488,21 Hz: antes medimos 869,07 Hz; com a correção medimos 487,68 Hz. A diferença residual é compatível com a janela de medição e blocos internos do sintetizador.

## Solução

O SF2 passa a usar o portamento nativo do FluidSynth, separado do pitch wheel:

- Knob zero: polifonia normal. Acima de zero: mono de prioridade da última nota com reutilização de vozes/legato nativo, incluindo retorno à nota ainda pressionada.
- Tempo: `round(5 + knob² × 1995)` milissegundos, portanto aproximadamente 5–2000 ms. Em 50%, cerca de 504 ms; em 90%, 1621 ms.
- CC65 interno habilita portamento; CC5/37 internos transportam o tempo de 14 bits em milissegundos. O modo é explicitamente cada nota, permitindo legato e staccato. Não se cria uma oscilação ou vibrato artificial para simular glide.
- A partir de FluidSynth 2.5, o modo de tempo é explicitamente linear quando a função correspondente está disponível. Versões anteriores continuam usando CC5/37 em milissegundos. A chamada opcional não impede carregar 2.3.4.
- O próprio DSP das vozes calcula a transição, inclusive mantendo a continuidade quando uma terceira nota chega antes do fim da rampa. Não há rampa de pitch bend compartilhada entre caudas.
- Pitch wheel permanece independente, com faixa inicial normal ±2 semitons. A fixture também inclui um SF2 com modulação de pitch wheel anulada; seu portamento nativo continua funcionando.
- O controle de tempo/mono é do knob do host. CC5/37 externos não sobrepõem o tempo enquanto o glide está ligado. CC65/68/84 não sobrepõem esse estado; mensagens de modo 124–127 mantêm o efeito de liberar notas, mas não substituem o modo definido pelo knob. MIDI Learn continua alterando o parâmetro do host pelo caminho existente.
- CC64, CC1 e controles de expressão seguem o caminho nativo. CC121 reaplica os controles de portamento do host após o reset. Pânico também limpa o histórico de teclas e de portamento, para a próxima nota começar afinada.

As chamadas de configuração agora verificam o status e apresentam falha em vez de ignorar erro. Durante a revisão foi corrigida a constante já existente que identifica CC no removedor de modulação padrão de CC1: a API pública usa `FLUID_MOD_CC=0x10`, e não `0x80` (que corresponde ao antigo SIN e é inválido desde 2.5). Isso mantém a intenção anterior de remover só o vibrato padrão aditivo; não reescreve os moduladores embutidos do SF2.

Não há mudança no layout, na ponte de comandos, nos formatos de perfil, nos valores salvos, na lógica do motor SFZ ou no processamento ASIO. O SFZ continua com sua implementação anterior; `MonoGlide` não foi alterado nesta etapa.

## Validação

`tests/real_sf2_glide_test.py` gera um SF2 determinístico com amostra senoidal própria e dois presets: normal e pitch wheel anulado. `tests/real_sf2_glide_probe.cpp` usa a classe de produção `Sf2Engine` e uma biblioteca real, sem sintetizador de teste. Frequências são medidas nas amostras de áudio por cruzamentos interpolados de zero. O teste reprova a implementação anterior.

Passaram os cenários de subida, descida, tempos curto/médio/longo, intervalo de quatro oitavas, retorno à tecla ainda pressionada, legato rápido sem salto, staccato, pitch bend separado, pânico, sustain/soltura do pedal, retorno à polifonia e reset CC121. Cada cenário roda em um processo separado para não reinscrever tipos de libinstpatch/GLib durante recarga de versões Linux antigas.

- FluidSynth 2.6.1 real, compilado do tag oficial, commit `71e85b2ca6bf48641ba7e2261d8f2640ae473773`: passou.
- FluidSynth 2.3.4 real, pacote Ubuntu: passou, incluindo caminho sem API de modo de tempo introduzida em 2.5.
- 10/10 grupos CTest, incluindo o SF2 TimGM6mb real para o teste anterior de ADSR/efeitos/MIDI: passaram.
- Compilação local Windows x64 do alvo `LivePerformanceVision`, com PortAudio/ASIO e WebView2: passou; marcador Vision conferido no EXE.

Os valores medidos e hashes estão em `docs/validation/sf2-native-glide-results.json`. Essa validação é áudio real offline e compilação cruzada, não teste de ASIO, teclado ou bancos pessoais no Windows. Esta etapa não gerou novo portátil nem iniciou Actions. O saldo autenticado de uso GitHub continua desconhecido porque as ferramentas não expõem essa consulta.

## Fontes primárias consultadas

- https://www.fluidsynth.org/api/group__channel__setup.html — modos de mono/legato/portamento e modo de tempo.
- https://www.fluidsynth.org/api/settings_synth.html#synth_portamento-time — CC5/37 e mudança de interpretação a partir de 2.5.
- https://www.fluidsynth.org/api/group__modulators.html — valores públicos dos flags de moduladores, diferentes dos bits do arquivo SF2.
- https://github.com/FluidSynth/fluidsynth/blob/v2.6.1/src/synth/fluid_synth.c — limite da sensibilidade, retorno de erro e configuração MIDI.
- https://github.com/FluidSynth/fluidsynth/blob/v2.6.1/src/synth/fluid_synth_monopoly.c — escolha das notas de origem e destino.
- https://github.com/FluidSynth/fluidsynth/blob/v2.6.1/src/rvoice/fluid_rvoice.c — acumulação do deslocamento para manter continuidade no legato.
