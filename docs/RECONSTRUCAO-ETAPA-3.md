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

Botão direito ou menu de contexto por teclado sobre um favorito ocupado inicia a captura exclusivamente de Program Change. ESC cancela. Favorito 1 corresponde ao PC 1 até favorito 20 ao PC 20, na numeração exibida ao músico. No protocolo MIDI os valores são 0–19; a conversão é interna.

O comando deve corresponder ao número fixo do favorito: ao aprender o favorito 5, por exemplo, somente PC 5 conclui a captura. Outro PC informa o número esperado e mantém a captura ativa, sem trocar timbre nem alterar o vínculo. Notas e CC nunca selecionam favoritos nem concluem essa captura. Notas continuam musicais e CC continuam disponíveis para os knobs.

O canal do PC aprendido é memorizado; após a captura, apenas o canal aprendido seleciona aquele favorito. Antes da captura, o PC correspondente funciona em qualquer canal. PC repetido continua acionável sem precisar de liberação. O usuário guarda os vínculos com Salvar perfil.

## Perfis

Escrita `LPHS_PROFILE 3`: acrescenta tipo Program Change (0xc0), canal e valor MIDI após os campos anteriores. Validação exige que o valor corresponda ao slot; nota/CC não são vínculos válidos de favorito. Leitura v1 mantida. Leitura v2 preserva timbres, parâmetros e CC dos knobs, removendo apenas vínculos antigos de nota/CC dos favoritos, que contrariavam a instrução do usuário. Arquivo/show válido não é substituído se os dados forem inválidos. Sem importação dos `.vshow.ini` originais. Leitores antigos não abrem v3.

## Verificação executada neste ambiente

- `bash scripts/verify_local.sh`: três grupos passaram (sessão, motor/MIDI, DSP).
- Testes DSP com sinais sintéticos: neutralidade estéreo, volume, resposta do filtro/EQ, compressão, tremolo, caudas dos efeitos temporais, reset, continuidade entre blocos e saída finita em 8/44,1/48/96/384 kHz.
- Testes MIDI: correspondência dos 20 PCs, canal aprendido, repetição de PC, nota/CC não capturam favorito, PC incorreto não altera vínculo, cancelamento e MIDI Learn CC dos knobs preservado.
- Perfis: ida/volta v3, leitura v1, migração v2 de notas/CC, rejeição de PC que não corresponde ao slot e preservação do show diante de corrupção.
- Teste do motor usa biblioteca simulada, incluindo aplicação de ganho após render. Não usa a DLL Windows real neste Linux.
- ASan/UBSan em DSP, sessão e motor/MIDI; leaks desativados pela limitação já registrada do ambiente.
- A interface Win32 foi editada, mas não compilada nem executada nesta etapa: compilador cruzado Windows indisponível aqui.

## GitHub

Inspecionadas as ferramentas autenticadas disponíveis: saldo de API, billing e minutos de Actions continuam indisponíveis. Saldo desconhecido. Nenhuma execução Actions iniciada, nenhum workflow automático criado. Publicação desta etapa agrupada em um único commit de fontes/documentação; não reenviar DLLs, instaladores ou amostras já preservados.

## Pendências prioritárias

Stream PortAudio/ASIO com seleção e painel do driver, persistência exclusiva de áudio, propriedade/thread do renderizador e troca segura de timbre; ADSR por voz; Envelope Filter; importação dos perfis originais; categorias e acabamento acessível; compilação/validação no Windows e teste auditivo com os drivers reais.
