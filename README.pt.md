# ChaCha20-Poly1305 AEAD

Uma implementação ChaCha20-Poly1305 AEAD de desempenho médio, feita por diversão e aprendizado

---
## Compilação

### Windows (MSVC/PowerShell)
```
powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Linux

```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

---

## Funcionalidades

- Cifra ChaCha20: Implementação completa, seguindo rigorosamente o RFC 8439.
- ChaCha20-Poly1305 AEAD: Construção de Criptografia Autenticada com Dados Associados (AEAD) para comunicação segura.
- Benchmarking de Alta Precisão: Medição de desempenho usando Ciclos Por Byte (CPB) via RDTSCP e serialização com LFENCE.
- Análise detalhada de throughput (MB/s) com métricas de Média, Melhor e Pior caso.
- Análise estatística incluindo Intervalo Interquartil (IQR) para filtrar ruído e jitter do sistema.
- Arquitetura Moderna em C++:
- Design orientado a objetos com uma API limpa para fácil integração.
- Aderência estrita a vetores de teste para 100% de correção criptográfica.
- Gerenciamento de memória eficiente usando std::vector e buffers de raw pointers para potencial zero-copy, juntamente com travamento e limpeza de memória para segurança.
- Build multiplataforma: Suporte nativo para Windows (MSVC) e Linux (GCC/Clang) via CMake.

---

## Notas de segurança

- Não auditado para tempo constante
- Não reforçado contra canais laterais
- Não destinado a criptografia em produção
- Educacional / experimental

---

## Notas de design

- Segue o layout de estado e a estrutura de quarter-round do RFC 8439
- Testado contra todos os vetores fornecidos no apêndice A do RFC 8439
- Funciona apenas em CPUs little-endian e que suportam SSE

---

## Teste de desempenho(traduzido)

```
Aquecimento pronto

=======================================================
 Teste de performance ChaCha20
=======================================================
[ RENDIMENTO ]
  Melhor:                         715.7812MB/s
  Pior:                        381.3228MB/s
  Médio:                      643.4753MB/s
  Amplitude:                    334.4584MB/s
  IQR:                           60.7639MB/s

[ TEMPO (segundos) ]
  Menor:                       0.0112s
  Maior:                        0.0210s
  Médio:                        0.0126s
  Amplitude:                      0.0098s
  IQR:                            0.0011s
[ EFICIÊNCIA ]
  CPB Médio:                    3.7629 c/B
=======================================================

Criptografia/Descriptografia batem

=======================================================
 Metricas de Criptografia ChaCha20-Poly1305
=======================================================
[ RENDIMENTO ]
  Melhor:                         471.1647MB/s
  Pior:                        323.5539MB/s
  Médio:                      416.6988MB/s
  Amplitude:                    147.6108MB/s
  IQR:                           36.5842MB/s

[ TEMPO (segundos) ]
  Menor:                       0.0170s
  Maior:                        0.0247s
  Médio:                        0.0193s
  Amplitude:                      0.0077s
  IQR:                            0.0017s
[ EFICIÊNCIA ]
  CPB Médio:                    5.7568 c/B
=======================================================


=======================================================
 Métricas de Descriptografia ChaCha20-Poly1305 
=======================================================
[ RENDIMENTO ]
  Melhor:                         468.6173MB/s
  Pior:                        352.4446MB/s
  Médio:                      422.7914MB/s
  Amplitude:                    116.1726MB/s
  IQR:                           48.2270MB/s

[ TEMPO (segundos) ]
  Menor:                       0.0171s
  Maior:                        0.0227s
  Médio:                        0.0190s
  Amplitude:                      0.0056s
  IQR:                            0.0022s
[ EFICIÊNCIA ]
  CPB Médio:                    5.6612 c/B
=======================================================
```
