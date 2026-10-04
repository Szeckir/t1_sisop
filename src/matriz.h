/* matriz binaria em um unico vetor de inteiros: a celula da linha i, coluna j
 * fica em m[i * colunas + j]. um so malloc, um so free e acesso sequencial na memoria. */

#ifndef MATRIZ_H
#define MATRIZ_H

/* aloca a matriz zerada; retorna null em erro. libere com free() */
int *matriz_alocar(int L, int C);

/* le arquivo no formato "linhas colunas" seguido dos valores 0 ou 1; retorna null em erro */
int *matriz_ler(const char *caminho, int *L, int *C);

/* gera matriz com densidade_pct por cento de 1; a semente torna o resultado reproduzivel */
int *matriz_aleatoria(int L, int C, int densidade_pct, unsigned int semente);

/* grava no formato aceito por matriz_ler; retorna 0 em sucesso */
int matriz_salvar(const int *m, int L, int C, const char *caminho);

/* imprime na tela; so para matrizes pequenas */
void matriz_imprimir(const int *m, int L, int C);

#endif
