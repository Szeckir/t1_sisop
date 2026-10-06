# T1 - Contagem paralela de objetos em matriz binaria

**Disciplina:** Sistemas Operacionais
**Professor:** Filipo Mor  
**Autores:** Júlia Bettiol de Oliveira e Thomaz Gomes Szeckir
**Link da Apresentação:** https://youtu.be/fNzSmvmrRsY?is=ooUJYSR2z8N0p5vW

## 1. Objetivo

O projeto conta objetos (componentes conexos de celulas `1`) em matrizes binarias usando **conectividade 8**. Sao entregues duas implementacoes funcionalmente equivalentes:

- `src/conta-objetos-sequencial.c`: referencia sequencial;
- `src/conta-objetos-paralelo.c`: versao paralela com **POSIX Threads (Pthreads)**.

As duas versoes usam flood fill iterativo, evitando recursao excessiva em matrizes grandes.

## 2. Arquitetura da solucao paralela

### Decomposicao

A matriz e dividida em **faixas contiguas de linhas**. Se houver `T` threads, cada thread recebe aproximadamente `L/T` linhas; as linhas restantes sao distribuidas entre as primeiras threads.

Cada thread executa flood fill somente dentro de sua faixa. Isso e trabalho efetivo: as unidades concorrentes percorrem e rotulam partes distintas da matriz simultaneamente.

### Rotulos locais

Cada componente local recebe como identificador `indice_da_celula_inicial + 1`. Como o indice e global na matriz, os identificadores sao unicos mesmo quando foram criados por threads diferentes.

O vetor `rotulos` e compartilhado, mas nao ha duas threads escrevendo na mesma celula: cada thread escreve exclusivamente nas linhas que recebeu. Por isso, a fase local nao exige mutex e nao possui condicao de corrida sobre esse vetor.

### Sincronizacao

A thread principal cria os trabalhadores com `pthread_create()` e aguarda todos com `pthread_join()`. O `join` funciona como barreira: a consolidacao so comeca quando todas as faixas terminaram de ser rotuladas.

Nao foi colocado mutex artificialmente na fase local, porque as regioes de escrita sao disjuntas. Cada thread tambem inicializa, no vetor `pai` do Union-Find, apenas os rotulos que ela mesma cria; como o rotulo e `indice + 1` de uma celula da sua faixa, essas posicoes tambem sao disjuntas. As unioes sao executadas pela thread principal apos os `join`, portanto nao existe atualizacao concorrente do Union-Find.

### Consolidacao das fronteiras

Somar as contagens locais seria incorreto, pois um mesmo objeto pode atravessar duas faixas. A solucao usa **Union-Find (Disjoint Set Union)**.

Para cada fronteira entre duas faixas, uma celula `1` da ultima linha da faixa superior e comparada com tres posicoes da primeira linha da faixa inferior:

- diagonal inferior esquerda (`c-1`);
- vertical (`c`);
- diagonal inferior direita (`c+1`).

Assim sao preservadas as conexoes verticais e diagonais da conectividade 8. As conexoes horizontais ja foram resolvidas dentro de cada faixa pelo flood fill local. O caso em que componentes se encontram diagonalmente em uma divisao tambem e tratado pelas comparacoes `c-1` e `c+1`.

Cada thread conta quantos componentes locais encontrou. Cada componente local comeca como um conjunto separado no Union-Find, e cada chamada de `uf_union()` que junta dois conjuntos distintos (uniao efetiva) revela que dois componentes locais eram o mesmo objeto. Portanto:

```text
objetos = soma dos componentes locais - unioes efetivas
```

Com isso a thread principal nao precisa varrer a matriz de novo depois dos `join`: a parte sequencial fica proporcional ao tamanho das fronteiras, `(T - 1) * C`, e nao ao tamanho da matriz, `L * C`.

## 3. Compilacao

Requer Linux ou macOS, compilador C e suporte a Pthreads.

```sh
make
```

Comandos usados pelo Makefile:

```sh
cc -std=c89 -Wall -Wextra -pedantic -O2 -Isrc src/conta-objetos-sequencial.c src/matriz.c -o bin/sequencial
cc -std=c89 -Wall -Wextra -pedantic -O2 -Isrc -pthread src/conta-objetos-paralelo.c src/matriz.c -o bin/paralelo
```

## 4. Execucao

Para ver todas as opcoes de uma vez:

```sh
make help
```

### Cinco matrizes obrigatorias

```sh
./bin/sequencial
./bin/paralelo 2
./bin/paralelo 4
```

Ou execute tudo de uma vez:

```sh
make test
```

### Arquivo especifico

```sh
./bin/sequencial tests/obrigatorios/ex3.txt
./bin/paralelo 4 tests/obrigatorios/ex3.txt
```

### Matriz aleatoria para desempenho

```sh
./bin/sequencial -g 4000 4000 35 2026
./bin/paralelo 2 -g 4000 4000 35 2026
./bin/paralelo 4 -g 4000 4000 35 2026
```

Os quatro numeros sao, respectivamente: linhas, colunas, densidade percentual e semente. A mesma semente garante os mesmos dados nas duas versoes.

## 5. Resultados obrigatorios

