# Análise estrutural anterior ao parser

Nenhum arquivo pessoal foi anexado. O vetor público `tests/fixtures/npno/tine-demo.npno` tem áudio sintetizado pelo projeto de referência: não é áudio de fábrica nem uma gravação de teclado. Análise por bytes feita em 2026-11-03 antes da implementação. SHA-256: `b9910e341008adb31a708daa9080293faff1c47e191d9d5f1b191a1bb0b748db`.

## Contêiner efetivamente observado

74.458 bytes; magic `43 42 49 4e`; formatType 1; tag `npno`; versão externa 540 (5.4); corpo em 44; magic do corpo `43 4e 53 50`; versão interna `0x450`. O CRC-32 confere. **Não há tabela de strings indexadas.** O documento NovaResearch é de jogos e descreve outro formato chamado CBIN; não se aplica a este instrumento.

Offsets abaixo são relativos ao início CNSP (B), salvo prefixo H (arquivo) ou R (registro de amostra). BE = big-endian, LE = little-endian. Confirmação por código público e por este vetor não substitui uma comparação com arquivos pessoais de todas as revisões.

| Offset | Tamanho / tipo | Ordem | Significado / evidência |
|---|---|---|---|
| H+0 | 4 ASCII | — | CBIN, confirmado no vetor |
| H+4 | u32 | LE | Envelope 0 (B=24, CRC16 no trailer) ou 1 (B=44, CRC32 no H+24) |
| H+8 | 4 ASCII | — | `npno`, confirmado |
| H+12/14 | u16 cada | LE | banco/slot; não alteram reprodução |
| H+16/20 | u32 cada | LE | categoria/revisão externa |
| B+0/4 | ASCII4/u16 | BE | CNSP/versão interna 0x450 ou 0x464 |
| B+0x1c | 32 ASCII | — | Nome#variante; não há string IDs |
| B+0x3c/0x5c | 32 ASCII cada | — | nomes longos/voicing de v6, referência nsp.ts |
| B+0x8c | 128 u8 | — | mapa MIDI tecla→root; 255=sem cobertura |
| B+0x18c | 128 i8 | — | afinação por tecla, 0,7 cents/unidade; medida entre 0,6–0,8 na referência |
| B+0x30c | 128 u8 | — | nível de soltura v5; escala não traduzida, conservar bytes |
| B+0x40c | i8 | — | ganho global em 0,1 dB, não é detune |
| B+0x40d | u8 | — | última tecla abafada na soltura |
| B+0x61c/61e/620 | u16 cada | BE | eco da versão interna/canais/contagem de registros |
| B+0x622 | 128 u16 | BE | contagens de registros por root; soma deve bater com diretório |
| B+0x732 | N×118 bytes | — | diretório; regiões ordenadas por root |
| R+0 | u32 | BE | primeiro bloco, relativo a B |
| R+4/5 | u8 cada | — | banco 0 ataque/1 pedal/2 soltura; piso dinâmico |
| R+6/0xa | u32/u16 | BE | frames/blocos |
| R+0xc/0x14 | 4 i16 cada | BE | histórico por canal, mais antigo primeiro |
| R+0x1c/0x20 | u32 cada | BE | início/comprimento do loop, fim exclusivo |
| R+0x24/28/2c | u32/u32/u16 | BE | marcações auxiliares do bloco de loop |
| R+0x2e | u32 Q23 | BE | decaimento no loop; unidade física diverge nas referências |
| R+0x32 | u16 | BE | janela/eco do piso; sem efeito medido na referência |
| R+0x34 | i16 | BE | trim: 1 dB em v5; 0,1 dB em v6; v5 negativo silencia |
| R+0x36 | 14 u32 Q23 | BE | escada de decaimentos por transposição, não é afinação |
| R+0x6e | u32 | BE | identificador de take, não confirmado como round robin |
| R+0x72/73/74/75 | 4 u8 | — | fade de soltura/outro fade/limiar v6/reservado; conservar raw |
| bloco+0 | u16 | BE | bits 0–4 largura residual; 5–7 ordem; 8–15 estatística de nível |
| bloco+2 | palavras u16 | BE | resíduos em complemento de 2, campos LSB-first; canais intercalados |

## Estatísticas do vetor público

3 roots (52, 76, 100), 3 ataques, mono, sem loop, uma camada cada, trim 3 dB. Mapa: teclas 0–53→52, 54–77→76, 78–101→100; 102–127 sem cobertura no original. Registros: offsets CNSP 2874/25358/47842; frames 53876/35238/23095; blocos 22/22/26. Diretório termina em B+2196, seguido de alinhamento zero até B+2874. Áudio codificado contíguo, 70×1022=71540 bytes. PCM esperado 224418 bytes (112209 frames × 16 bits mono); duração acumulada 3,2058 s a 35002 Hz. Taxa não é um campo do arquivo: é a constante de reprodução documentada no codec de referência.

## Divergências e campos pendentes

