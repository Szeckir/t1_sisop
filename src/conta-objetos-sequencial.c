/* contagem sequencial de objetos (conectividade 8) com um unico fluxo de controle.
 * referencia de correcao e de desempenho para a versao paralela. */
#define _POSIX_C_SOURCE 200809L 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "matriz.h"

/* deslocamentos dos 8 vizinhos: as 4 direcoes retas e as 4 diagonais */
static const int DI[8] = {-1, -1, -1,  0, 0,  1, 1, 1};
static const int DJ[8] = {-1,  0,  1, -1, 1, -1, 0, 1};

/* relogio monotonico: nao volta no tempo se o relogio do sistema for ajustado */
static double agora_segundos(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (double) ts.tv_sec + (double) ts.tv_nsec / 1000000000.0;
}

/* flood fill iterativo a partir de 'inicio'; pilha no heap evita estouro em objetos grandes.
 * retorna o numero de celulas do objeto. */
static int preenche_regiao(const int *m, int L, int C,
                           char *visitado, int *pilha, int inicio)
{
    int tamanho = 0;
    int topo = 0;

    /* marca ao empilhar, nao ao desempilhar: nenhuma celula entra duas vezes na pilha */
    visitado[inicio] = 1;
    pilha[topo++] = inicio;

    while (topo > 0) {
        int atual;
        int i;
        int j;
        int k;

        atual = pilha[--topo];
        tamanho++;

        i = atual / C;
        j = atual % C;

        for (k = 0; k < 8; k++) {
            int ni = i + DI[k];
            int nj = j + DJ[k];
            int idx;

            /* ignora vizinhos fora da matriz */
            if (ni < 0 || ni >= L || nj < 0 || nj >= C) {
                continue;
            }

            idx = ni * C + nj;

            if (m[idx] == 1 && visitado[idx] == 0) {
                visitado[idx] = 1;
                pilha[topo++] = idx;
            }
        }
    }

    return tamanho;
}

/* cada 1 ainda nao visitado inicia um objeto novo; o flood fill marca o resto dele.
 * retorna a contagem ou -1 em erro de alocacao. */
int conta_objetos(const int *m, int L, int C)
{
    char *visitado;
    int *pilha;
    int total;
    int i;
    int objetos = 0;

    if (m == NULL) {
        return -1;
    }

    total = L * C;

    visitado = (char *) calloc(total, sizeof(char));
    if (visitado == NULL) {
        fprintf(stderr, "conta_objetos: sem memoria para o mapa de visitados\n");
        return -1;
    }

    /* pior caso: a matriz inteira e um unico objeto */
    pilha = (int *) malloc(total * sizeof(int));
    if (pilha == NULL) {
        fprintf(stderr, "conta_objetos: sem memoria para a pilha\n");
        free(visitado);
        return -1;
    }

    for (i = 0; i < total; i++) {
        if (m[i] == 1 && visitado[i] == 0) {
            objetos++;
            preenche_regiao(m, L, C, visitado, pilha, i);
        }
    }

    free(visitado);
    free(pilha);

    return objetos;
}

typedef struct {
    const char *arquivo;
    int esperado;
} CasoTeste;

#define NUM_CASOS 5

static const CasoTeste CASOS[NUM_CASOS] = {
    {"tests/obrigatorios/ex1.txt", 3},
    {"tests/obrigatorios/ex2.txt", 4},
    {"tests/obrigatorios/ex3.txt", 5},
    {"tests/obrigatorios/ex4.txt", 6},
    {"tests/obrigatorios/ex5.txt", 7}
};

static void uso(const char *prog)
{
    fprintf(stderr,
        "uso:\n"
        "  %s                                   roda as 5 matrizes obrigatorias\n"
        "  %s <arquivo>                         roda uma matriz de arquivo\n"
        "  %s -g <L> <C> <dens%%> <semente>       gera matriz e roda\n",
        prog, prog, prog);
}

/* esperado < 0 significa sem gabarito; retorna 1 se o resultado divergiu */
static int roda(const char *rotulo, const int *m, int L, int C, int esperado)
{
    double t0;
    double t1;
    int obtido;

    t0 = agora_segundos();
    obtido = conta_objetos(m, L, C);
    t1 = agora_segundos();

    if (obtido < 0) {
        printf("%-28s  ERRO de alocacao\n", rotulo);
        return 1;
    }

    if (esperado < 0) {
        printf("%-28s %8s %8d %14.6f\n", rotulo, "-", obtido, t1 - t0);
        return 0;
    }

    printf("%-28s %8d %8d %14.6f   %s\n",
           rotulo, esperado, obtido, t1 - t0,
           (obtido == esperado) ? "OK" : "FALHOU");

    return (obtido == esperado) ? 0 : 1;
}

int main(int argc, char *argv[])
{
    int *m;
    int L;
    int C;
    int falhas = 0;
    int i;

    printf("=== Contagem de objetos - versao SEQUENCIAL (conectividade 8) ===\n\n");
    printf("%-28s %8s %8s %14s   %s\n",
           "Matriz", "Esperado", "Obtido", "Tempo (s)", "Status");
    printf("-----------------------------------------------------------------------------\n");

    /* sem argumentos: as cinco matrizes obrigatorias */
    if (argc == 1) {
        for (i = 0; i < NUM_CASOS; i++) {
            m = matriz_ler(CASOS[i].arquivo, &L, &C);
            if (m == NULL) {
                fprintf(stderr, "\nnao consegui ler %s "
                        "(rode a partir da raiz do projeto)\n", CASOS[i].arquivo);
                return EXIT_FAILURE;
            }
            falhas += roda(CASOS[i].arquivo, m, L, C, CASOS[i].esperado);
            free(m);
        }
    }
    /* -g: matriz gerada com semente fixa */
    else if (argc == 6 && strcmp(argv[1], "-g") == 0) {
        char rotulo[64];
        int dens;
        unsigned int semente;

        L = atoi(argv[2]);
        C = atoi(argv[3]);
        dens = atoi(argv[4]);
        semente = (unsigned int) strtoul(argv[5], NULL, 10);

        m = matriz_aleatoria(L, C, dens, semente);
        if (m == NULL) {
            return EXIT_FAILURE;
        }
        sprintf(rotulo, "aleatoria %dx%d d=%d%%", L, C, dens);
        falhas += roda(rotulo, m, L, C, -1);
        free(m);
    }
    /* um arquivo avulso, sem gabarito */
    else if (argc == 2) {
        m = matriz_ler(argv[1], &L, &C);
        if (m == NULL) {
            return EXIT_FAILURE;
        }
        falhas += roda(argv[1], m, L, C, -1);
        free(m);
    }
    else {
        uso(argv[0]);
        return EXIT_FAILURE;
    }

    printf("-----------------------------------------------------------------------------\n");

    if (falhas == 0) {
        printf("\nSem divergencias.\n");
        return EXIT_SUCCESS;
    }

    printf("\n%d caso(s) com resultado incorreto.\n", falhas);
    return EXIT_FAILURE;
}