| Exemplo | Dimensoes | Esperado | Sequencial | Paralelo 2 threads | Paralelo 4 threads |
|---|---:|---:|---:|---:|---:|
| 1 | 5 x 5 | 3 | 3 | 3 | 3 |
| 2 | 6 x 8 | 4 | 4 | 4 | 4 |
| 3 | 8 x 8 | 5 | 5 | 5 | 5 |
| 4 | 9 x 12 | 6 | 6 | 6 | 6 |
| 5 | 12 x 12 | 7 | 7 | 7 | 7 |

Todas as configuracoes produziram exatamente o resultado esperado.

## 6. Desempenho

O script `scripts/benchmark.sh` executa cinco repeticoes sobre a mesma matriz pseudoaleatoria de 4000 x 4000, com densidade de 35% e semente 2026, usando o sequencial e o paralelo com 2, 4, 8 e 16 threads. Tambem mede matrizes de 12 x 12 ate 4000 x 4000 para mostrar a partir de que tamanho o paralelo compensa.

```sh
make benchmark   # gera results/benchmark.txt e results/medicoes.csv
make graficos    # gera results/grafico-*.svg (Python 3, sem bibliotecas externas)
```

Medianas obtidas em um Intel Core i7-14700HX (Ubuntu no WSL2, GCC 15.2, `-O2`):

| Configuracao | Mediana (ms) | Aceleracao S = Tseq/Tpar | Eficiencia S/p |
|---|---:|---:|---:|
| Sequencial | 234,624 | 1,000 | 1,000 |
| 2 threads | 169,428 | 1,385 | 0,692 |
| 4 threads | 93,666 | 2,505 | 0,626 |
| 8 threads | 62,826 | 3,735 | 0,467 |
| 16 threads | 52,127 | 4,501 | 0,281 |

A primeira versao paralela varria a matriz inteira de novo na thread principal depois dos `join` e, por isso, mal superava o sequencial (S = 1,005 com 2 threads e 1,922 com 16). Remover essa etapa sequencial deixou a versao atual de 1,4x a 2,3x mais rapida que a anterior.

Nas matrizes obrigatorias (ate 144 celulas) a versao paralela e mais lenta que a sequencial: a contagem leva poucos microssegundos, e criar/aguardar as threads custa centenas de microssegundos. O paralelo so compensa a partir de algo entre 10 mil e 100 mil celulas.

A analise completa, com graficos, esta em [`results/analise-desempenho.md`](results/analise-desempenho.md). Os tempos dependem da maquina; recomenda-se medir de novo na maquina da apresentacao.

## 7. Ausencia de condicoes de corrida e deadlock

- cada thread escreve apenas nos rotulos de sua faixa;
- cada thread inicializa no vetor `pai` apenas os rotulos que criou (posicoes disjuntas);
- cada thread conta seus componentes em variavel local e so publica o total no fim;
- a matriz de entrada e somente leitura;
- cada thread possui sua propria pilha de flood fill;
- a consolidacao so ocorre depois de todos os `pthread_join()`;
- as unioes do Union-Find sao feitas apenas pela thread principal;
- nao ha aquisicao de multiplos locks, portanto nao ha ciclo de espera ou deadlock.

Verificado com os detectores de condicao de corrida do Valgrind: `valgrind --tool=helgrind ./bin/paralelo 4 -g 300 300 45 7` e `valgrind --tool=drd ./bin/paralelo 8 -g 200 300 45 9` reportaram 0 erros.

## 8. Tratamento de erros

O projeto verifica alocacoes dinamicas, abertura/leitura de arquivos, `clock_gettime()`, `pthread_create()` e `pthread_join()`. Memoria dinamica e liberada antes da finalizacao: `valgrind --leak-check=full` nas versoes sequencial e paralela reportou 0 erros e "All heap blocks were freed -- no leaks are possible".

## 9. Estrutura

```text
README.md
Makefile
src/
  conta-objetos-sequencial.c
  conta-objetos-paralelo.c
  matriz.c
  matriz.h
tests/
  obrigatorios/   ex1.txt ... ex5.txt (matrizes do enunciado)
  adicionais/     a1_zeros.txt, a2_objeto_unico.txt, a3_diagonal.txt
scripts/
  benchmark.sh    medicoes repetidas (make benchmark)
  graficos.py     graficos SVG a partir do CSV (make graficos)
results/
  benchmark.txt
  medicoes.csv
  analise-desempenho.md
  testes-adicionais.txt
  grafico-tempo.svg, grafico-aceleracao.svg, grafico-eficiencia.svg, grafico-escala.svg
slides/
  apresentacao.pdf
```

## 10. Referencias e ferramentas

- Enunciado do Trabalho Pratico - Processos e Threads, Sistemas Operacionais, PUCRS, 2026/II.
- Material da disciplina sobre processos, Pthreads, `pthread_create`, `pthread_join`, comunicacao e sincronizacao.
- APIs POSIX/Pthreads disponibilizadas pelo sistema operacional.

- ChatGPT (OpenAI): apoio na revisao, documentacao, estruturacao da solucao e testes.
- Claude Code (Anthropic): revisao do codigo frente ao enunciado e otimizacao da contagem final da versao paralela

Nao foram usadas bibliotecas externas para o algoritmo de contagem ou consolidacao.
