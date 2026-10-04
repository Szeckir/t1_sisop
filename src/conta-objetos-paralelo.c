/* contagem paralela de objetos (conectividade 8) com pthreads.
 * cada thread rotula uma faixa de linhas; a thread principal une as fronteiras com union-find. */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "matriz.h"

static const int DI[8] = {-1, -1, -1,  0, 0,  1, 1, 1};
static const int DJ[8] = {-1,  0,  1, -1, 1, -1, 0, 1};

typedef struct {
    const int *m;
    int L;
    int C;
    int linha_ini;
    int linha_fim;       
    int *rotulos;        /* compartilhado; cada thread escreve so na sua faixa */
    int *pai;            /* union-find; cada thread inicializa os rotulos que cria */
    int componentes;
    int erro;
} Trabalho;

static double agora_segundos(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (double) ts.tv_sec + (double) ts.tv_nsec / 1000000000.0;
}

/* flood fill restrito a faixa [linha_ini, linha_fim); o rotulo inicio + 1 e unico na matriz.
 * recebe os campos como parametros para o compilador nao reler a struct a cada vizinho. */
static void rotula_regiao_local(const int *m, int *rotulos, int C,
                                int linha_ini, int linha_fim,
                                int inicio, int *pilha)
{
    int topo = 0;
    int rotulo = inicio + 1;

    rotulos[inicio] = rotulo;
    pilha[topo++] = inicio;

    while (topo > 0) {
        int atual = pilha[--topo];
        int i = atual / C;
        int j = atual % C;
        int k;

        for (k = 0; k < 8; k++) {
            int ni = i + DI[k];
            int nj = j + DJ[k];
            int idx;

            /* nao sai da faixa: a uniao entre faixas e feita depois */
            if (ni < linha_ini || ni >= linha_fim || nj < 0 || nj >= C) {
                continue;
            }

            idx = ni * C + nj;
            if (m[idx] == 1 && rotulos[idx] == 0) {
                rotulos[idx] = rotulo;
                pilha[topo++] = idx;
            }
        }
    }
}

static void *trabalhador(void *arg)
{
    Trabalho *t = (Trabalho *) arg;
    const int *m = t->m;
    int *rotulos = t->rotulos;
    int *pai = t->pai;
    int C = t->C;
    int linha_ini = t->linha_ini;
    int linha_fim = t->linha_fim;
    int componentes = 0;
    int capacidade;
    int *pilha;
    int i;
    int j;

    capacidade = (linha_fim - linha_ini) * C;
    pilha = (int *) malloc(capacidade * sizeof(int));
    if (pilha == NULL) {
        t->erro = 1;
        return NULL;
    }

    for (i = linha_ini; i < linha_fim; i++) {
        for (j = 0; j < C; j++) {
            int idx = i * C + j;
            if (m[idx] == 1 && rotulos[idx] == 0) {
                /* idx + 1 pertence a esta faixa: cada thread escreve posicoes disjuntas de pai */
                pai[idx + 1] = idx + 1;
                rotula_regiao_local(m, rotulos, C, linha_ini, linha_fim,
                                    idx, pilha);
                componentes++;
            }
        }
    }

    /* contador local evita false sharing entre structs vizinhas no vetor trab */
    t->componentes = componentes;

    free(pilha);
    return NULL;
}

/* union-find com compressao de caminho */
static int uf_find(int *pai, int x)
{
    int raiz = x;
    while (pai[raiz] != raiz) {
        raiz = pai[raiz];
    }
    while (pai[x] != x) {
        int prox = pai[x];
        pai[x] = raiz;
        x = prox;
    }
    return raiz;
}

/* uniao por rank; retorna 1 se juntou dois conjuntos distintos, 0 se ja eram o mesmo */
static int uf_union(int *pai, unsigned char *rank, int a, int b)
{
    int ra;
    int rb;

    ra = uf_find(pai, a);
    rb = uf_find(pai, b);
    if (ra == rb) {
        return 0;
    }

    if (rank[ra] < rank[rb]) {
        pai[ra] = rb;
    } else if (rank[ra] > rank[rb]) {
        pai[rb] = ra;
    } else {
        pai[rb] = ra;
        rank[ra]++;
    }
    return 1;
}

