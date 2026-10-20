# Etapa 4 — saída ASIO e pacote Windows x64

## Implementado

`AudioHost` integra a API PortAudio real ao renderizador sfizz. A compilação habilita somente ASIO e desabilita WASAPI, DirectSound, MME e WDMKS. A lista da interface mostra apenas drivers ASIO com pelo menos duas saídas. Nenhuma seleção automática de outro backend.

A interface oferece lista ASIO, buffer (padrão do driver ou 64/128/256/512/1024/2048 frames), Conectar, Desconectar, ASIO Control e Pânico (F12). Taxa inicial fixa de 48 kHz; um driver que não aceite esse formato retorna erro. A latência mostrada é a de saída reportada pelo driver, não uma medição total de latência MIDI/áudio.

O último driver conectado e seu buffer são gravados automaticamente em `%APPDATA%\Live Performance by Hugo Sodre\audio.settings`. A reabertura tenta exatamente o driver salvo por nome; ausências ou nomes duplicados exigem seleção manual, sem fallback. Favoritos, knobs e vínculos PC continuam sendo guardados somente com Salvar perfil.

Enter ou duplo clique sobre o catálogo carrega o timbre para tocar sem precisar favoritar. Espaço acrescenta aos favoritos. Para editar e guardar parâmetros por timbre, use um favorito. ESC, E, setas, Tab e Program Changes 1–20 preservados. Mensagens MIDI atrasadas de uma entrada já fechada são descartadas pelo handle de origem.

## Propriedade do áudio

- Enquanto o stream roda, o callback é o único consumidor do sintetizador ativo.
- A interface coloca notas/CC numa fila SPSC fixa; Program Change e MIDI Learn são resolvidos na interface. Os parâmetros usam um snapshot atômico consistente, sem parar o stream a cada knob.
- Overflow ou Pânico esvaziam eventos pendentes e silenciam vozes/caudas, evitando deixar Note Off perdido.
- SFZ candidato é criado e validado fora do callback enquanto o timbre anterior continua renderizando. Só depois o stream é parado e a instância trocada. Uma falha SFZ não para o áudio antigo.
- A retomada usa o mesmo dispositivo e buffer. Se a primeira retomada falhar, tenta restaurar o sintetizador/dispositivo anteriores; erros são apresentados. Falha dupla deixa áudio parado e sinaliza erro.
- ASIO Control para o stream antes de abrir o painel e tenta restaurá-lo depois. Desconexão e troca de driver liberam a fila e silenciam notas.
- O callback processa buffers grandes em blocos de até 256 frames, captura falhas e produz silêncio. A interface verifica o sinal de falha periodicamente. Não há I/O de arquivo, MessageBox, carregamento SFZ ou destruição de sintetizador no callback.

A preparação SFZ ainda bloqueia a interface; mensagens MIDI WinMM passam pela fila da janela. Ainda não há troca sem interrupção, preservação de caudas entre timbres ou homologação de palco. Reset/crash de um driver de terceiros não pode ser tratado como erro C++ comum garantidamente recuperável.

## Compilação reproduzível

Dependências externas: PortAudio v19.7.0, commit `147dd722548358763a8b649b3e4b41dfffbcfbb6`, e Steinberg ASIO SDK. Não há download automático no CMake nem execução Actions. Os cabeçalhos públicos PortAudio usados são preservados em `third_party/portaudio` com seus avisos. O SDK ASIO não é redistribuído como fonte no repositório.

Windows com CMake e compilador C/C++ instalados:

```powershell
./scripts/build_windows.ps1 -PortAudioSource C:/deps/portaudio -AsioSdkRoot C:/deps/asiosdk
```

Equivalente:

```text
cmake -S . -B build -DLP_PORTAUDIO_SOURCE=C:/deps/portaudio -DASIOSDK_ROOT_DIR=C:/deps/asiosdk
cmake --build build --config Release --target LivePerformance
```

A cópia das DLLs/amostras recuperadas é habilitada por padrão. O executável inclui manifesto UTF-8 para caminhos de SFZ com acentos no Windows moderno.

## Validação executada

- Quatro grupos de testes Linux passaram via script e CMake/CTest.
- `audio_host_test` usa PortAudio e sfizz simulados para testar enumeração exclusivamente ASIO, callback estéreo com 2048 frames, MIDI, parâmetros sem reabrir o driver, preservação após SFZ inválido, rollback após erro de abertura, mesmo buffer/dispositivo, overflow, pânico, painel e persistência de áudio.
- ASan/UBSan no host de áudio e dependências testadas, com leak detection desativada pela limitação já registrada do ambiente.
- Compilação cruzada Windows x64 bem-sucedida com MinGW GCC 13, PortAudio v19.7.0 e ASIO SDK 2.3.3. Link real, incluindo backend ASIO, recursos e manifesto.
- Inspeção PE: executável não exige DLLs MinGW externas. As DLLs/amostras sfizz são as recuperadas, preservadas separadamente dos fontes.
- Não houve execução do novo EXE no Windows, teste visual, renderização auditiva com a DLL real, medição de latência ou teste com ASIO4ALL/TASCAM/controlador físico. O pacote é de teste, não instalador final homologado.

Referências da implementação: https://portaudio.com/docs/v19-doxydocs/portaudio_8h.html ; https://github.com/PortAudio/portaudio/blob/v19.7.0/include/pa_asio.h ; https://portaudio.com/docs/v19-doxydocs/writing_a_callback.html .

## Pendências

ADSR por voz, Envelope Filter dinâmico, categorias completas e acabamento visual, compatibilidade com perfis INI originais, troca sem interrupção e validação no Windows real. O painel continua identificando ADSR como pendente, sem fingir que altera envoltória por voz.

## Uso remoto

Saldo autenticado de API/billing/minutos Actions continua desconhecido porque as ferramentas não o expõem. Nenhuma execução Actions iniciada. Testes e compilação Windows realizados localmente. Publicação agrupada em um commit de fontes e documentação; DLLs/amostras existentes não são reenviadas ao GitHub.
