# Inventário de recuperação dos instaladores (20/10/2026)

## Arquivos inspecionados
- Live-Performance-by-Hugo-Sodre-Instalador-Windows-x64-Teste(1).exe — 29.442.742 bytes — SHA-256 `f1efad01f6ad55a6b09322cd96eed8c92add6cc1ad5285c46386811f5d91a2a9`.
- Vision-SFZ-Native-Prototipo-0.8-Windows-x64.exe (extraído do ZIP) — 21.568.321 bytes — SHA-256 `88a101e5364b304ad0f0db36e6519f2f643a863a4f49c720fe0a69455cf347b7`.

## Recuperação comprovada
- O ZIP anterior foi descompactado, recuperando seu instalador EXE.
- Ambos os EXEs são instaladores Windows NSIS; não são o código-fonte original.
- Varredura de strings do instalador Live Performance recuperou texto descritivo de recursos e testes, incluindo:
  - `--favorites-program-selftest`, `--midi-mapping-selftest`, `--sfz-recovery-selftest`, `--sfz-selftest`;
  - `%APPDATA%\\Vision SFZ Native\\settings.ini` e `%APPDATA%\\Vision SFZ Native\\diagnostico.txt`;
  - interface Win32 nativa, MIDI WinMM, motor sfizz carregado por DLL, favoritos e perfis locais;
  - teclas ESC, Espaço e E; 14 controles; carregamento SFZ com validação prévia e preservação do timbre anterior quando falha;
  - referências a ASIO e WASAPI, PortAudio e API de sfizz;
  - instruções de teste e avisos de recursos ainda não homologados em hardware real.
- Foram extraídas **strings legíveis**, não os arquivos internos do instalador nem código-fonte compilável.

## Cautelas importantes
- Algumas strings podem ser documentação embarcada, código de bibliotecas de terceiros ou material de versões anteriores; **não demonstram que a função esteja implementada e funcionando na última versão**.
- A extração do payload NSIS continua pendente: o extrator libarchive falhou e o ambiente não dispõe de 7-Zip; tentativa de instalar ferramenta por apt expirou.
- Não publicar executáveis/DLLs ou amostras SFZ sem inventário e verificação de licença; não executar binários desconhecidos neste ambiente.
- O repositório novo é público: evitar publicar configurações pessoais, diagnósticos privados, credenciais ou arquivos de usuário.

## Plano de reconstrução
1. Extrair payloads com 7-Zip ou NSIS em ambiente Windows isolado.
2. Inventariar e calcular hashes de EXEs, DLLs, imagens, bibliotecas SFZ, presets e configs.
3. Separar artefatos próprios e componentes redistribuíveis de terceiros.
4. Criar projeto-fonte editável para o host Win32 + sfizz + PortAudio/ASIO + WinMM MIDI, com testes de regressão para as funcionalidades documentadas.
5. Migrar apenas recursos permitidos e corrigir somente o que os testes revelarem.

**Status: documentação e reconhecimento binário parciais publicados; payload interno ainda não extraído e código-fonte ainda não reconstruído.**
