# Performance (tecla P) — contrato de implementação
Status: projeto e modelo de zonas adicionados; motor de quatro vozes, UI, salvamento e testes Windows ainda pendentes. Não anunciar como integrado.
- P alterna tela Performance, E continua painel de parâmetros, ESC volta à tela principal.
- Camada: 1 a 4 instrumentos simultâneos cobrindo o teclado inteiro.
- Dividir: 2, 3 ou 4 zonas contíguas pré-definidas, com limites ajustáveis e no máximo quatro instrumentos totais. Distribuições aceitas: 1+1, 2+2, 3+1, 2+1+1, 1+1+1+1.
- Cada parte tem volume, ADSR, efeitos, EQ, faixa, transposição e instrumento SFZ ou SF2 independentes.
- Nomear e salvar uma Performance; Favoritos 1–20 aceitam instrumento ou Performance e mantêm Program Change 1–20.
- Notas MIDI só entram nas partes cuja zona as contém; Note Off deve chegar à mesma parte e nota transposta da nota iniciada, mesmo após mudanças de zona.
- Sustain, pitch bend, modulation e panic propagados para todas as partes relevantes; evitar notas presas.
- Renderizar quatro sintetizadores em buffers de áudio pré-alocados, misturar com headroom e sem alocação no callback ASIO; nunca carregar samples dentro do callback.
- Troca de Performance: preparar instrumentos fora da thread de áudio; preservar caudas quando viável sem bloquear callback.
- Preservar compatibilidade dos perfis .lphs antigos; novo formato deve suportar performances nomeadas e mapeamentos CC individuais.
- Testar combinações SFZ+SF2, splits, layers, pedal, mudanças de favorito, CPU, xruns e reabertura do programa.
- Não executar GitHub Actions a cada edição; compilar em lote ao fim da implementação.
