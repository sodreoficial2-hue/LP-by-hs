# Etapa 8 — sustain, modulation e pitch bend

A entrada MIDI agora encaminha pitch bend completo de 14 bits da janela ao callback ASIO e à função nativa `sfizz_send_pitch_wheel`. O centro MIDI 8192 vira zero; os extremos viram -8192 e +8191. A extensão musical segue o SFZ (normalmente dois semitons, podendo ser diferente por instrumento).

O sustain CC64 é um controle dedicado: 0–63 significa solto, 64–127 pressionado. O host normaliza esses estados para 0/127, pois o padrão de limiar do sfizz é diferente do pedal MIDI convencional. Não há suporte novo a meio pedal nesta etapa. CC64 e mensagens de modo do canal (120–127) não são capturadas pelo MIDI Learn. Vínculos antigos de knob com esses CCs continuam no arquivo, mas não interceptam a função nativa.

A modulation CC1 chega ao instrumento inclusive se estiver simultaneamente aprendida para um parâmetro. Os demais vínculos de knob mantêm o comportamento anterior. O banco curado recebe vibrato de 5 Hz, profundidade máxima de 35 cents, nos oito leads mono/poly. Nos demais timbres, CC1 mantém sua função SFZ: por exemplo, expressão/dinâmica ou vibrato dos saxofones. Um piano sem modulação programada não ganha vibrato artificial.

Os saxofones soprano e alto MTG tinham sustain desativado e CC64 como legato. As adaptações habilitam sustain/sostenuto e reservam CC68 para o legato. Créditos e licença CC-BY-4.0 permanecem; as amostras de áudio não foram modificadas.

## Estado de execução

O host acompanha o último pedal, CC1 e bend recebidos mesmo sem ASIO conectado. Aplica os valores ao novo instrumento e ao reconectar/trocar o ASIO, inclusive se a fila antiga não tiver sido consumida. Controles ainda não recebidos deixam os defaults nativos do SFZ intactos. As posições físicas não são gravadas no perfil de show.

Pânico e estouro da fila desligam notas, liberam pedal, zeram modulation e centralizam bend; isso evita restaurar um pedal preso na próxima troca. CC121 também reinicia esses três controles. Notas/caudas antigas continuam encerradas na troca de instrumento, conforme o host existente; a preservação é do estado dos controles, não de vozes entre dois sintetizadores.

## Restaurar bancos antigos

O montador já aplica a configuração. Para usar os ZIPs de dados salvos na etapa 7, extrair todas as partes e aplicar uma vez:

```sh
python3 scripts/configure_bank_performance.py /caminho/soundbank
```

O procedimento é idempotente, altera somente mapeamentos e refaz o inventário SHA-256. O protótipo local já recebeu as adaptações. O arquivo PERFORMANCE.txt identifica as mudanças e suas fontes. Não foram gerados novos ZIPs de teste, instalador ou upload ao Drive nesta etapa.

## Verificação

- Regressão de MIDI Learn e favoritos PC 1–20, CC64 reservado, CC1 com função dupla e bend nos extremos, centro e variação de um único bit.
- Callback com PortAudio/sfizz simulados: pedal/CC1/bend atravessam troca de timbre, fila pendente, desconexão/reconexão, reset e pânico; fluxo anterior de falhas e rollback preservado.
- Sfizz real: nota de referência em 440 Hz, bend inferior medido em 391,996 Hz, superior em 493,883 Hz e retorno a 440 Hz. CC1 muda a afinação de uma nota sustentada e retorna ao repouso.
- Sfizz real: sustain nas posições 64 e 127, liberação em zero, pedal solto em 63. Renderização adicional verifica retenção e liberação em órgão drawbar, sax alto, sax soprano e lead Square mono instalados.
- Nove grupos CTest, incluindo o banco completo de 66 presets e mono/poly. Compilação Windows local concluída; a DLL Windows recuperada exporta a função de bend necessária.

A validação sonora acima é offline no Linux. O callback foi exercitado com PortAudio simulado. O teclado físico e ASIO real no Windows ainda precisam da avaliação do usuário na próxima versão autorizada; não inferir essa validação a partir da compilação.

A cota autenticada do GitHub continua desconhecida: não há ferramenta disponível de saldo/rate-limit autenticado. Apenas um commit agrupado de código e metadados; sem Actions, executáveis ou amostras enviados ao GitHub.
