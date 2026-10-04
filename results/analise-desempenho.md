# Analise de desempenho

## Metodo

- **Maquina:** Intel Core i7-14700HX (20 nucleos: 8 de desempenho + 12 de eficiencia, 28 threads logicas), 15,7 GB de RAM, Windows 11 Pro com Ubuntu 26.04 LTS no WSL2 (kernel 6.6.114.1), GCC 15.2.0, flags `-std=c89 -Wall -Wextra -pedantic -O2`.
- **Dados:** matriz pseudoaleatoria de 4000 x 4000 (16 milhoes de celulas), densidade de 35% e semente 2026. A semente fixa faz com que todas as configuracoes processem exatamente os mesmos dados; todas as execucoes encontraram 489760 objetos.
- **Trecho medido:** somente a rotina de contagem, com `clock_gettime(CLOCK_MONOTONIC)`. Geracao da matriz e impressao ficam fora da medicao.
- **Repeticoes:** 5 por configuracao, intercaladas (em cada repeticao rodam todas as configuracoes, uma apos a outra). O valor representativo e a **mediana**, que reduz a influencia de variacoes ocasionais do sistema.
- **Configuracoes:** sequencial; paralela atual com 2, 4, 8 e 16 threads; e, para comparacao, a versao paralela anterior (v1, antes da otimizacao descrita abaixo) com as mesmas quantidades de threads.
- **Reproducao:** `make benchmark` gera `results/benchmark.txt` (saida bruta) e `results/medicoes.csv` (uma linha por execucao); `make graficos` gera os graficos a partir do CSV. A comparacao com a v1 foi feita com `PARALELO_ANTERIOR=<binario da v1> sh scripts/benchmark.sh`.

Formulas: `S(p) = Tsequencial / Tparalelo(p)` e `E(p) = S(p) / p`.

## Resultados - matriz 4000 x 4000

| Versao | Threads | Mediana (ms) | Desvio-padrao (ms) | Aceleracao S(p) | Eficiencia E(p) |
|---|---:|---:|---:|---:|---:|
| Sequencial | 1 | 234,624 | 13,482 | 1,000 | 1,000 |
| Paralela (atual) | 2 | 169,428 | 5,563 | 1,385 | 0,692 |
| Paralela (atual) | 4 | 93,666 | 5,704 | 2,505 | 0,626 |
| Paralela (atual) | 8 | 62,826 | 3,329 | 3,735 | 0,467 |
| Paralela (atual) | 16 | 52,127 | 6,015 | 4,501 | 0,281 |

![Tempo de execucao](grafico-tempo.svg)

![Aceleracao](grafico-aceleracao.svg)

![Eficiencia](grafico-eficiencia.svg)

## Comparacao com a versao anterior (v1)

A primeira versao paralela fazia, depois do `pthread_join`, duas etapas **sequenciais do tamanho da matriz inteira**: inicializava o vetor `pai` do Union-Find (16 milhoes de posicoes) e percorria de novo todas as celulas para contar as raizes distintas. Pela Lei de Amdahl, essa parte sequencial limitava a aceleracao, por mais threads que se usassem.

A versao atual elimina as duas etapas:

- cada thread inicializa no `pai` apenas os rotulos que ela cria (posicoes disjuntas, sem condicao de corrida);
- a contagem final vira `objetos = soma dos componentes locais - unioes efetivas`, sem varrer a matriz de novo;
- cada thread conta seus componentes em variavel local e so grava o total na sua struct no fim, evitando *false sharing* entre as structs vizinhas no vetor `trab`; os demais campos da struct sao lidos uma vez so, em vez de a cada vizinho testado.

| Threads | v1 mediana (ms) | v1 S(p) | Atual mediana (ms) | Atual S(p) | Ganho da atual sobre a v1 |
|---:|---:|---:|---:|---:|---:|
| 2 | 233,466 | 1,005 | 169,428 | 1,385 | 1,38x |
| 4 | 161,433 | 1,453 | 93,666 | 2,505 | 1,72x |
| 8 | 133,500 | 1,757 | 62,826 | 3,735 | 2,13x |
| 16 | 122,051 | 1,922 | 52,127 | 4,501 | 2,34x |

O ganho cresce com o numero de threads, como esperado: quanto mais threads, menor o tempo da parte paralela e maior o peso relativo da parte sequencial que foi removida. Com 2 threads, a v1 praticamente empatava com o sequencial (S = 1,005).

## Matrizes pequenas x grandes

Mesma densidade e semente, sequencial contra 4 threads (mediana de 5 repeticoes):

| Matriz | Celulas | Sequencial (ms) | Paralela 4 threads (ms) | S |
|---|---:|---:|---:|---:|
| 12 x 12 | 144 | 0,003 | 0,351 | 0,01 |
| 100 x 100 | 10 mil | 0,185 | 0,283 | 0,65 |
| 316 x 316 | 100 mil | 1,494 | 0,940 | 1,59 |
| 1000 x 1000 | 1 milhao | 14,141 | 5,804 | 2,44 |
| 2000 x 2000 | 4 milhoes | 54,892 | 23,241 | 2,36 |
| 4000 x 4000 | 16 milhoes | 221,577 | 90,990 | 2,44 |

![Tempo x tamanho](grafico-escala.svg)

Nas matrizes obrigatorias (no maximo 144 celulas) a versao paralela e **mais lenta** que a sequencial: contar 144 celulas leva poucos microssegundos, enquanto criar e aguardar as threads (`pthread_create`/`pthread_join`) e alocar as estruturas custa algumas centenas de microssegundos. O tempo paralelo nesses casos e quase todo overhead. O paralelismo so passa a compensar entre 10 mil e 100 mil celulas, e a partir de 1 milhao de celulas a aceleracao com 4 threads se estabiliza em torno de 2,4.

## Por que a aceleracao nao e linear

Mesmo na versao atual, S(16) = 4,5 esta longe do ideal 16. As causas principais:

1. **Parte sequencial restante (Lei de Amdahl):** alocacao dos vetores de rotulos e do Union-Find, criacao e espera das threads e a consolidacao das `T - 1` fronteiras continuam sequenciais.
2. **Largura de banda de memoria:** o flood fill faz pouco calculo por celula e muitos acessos a memoria (matriz de `int` e vetor de rotulos, 8 bytes por celula). Com muitas threads, o gargalo passa a ser a memoria, e nao os nucleos.
3. **Nucleos heterogeneos e divisao estatica:** o i7-14700HX tem nucleos de desempenho e de eficiencia. Como cada thread recebe o mesmo numero de linhas, uma thread que cai num nucleo mais lento termina depois, e o `pthread_join` espera pela mais lenta.
4. **Hyper-threading:** acima de 8 threads, parte delas divide o mesmo nucleo fisico com outra thread, o que rende menos que um nucleo inteiro.

Esses numeros dependem do hardware, da carga do sistema e do compilador. Para a apresentacao, recomenda-se rodar `make benchmark` e `make graficos` na maquina da demonstracao.
