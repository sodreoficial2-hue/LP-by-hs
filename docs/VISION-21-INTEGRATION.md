# Integração visual Vision Laiberry 2.1 → Live Performance

## Resultado e arquitetura

A interface adicional `LivePerformanceVision` reutiliza a linguagem visual do
Vision Laiberry 2.1 (cores, fontes e imagens recuperadas) sobre o **mesmo App C++**
do Live Performance. Um WebView2 incorporado envia comandos à thread da interface
Win32, que chama as funções atuais de seleção de timbres, parâmetros, áudio,
MIDI, favoritos, performances e diálogos de arquivos.

Não há sintetizador no JavaScript, Web Audio, Web MIDI, tráfego de áudio pela ponte
ou segundo AudioHost. O MIDI físico continua chegando por WinMM ao App original.
`audio_host.cpp`, `sfizz_engine.cpp`, `sf2_engine.cpp`, `dsp.cpp`, `midi.hpp` e
`session.hpp` não foram alterados nesta integração.

O alvo clássico `LivePerformance` continua disponível. A opção
`LP_ENABLE_VISION_UI` é desligada por padrão, e adiciona o novo alvo quando ligada.
Nenhum workflow do Actions foi alterado nesta etapa.

## Origem do visual

O instalador `Vision-Laiberry-Instruments-2.1-ASIO-Setup-Windows-x64.exe` é um SFX
com conteúdo ZIP legível sem execução. Seu `resources/app/package.json` informa
`version: 2.1.0`. SHA-256 do instalador:
`efe2a668004f10a51b78a83a4d3f2d2c327f5406bf92df39c27473dd192c0157`.

`scripts/prepare_vision_assets.py` recupera os bytes originais das três imagens de
referência e das fontes DM Sans e Plus Jakarta Sans. O retrato vem do recurso
existente do Live Performance. O resultado é `vision-ui/assets.js`, autocontido,
com proveniência e hashes em `vision-ui/asset-provenance.json`.

As imagens dos instrumentos são recortes **durante a renderização SVG**, com
clipping explícito; os JPEGs originais não foram modificados. Algumas ilustrações
disponíveis apenas no CDN antigo retornaram acesso negado. Por isso, a interface
usa os recursos recuperados localmente e não depende daquele CDN.

O React/Next.js minificado antigo não é executado. Ele também continha o
sintetizador GM/SF3, armazenamento em localStorage e chamadas `/api/favorites`,
incompatíveis com o estado do host atual. O novo JavaScript reconstrói os
componentes de navegação e conecta os controles ao modelo nativo existente.
É uma adaptação para as 16 categorias atuais, não uma cópia de pixels da antiga
grade de 12 categorias.

## Comportamentos integrados

- As 16 categorias ficam na tela inicial, com foco e setas. Navegar por categorias
  não troca o timbre carregado.
- Clique, Enter ou setas na lista selecionam/carregam o timbre pelo App original.
- Espaço favorita; existem 20 posições numeradas. Clique direito em um favorito
  chama o aprendizado original, que espera o Program Change da posição (1–20 na
  tela, 0–19 no MIDI). Notas e CC não viram seleção de favorito.
- `E` abre/fecha os parâmetros; `ESC` cancela o aprendizado e retorna; `F12`
  executa pânico. Digitar E em um campo de texto não abre parâmetros.
- Os 16 controles do host incluem Master. No mix de Performance são mostrados
  apenas os quatro volumes de parte. MIDI Learn chama o mesmo código nativo.
- ASIO, buffer, MIDI, ASIO Control e reconexão continuam sob controle nativo.
- Abrir/Salvar perfil e importar SFZ/SF2 usam os diálogos Windows existentes.
- O editor usa as quatro partes, os modos de divisão e o arquivo de perfil atuais.
- O teclado virtual libera notas em key-up, perda de foco, troca de tela e pânico.
- Status e alterações recebidas pelo MIDI físico são sincronizados com a tela.
- Não há cópia dos perfis no navegador nem salvamento automático de perfis de show.

## Ponte e proteção do processamento

`vision_protocol.hpp` valida um protocolo de comandos limitado, com IDs,
índices, limites numéricos e texto UTF-8 percent-encoded. Não aceita comandos
arbitrários do Windows, caminhos de arquivos ou código. Textos e nomes são
serializados em JSON e inseridos na tela com textContent.