- OpenNord usa 44100 Hz e compensação de root +4; drawbar documenta 35002 Hz com root original e teste em hardware. O parser usará a segunda convenção, sem resampling. As convenções são aproximadamente equivalentes em pitch; não representam taxas lidas de um campo.
- Curva de velocity não é uma lista de limites MIDI no arquivo: a seleção usa pisos/limiares e depende do teclado/configuração. Usaremos uma curva de referência medida, Touch 0, interpolada (incerteza até 2 velocities). Não alegar equivalência universal.
- Banco 1 é chamado de lista alternativa no OpenNord e de ressonância com pedal na referência com teste de hardware. Usaremos CC64, não round robin inventado.
- [hipótese] Revisões externas 5.0/5.2/5.3/5.4 e 6.0/6.1/6.2/6.3 compartilham o layout da respectiva versão interna. Só 5.4 é observada neste vetor público. Testes sintéticos cobrirão ambos os layouts, sem alegar validação de corpus real.
- Não foi identificado campo de detune global, pan, crossfade ou sequência round robin. Não fabricar offsets. Bytes de cabeçalho e registros serão conservados no sidecar e original.
- A especificação PDF rev.9 citada no pedido não foi recuperada; não a usamos como evidência.

## Referências consultadas

1. [OpenNord, commit c86b22a](https://github.com/simonflore/opennord/tree/c86b22a8014afcc9d6dc96409992f10f3c004f57), `src/lib/ns4/nsp.ts`, `nsp-audio.ts` (`readNspDirectory`, `readStroke`, `decodeNspStroke`), `nsp-write.ts`, `nsp-synth.testing.ts`, leitores e testes CBIN/checksum. AGPL-3.0.
2. [drawbar, commit b680c5b](https://github.com/jmoo/drawbar/tree/b680c5b0b9ec9b8b04da951c6684c39aad9afe48), `crates/nord-format/src/formats/npno/{mod,codec,index,synthetic}.rs`, `formats/ne5/velocity.rs` (`top_velocity`), `crates/nord-cli/src/piano.rs`, fixture demo e sua documentação. BSD-3-Clause.
3. [Bridge](https://github.com/misteriosooo1/Nord-Kontakt-Bridge): README aponta para a CLI anterior, não especifica outro codec.
4. [NovaResearch CBIN](https://github.com/Novahq-net/NovaResearch/blob/main/Shared/CBIN%20File%20Format.md): examinado e descartado, contêiner de jogos diferente.
5. [Relato de conversão](https://yamahasynth.com/community/montage-series-synthesizers/nord-sounds-for-the-montage/): contexto histórico; não fornece offsets usados aqui. PDF e fóruns indicados sem conteúdo técnico recuperável nesta sessão.

## Intervalos opacos e diferenças entre os vetores

Além dos campos da tabela, os intervalos CNSP `[0x06,0x1c)`, `[0x7c,0x8c)`, `[0x10c,0x18c)`, `[0x20c,0x30c)`, `[0x38c,0x40c)`, `[0x40e,0x61c)` e `[0x722,0x732)` permanecem opacos. Em v5 os campos de nomes adicionais `[0x3c,0x7c)` também permanecem opacos. O cabeçalho externo completo e todos esses bytes continuam no original; o prefixo do corpo e cada registro são conservados no sidecar. Não atribuímos significado a campos desconhecidos. Os alinhamentos entre diretório/áudio e entre strokes são aceitos apenas quando zerados; blocos sobrepostos ou spans fora do EOF são rejeitados.

| Vetor | Envelope / B | Revisão / stream | Canais / registros | Frames / PCM bytes | Áudio codificado | Duração somada |
|---|---|---|---|---|---|---|
| tine-demo.npno | 1 / 44 | 540 / 0x450 | 1 / 3 | 112209 / 224418 | 71540 B | 3,2058 s |
| layers-v5.npno | 0 / 24 | 530 / 0x450 | 1 / 18 | 42570 / 85140 | 36792 B | 1,2162 s |
| layers-v6.npno | 1 / 44 | 610 / 0x464 | 2 / 18 | 42190 / 168760 | 73584 B | 1,2054 s |

As duas últimas entradas são construções sintéticas. Têm roots 48 e 72, nove registros por root (três camadas em cada banco), loop 100..899 nos ataques/ressonâncias, e tecla 61 com raw fine-tune 3. v5 armazena pisos 0/12/24 e deriva limiares 0/30/90; v6 armazena pisos 0/60/120 e lê limiares 0/30/90 de R+0x74. A curva escolhida produz faixas 100–127, 62–99, 1–61. v5 usa trim +2 dB; v6 usa trim -24 ×0,1 dB; ganho global é +1 dB nesses dois vetores. A demo pública tem ganho global +5 dB, trim +3 dB (saldo +2 dB). Todos os checksums dos três vetores conferem.

O delta de tamanho entre os vetores sintéticos não é apenas o cabeçalho: estéreo dobra palavras por bloco e muda a escolha de largura residual e o padding de frames. São dados codificados, não PCM cru contíguo. As estatísticas e o histograma real de largura/ordem dos blocos estão no JSON acompanhante. Nenhuma dessas estatísticas foi apresentada como análise de arquivos pessoais inexistentes.
