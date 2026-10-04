/* alocacao, leitura, geracao e gravacao de matrizes binarias em vetor 1d */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matriz.h"

int *matriz_alocar(int L, int C)
{
    int *m;

    if (L <= 0 || C <= 0) {
        fprintf(stderr, "matriz_alocar: dimensoes invalidas (%d x %d)\n", L, C);
        return NULL;
    }

    m = (int *) malloc(L * C * sizeof(int));

    if (m == NULL) {
        fprintf(stderr, "matriz_alocar: memoria insuficiente para %d x %d\n",
                L, C);
        return NULL;
    }

    /* malloc nao zera a memoria */
    memset(m, 0, L * C * sizeof(int));

    return m;
}

int *matriz_ler(const char *caminho, int *L, int *C)
{
    FILE *f;
    int *m;
    int linhas;
    int colunas;
    int total;
    int i;

    f = fopen(caminho, "r");
    if (f == NULL) {
        perror(caminho);
        return NULL;
    }

    if (fscanf(f, "%d %d", &linhas, &colunas) != 2) {
        fprintf(stderr, "%s: cabecalho invalido (esperado: <linhas> <colunas>)\n",
                caminho);
        fclose(f);
        return NULL;
    }

    m = matriz_alocar(linhas, colunas);
    if (m == NULL) {
        fclose(f);
        return NULL;
    }

    total = linhas * colunas;

    for (i = 0; i < total; i++) {
        int v;

        if (fscanf(f, "%d", &v) != 1) {
            fprintf(stderr, "%s: faltam valores (lidos %d de %d)\n",
                    caminho, i, total);
            free(m);
            fclose(f);
            return NULL;
        }
        if (v != 0 && v != 1) {
            fprintf(stderr, "%s: valor invalido '%d' na posicao %d\n",
                    caminho, v, i);
            free(m);
            fclose(f);
            return NULL;
        }
        m[i] = v;
    }

    fclose(f);

    *L = linhas;
    *C = colunas;
    return m;
}

int *matriz_aleatoria(int L, int C, int densidade_pct, unsigned int semente)
{
    int *m;
    int total;
    int i;

    if (densidade_pct < 0 || densidade_pct > 100) {
        fprintf(stderr, "matriz_aleatoria: densidade invalida (%d)\n",
                densidade_pct);
        return NULL;
    }

    m = matriz_alocar(L, C);
    if (m == NULL) {
        return NULL;
    }

    /* semente fixa: sequencial e paralelo processam exatamente os mesmos dados */
    srand(semente);

    total = L * C;
    for (i = 0; i < total; i++) {
        m[i] = ((rand() % 100) < densidade_pct) ? 1 : 0;
    }

    return m;
}

int matriz_salvar(const int *m, int L, int C, const char *caminho)
{
    FILE *f;
    int i;
    int j;

    if (m == NULL) {
        return -1;
    }

    f = fopen(caminho, "w");
    if (f == NULL) {
        perror(caminho);
        return -1;
    }

    fprintf(f, "%d %d\n", L, C);

    for (i = 0; i < L; i++) {
        for (j = 0; j < C; j++) {
            fprintf(f, "%d%c", m[i * C + j], (j == C - 1) ? '\n' : ' ');
        }
    }

    if (fclose(f) != 0) {
        perror(caminho);
        return -1;
    }
    return 0;
}

void matriz_imprimir(const int *m, int L, int C)
{
    int i;
    int j;

    if (m == NULL) {
        return;
    }

    for (i = 0; i < L; i++) {
        for (j = 0; j < C; j++) {
            printf("%d ", m[i * C + j]);
        }
        printf("\n");
    }
}