Somente `https://lp-vision.local/index.html`, mapeado à pasta local, pode enviar
comandos. Navegação externa, novas janelas e permissões web são bloqueadas. A
CSP não permite rede, frames ou objetos. A criação e os callbacks do WebView2
usam COM STA e guardas de vida útil; os comandos são enfileirados ao HWND nativo.
Nenhum callback de áudio faz acesso ao DOM ou espera pelo navegador.

Se o runtime ou os arquivos visuais não estiverem disponíveis, a interface
clássica fica disponível no próprio executável adicional. Uma falha de processo
do WebView2 libera notas virtuais e reexibe os controles clássicos, sem recriar
o AudioHost. Essa recuperação está implementada, mas exige teste no Windows.

## Compilar e instalar os arquivos

Requer Windows x64, CMake, compilador C++, PortAudio 19.7.0 e SDK ASIO usados
pelo host atual. O novo alvo usa o SDK WebView2 1.0.2903.40, com download e hash
fixados no CMake; pode-se informar uma cópia extraída por `LP_WEBVIEW2_SDK_ROOT`.

```powershell
./scripts/build_vision_windows.ps1 -PortAudioSource C:/deps/portaudio -AsioSdkRoot C:/deps/asiosdk
```

O script compila os dois aplicativos localmente. Não dispara o Actions e não
gera/upload um portátil de teste automaticamente. A pasta de saída do novo alvo
recebe `vision-ui`, `WebView2Loader.dll` e os recursos recuperados de áudio.
Para SF2, também é necessário manter o conjunto completo de DLLs FluidSynth do
portátil atual. Bancos do usuário permanecem nos locais já reconhecidos pelo host.

**O Microsoft Edge WebView2 Runtime deve estar instalado no Windows.** O loader
copiado não inclui o runtime. Não foi criado instalador do runtime nem pacote
Fixed Version nesta etapa; logo, não se deve anunciar um portátil independente
de qualquer pré-requisito.

Para regenerar os recursos a partir do instalador original:

```sh
python scripts/prepare_vision_assets.py /caminho/Vision-Laiberry-Instruments-2.1-ASIO-Setup-Windows-x64.exe
```

## Validação realizada nesta etapa

1. **8/8 testes CTest passaram em Linux, build Debug**: perfis e favoritos;
   motor/MIDI; DSP; AudioHost com PortAudio de teste; catálogo; Performance/MIDI;
   estado da interface; novo protocolo Vision. O protocolo também compilou e
   passou com `-Wall -Wextra -Werror`.
2. **Renderer passou em Chromium headless** com estado de teste e ponte simulada:
   duas resoluções (1360×900 e 1024×600), grade de 16 categorias, setas/Enter,
   carregamento comandado, Espaço/favoritos, posições 1 e 20 no MIDI Learn,
   E/ESC, digitação em campos, 16 parâmetros, seletores ASIO/MIDI, ASIO Control,
   Performance, comandos Abrir/Salvar e liberação de notas do teclado virtual.
   Nenhum erro JavaScript ou pedido de recurso externo foi observado. Os nomes
   contendo aspas e tags foram exibidos literalmente, sem interpretação HTML.
3. **Compilação cruzada Windows x64 concluída localmente** para os dois aplicativos,
   com PortAudio/ASIO reais, recursos Win32 e SDK oficial WebView2. Também foi
   compilado `lp_win32_vision_test`, que exercita o dispatcher real com dublês de
   áudio. Esse executável de teste **não foi executado no Windows** nesta sessão.
4. Recursos visuais foram inspecionados em capturas das telas renderizadas;
   clipping de instrumentos foi corrigido após essa inspeção.

Os testes de renderer usam um estado simulado, não constituem teste end-to-end
do WebView2 ligado ao host Windows. A compilação cruzada não certifica ASIO4ALL,
TASCAM, Roland, latência, reconexão, foco do Narrador ou estabilidade de palco.
Ainda é necessário um teste do aplicativo integrado no Windows com driver e
controlador reais antes de substituir a versão de uso.

## Uso do GitHub

O saldo autenticado de uso/quota permanece **desconhecido**: a conexão não fornece
essa consulta. Nenhuma execução hospedada foi iniciada para esta etapa. O código
deve ser preservado em uma única publicação da etapa validada, numa branch
separada, cujos filtros atuais de workflow não disparam Actions automaticamente.