/* liga a ultima linha de cada faixa com c-1, c e c+1 da primeira linha da seguinte.
 * retorna o numero de unioes efetivas. */
static int consolida_fronteiras(const int *m, int L, int C,
                                int nthreads, const Trabalho *trab,
                                int *rotulos, int *pai,
                                unsigned char *rank)
{
    int unioes = 0;
    int t;
    (void) L;

    for (t = 0; t < nthreads - 1; t++) {
        int sup = trab[t].linha_fim - 1;
        int inf = trab[t + 1].linha_ini;
        int c;

        for (c = 0; c < C; c++) {
            int idx_sup = sup * C + c;
            int dc;

            if (m[idx_sup] == 0) {
                continue;
            }

            for (dc = -1; dc <= 1; dc++) {
                int nc = c + dc;
                int idx_inf;
                int a;
                int b;

                if (nc < 0 || nc >= C) {
                    continue;
                }
                idx_inf = inf * C + nc;
                if (m[idx_inf] == 0) {
                    continue;
                }

                a = rotulos[idx_sup];
                b = rotulos[idx_inf];
                if (a != 0 && b != 0 && a != b) {
                    unioes += uf_union(pai, rank, a, b);
                }
            }
        }
    }
    return unioes;
}

/* retorna o numero de objetos ou -1 em erro */
static int conta_objetos_paralelo(const int *m, int L, int C, int nthreads)
{
    pthread_t *threads;
    Trabalho *trab;
    int *rotulos;
    int *pai;
    unsigned char *rank;
    int total;
    int base;
    int resto;
    int linha = 0;
    int criadas = 0;
    int erro = 0;
    int objetos = 0;
    int i;

    if (m == NULL || L <= 0 || C <= 0 || nthreads < 2) {
        return -1;
    }
    /* evita faixas vazias */
    if (nthreads > L) {
        nthreads = L;
    }
    if (nthreads < 2) {
        return -1;
    }

    total = L * C;
    threads = (pthread_t *) malloc(nthreads * sizeof(pthread_t));
    trab = (Trabalho *) calloc(nthreads, sizeof(Trabalho));
    rotulos = (int *) calloc(total, sizeof(int));
    /* pai nao e inicializado aqui: cada thread grava pai[r] = r ao criar o rotulo r */
    pai = (int *) malloc((total + 1) * sizeof(int));
    rank = (unsigned char *) calloc(total + 1, sizeof(unsigned char));

    if (threads == NULL || trab == NULL || rotulos == NULL || pai == NULL ||
        rank == NULL) {
        fprintf(stderr, "paralelo: memoria insuficiente\n");
        free(threads); free(trab); free(rotulos); free(pai); free(rank);
        return -1;
    }

    base = L / nthreads;
    resto = L % nthreads;

    for (i = 0; i < nthreads; i++) {
        int qtd = base + ((i < resto) ? 1 : 0);
        int rc;

        trab[i].m = m;
        trab[i].L = L;
        trab[i].C = C;
        trab[i].linha_ini = linha;
        trab[i].linha_fim = linha + qtd;
        trab[i].rotulos = rotulos;
        trab[i].pai = pai;
        linha += qtd;

        rc = pthread_create(&threads[i], NULL, trabalhador, &trab[i]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create falhou para thread %d (codigo %d)\n", i, rc);
            erro = 1;
            break;
        }
        criadas++;
    }

    /* join como barreira: a consolidacao so comeca depois que todas as threads terminam */
    for (i = 0; i < criadas; i++) {
        int rc = pthread_join(threads[i], NULL);
        if (rc != 0) {
            fprintf(stderr, "pthread_join falhou para thread %d (codigo %d)\n", i, rc);
            erro = 1;
        }
        if (trab[i].erro) {
            fprintf(stderr, "thread %d: memoria insuficiente\n", i);
            erro = 1;
        }
    }

    if (!erro && criadas == nthreads) {
        /* cada uniao efetiva junta dois componentes locais do mesmo objeto:
         * objetos = soma dos componentes locais - unioes, sem varrer a matriz de novo */
        for (i = 0; i < nthreads; i++) {
            objetos += trab[i].componentes;
        }
        objetos -= consolida_fronteiras(m, L, C, nthreads, trab,
                                        rotulos, pai, rank);
    } else {
        objetos = -1;
    }

    free(threads);
    free(trab);
    free(rotulos);
    free(pai);
    free(rank);
    return objetos;
}

