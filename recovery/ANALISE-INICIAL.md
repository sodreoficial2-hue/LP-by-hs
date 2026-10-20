# Recuperação técnica dos instaladores — 20/10/2026

## Origem e integridade

1. **Live-Performance-by-Hugo-Sodre-Instalador-Windows-x64-Teste(1).exe** — 29.442.742 bytes; SHA-256 `f1efad01f6ad55a6b09322cd96eed8c92add6cc1ad5285c46386811f5d91a2a9`.
2. **Vision-SFZ-Native-Prototipo-0.8-Windows-x64.exe** (dentro do ZIP recebido) — 21.568.321 bytes; SHA-256 `88a101e5364b304ad0f0db36e6519f2f643a863a4f49c720fe0a69455cf347b7`.

Ambos são executáveis PE32 de instalador **NSIS/Nullsoft**. Não executar os arquivos até revisão em ambiente Windows isolado.

## Evidências concretas no instalador Live Performance

As cadeias legíveis do binário incluem símbolos da API **sfizz**: `sfizz_create_synth`, `sfizz_load_file`, `sfizz_send_note_on`, `sfizz_send_note_off`, `sfizz_send_cc`, `sfizz_render_block`, `sfizz_all_sound_off` e `sfizz_get_num_regions`. Há também a string `sfizz.dll`.

O binário inclui cadeias de **PortAudio V19.7.0-devel**, mensagens de driver **ASIO** (incluindo `Failed to load ASIO driver`) e referências a **WASAPI** em caminhos de compilação do PortAudio. Referências a WASAPI no binário **não comprovam** que a interface do programa o ofereça como opção de áudio.

Constam referências às funções Windows MIDI `midiInGetNumDevs`, `midiInGetDevCapsW`, `midiInOpen`, `midiInStart`, `midiInStop` e `midiInClose`.

Uma string de caminho de compilação aponta para `D:\\a\\vision-library-instrument\\vision-library-instrument\\build-native\\_deps\\portaudio-src\\src\\hostapi\\wasapi\\pa_win_wasapi.c`, o que sugere uma origem de build do projeto Vision Library, mas não recupera o repositório.

Uma string de manifesto identifica o componente `Vision.SFZ.Native`.

## Limitações

- A extração inicial com libarchive não reconheceu o formato de payload NSIS; **nenhum arquivo instalado foi extraído nesta etapa**.
- As strings comprovam componentes presentes/referenciados, mas não equivalem a código-fonte original nem comprovam que cada função esteja operacional.
- O instalador antigo apresentou poucas strings significativas na varredura direta; isso pode decorrer de compressão.
- Não há, até aqui, reconstrução compilável nem teste de áudio, MIDI, efeitos ou interface.

## Próximos passos técnicos

1. Extrair ambos os NSIS em ambiente controlado com ferramenta que suporte NSIS, ou instalar em VM Windows isolada e copiar os arquivos de instalação.
2. Inventariar executáveis, DLLs, recursos gráficos, arquivos SFZ, presets e configurações.
3. Inspecionar formato dos executáveis (nativo C/C++, .NET, Python empacotado etc.) e separar recursos recuperáveis de código a reimplementar.
4. Construir projeto-fonte editável e testes de regressão, preservando comportamento confirmado pelo usuário.
5. Não publicar bibliotecas de terceiros no repositório antes de verificar suas licenças.

**Status:** investigação inicial documentada; ainda não é o código-fonte recuperado.
