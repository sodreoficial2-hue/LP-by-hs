# Etapa 3 — DSP e botões MIDI dos favoritos

## Resultado implementado

`Dsp` processa os canais estéreo após `sfizz_render_block`. Em estado neutro, preserva exatamente os samples. Processamento sem alocação de memória dentro de `process`; buffers preparados na criação. Não há stream de saída de áudio nesta etapa.

Controles ligados ao processamento:

- Volume: ganho 0–1.
- Filter: passa-baixas de um polo, frequência exponencial de 80 Hz a 16 kHz; posição máxima contorna o filtro. Ainda não é Envelope Filter dinâmico.
- EQ: três filtros de pico em 120 Hz, 1 kHz e 6 kHz (limitado em taxas baixas), de -12 a +12 dB; posição central neutra.
- Compressor: detector de pico com canais vinculados, ataque de 5 ms, recuperação de 100 ms, limiar/razão controlados pelo knob. Não é um limiter.
- Tremolo: modulação de amplitude de 5 Hz.
- Delay: 320 ms com feedback de 0,35 e mistura controlável.
- Chorus: atraso interpolado modulado em 0,8 Hz, fases distintas por canal.
- Reverb: quatro combs por canal, feedback de 0,72; tempos distintos entre canais.

Volume é aplicado no fim da cadeia. Não há clipping ou normalização automática; boosts de EQ e sinais somados exigem margem de nível. Alterações de parâmetros ainda não têm suavização completa e podem produzir degraus audíveis; calibração musical/auditiva pendente. Não foi comprovada qualidade sonora profissional nem desempenho de baixa latência.

Os quatro parâmetros ADSR continuam apenas persistidos. A interface identifica explicitamente essa pendência. Não foi aplicada uma envoltória global ao sinal estéreo porque isso não substitui ADSR por voz em acordes e sustain.

`SfizzEngine::parameters` recebe o estado do favorito; seleção de favorito e alterações via mouse/teclado/MIDI atualizam o DSP. Carga bem-sucedida, unload e pânico limpam as caudas. Carga SFZ malsucedida preserva o sintetizador e DSP anteriores. A política de troca com caudas sustentadas permanece pendente.

## MIDI Learn de favoritos

Botão direito ou menu de contexto por teclado sobre um favorito ocupado inicia a captura de nota ou CC. ESC cancela. O vínculo inclui canal MIDI. Para CC, a transição de valor abaixo de 64 para valor de 64 ou mais aciona uma única troca; a liberação é consumida. A nota reservada, inclusive Note Off e Note On de velocity zero, não chega ao sintetizador. Uma mesma mensagem não é atribuída simultaneamente a dois favoritos; nova captura transfere o vínculo. Botões reservados têm precedência sobre MIDI Learn de knobs e execução musical.

Program Change continua 0–19 no protocolo, exibido como 1–20. Fechar/trocar entrada MIDI limpa estado dos botões e chama pânico. Favoritos são salvos pelo usuário; não foi adicionado salvamento automático do show.

## Perfis

Escrita `LPHS_PROFILE 2`: cada linha acrescenta tipo, canal e número do botão após os campos existentes. Leitura v1 da reconstrução preservada, sem vínculo de botão. Duplicatas e campos inválidos são rejeitados antes de substituir arquivo/show válido. Esta versão não importa os `.vshow.ini` originais. Perfis v2 não podem ser abertos pela etapa 2 antiga.

## Verificação executada neste ambiente

- `bash scripts/verify_local.sh`: três grupos passaram (sessão, motor/MIDI, DSP).
- Testes DSP com sinais sintéticos: neutralidade estéreo, volume, resposta do filtro/EQ, compressão, tremolo, caudas dos efeitos temporais, reset, continuidade entre blocos e saída finita em 8/44,1/48/96/384 kHz.
- Testes MIDI: canais, botões note/CC, prevenção de repetição enquanto pressionado, releases consumidos, transferência de vínculo e cancelamento.
- Perfis: ida/volta v2, leitura v1, rejeição de botões duplicados e preservação do show diante de corrupção.
- Teste do motor usa biblioteca simulada, incluindo aplicação de ganho após render. Não usa a DLL Windows real neste Linux.
- ASan/UBSan em DSP, sessão e motor/MIDI; leaks desativados pela limitação já registrada do ambiente.
- A interface Win32 foi editada, mas não compilada nem executada nesta etapa: compilador cruzado Windows indisponível aqui.

## GitHub

Inspecionadas as ferramentas autenticadas disponíveis: saldo de API, billing e minutos de Actions continuam indisponíveis. Saldo desconhecido. Nenhuma execução Actions iniciada, nenhum workflow automático criado. Publicação desta etapa agrupada em um único commit de fontes/documentação; não reenviar DLLs, instaladores ou amostras já preservados.

## Pendências prioritárias

Stream PortAudio/ASIO com seleção e painel do driver, persistência exclusiva de áudio, propriedade/thread do renderizador e troca segura de timbre; ADSR por voz; Envelope Filter; importação dos perfis originais; categorias e acabamento acessível; compilação/validação no Windows e teste auditivo com os drivers reais.