typedef struct {
    const char *arquivo;
    int esperado;
} CasoTeste;

#define NUM_CASOS 5
static const CasoTeste CASOS[NUM_CASOS] = {
    {"tests/obrigatorios/ex1.txt", 3}, {"tests/obrigatorios/ex2.txt", 4},
    {"tests/obrigatorios/ex3.txt", 5}, {"tests/obrigatorios/ex4.txt", 6},
    {"tests/obrigatorios/ex5.txt", 7}
};

static void uso(const char *prog)
{
    fprintf(stderr,
        "uso:\n"
        "  %s <threads>                         roda as 5 matrizes obrigatorias\n"
        "  %s <threads> <arquivo>               roda uma matriz de arquivo\n"
        "  %s <threads> -g <L> <C> <dens%%> <semente>  gera matriz e roda\n",
        prog, prog, prog);
}

static int roda(const char *rotulo, const int *m, int L, int C,
                int esperado, int nthreads)
{
    double t0;
    double t1;
    int obtido;

    t0 = agora_segundos();
    obtido = conta_objetos_paralelo(m, L, C, nthreads);
    t1 = agora_segundos();

    if (obtido < 0) {
        printf("%-28s  ERRO\n", rotulo);
        return 1;
    }
    if (esperado < 0) {
        printf("%-28s %8s %8d %14.6f\n", rotulo, "-", obtido, t1 - t0);
        return 0;
    }
    printf("%-28s %8d %8d %14.6f   %s\n", rotulo, esperado, obtido,
           t1 - t0, (obtido == esperado) ? "OK" : "FALHOU");
    return (obtido == esperado) ? 0 : 1;
}

int main(int argc, char *argv[])
{
    int nthreads;
    int *m;
    int L;
    int C;
    int falhas = 0;
    int i;

    if (argc < 2) {
        uso(argv[0]);
        return EXIT_FAILURE;
    }
    nthreads = atoi(argv[1]);
    if (nthreads < 2) {
        fprintf(stderr, "a versao paralela exige pelo menos 2 threads\n");
        return EXIT_FAILURE;
    }

    printf("=== Contagem de objetos - versao PARALELA (%d Pthreads) ===\n\n", nthreads);
    printf("%-28s %8s %8s %14s   %s\n", "Matriz", "Esperado", "Obtido", "Tempo (s)", "Status");
    printf("-----------------------------------------------------------------------------\n");

    if (argc == 2) {
        for (i = 0; i < NUM_CASOS; i++) {
            m = matriz_ler(CASOS[i].arquivo, &L, &C);
            if (m == NULL) {
                falhas++;
                continue;
            }
            falhas += roda(CASOS[i].arquivo, m, L, C, CASOS[i].esperado, nthreads);
            free(m);
        }
    } else if (argc == 3) {
        m = matriz_ler(argv[2], &L, &C);
        if (m == NULL) return EXIT_FAILURE;
        falhas += roda(argv[2], m, L, C, -1, nthreads);
        free(m);
    } else if (argc == 7 && strcmp(argv[2], "-g") == 0) {
        int dens;
        unsigned int seed;
        L = atoi(argv[3]); C = atoi(argv[4]); dens = atoi(argv[5]);
        seed = (unsigned int) strtoul(argv[6], NULL, 10);
        m = matriz_aleatoria(L, C, dens, seed);
        if (m == NULL) return EXIT_FAILURE;
        falhas += roda("matriz gerada", m, L, C, -1, nthreads);
        free(m);
    } else {
        uso(argv[0]);
        return EXIT_FAILURE;
    }

    return (falhas == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
