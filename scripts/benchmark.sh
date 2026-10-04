#!/bin/sh
# medicoes repetidas sobre os mesmos dados; rode da raiz com: make benchmark
# gera results/benchmark.txt (saida bruta) e results/medicoes.csv (uma linha por execucao)
RUNS=5
L=4000
C=4000
D=35
SEED=2026
THREADS="2 4 8 16"
TAMANHOS="12 100 316 1000 2000 4000"
OUT=results/benchmark.txt
CSV=results/medicoes.csv

if [ ! -x bin/sequencial ] || [ ! -x bin/paralelo ]; then
  echo "binarios nao encontrados: rode 'make' antes" >&2
  exit 1
fi

# roda um programa, guarda a saida bruta e devolve os dois ultimos campos: objetos e tempo
executa() {
  "$@" | tee -a "$OUT" | awk '/^aleatoria|^matriz gerada/ {print $(NF-1), $NF}'
}

# registra <matriz> <linhas> <colunas> <versao> <threads> <rep> <objetos> <tempo_s> <objetos_seq>
registra() {
  awk -v m="$1" -v l="$2" -v c="$3" -v v="$4" -v t="$5" -v r="$6" \
      -v o="$7" -v s="$8" -v ref="$9" 'BEGIN {
    printf "%s,%s,%s,%s,%s,%s,%.3f,%s,%s\n", m, l, c, v, t, r, s * 1000, o,
           (o == ref) ? "true" : "false"
  }' >> "$CSV"
  printf "  %-13s %-12s %2s threads  rep %s: %s s (%s objetos)\n" "$1" "$4" "$5" "$6" "$8" "$7"
}

# mede <matriz> <linhas> <colunas> <versao> <threads> <rep> <objetos_seq> comando...
mede() {
  _m=$1; _l=$2; _c=$3; _v=$4; _t=$5; _r=$6; _ref=$7
  shift 7
  echo "[$_m $_v $_t threads, repeticao $_r]" >> "$OUT"
  set -- $(executa "$@")
  registra "$_m" "$_l" "$_c" "$_v" "$_t" "$_r" "$1" "$2" "$_ref"
}

: > "$OUT"
echo "matriz,linhas,colunas,versao,trabalhadores,repeticao,tempo_ms,objetos,resultado_correto" > "$CSV"

echo "Parte 1: ${L}x${C}, densidade ${D}%, semente ${SEED}, ${RUNS} repeticoes, threads: ${THREADS}" | tee -a "$OUT"
echo "IMPORTANTE: os tempos dependem da maquina." | tee -a "$OUT"

# parte 1: matriz grande; as configuracoes rodam intercaladas em cada repeticao
i=1
while [ "$i" -le "$RUNS" ]; do
  echo "--- repeticao $i ---" >> "$OUT"
  echo "[matriz_grande sequencial 1 thread, repeticao $i]" >> "$OUT"
  set -- $(executa ./bin/sequencial -g "$L" "$C" "$D" "$SEED")
  ref=$1
  registra matriz_grande "$L" "$C" sequencial 1 "$i" "$1" "$2" "$ref"
  for t in $THREADS; do
    mede matriz_grande "$L" "$C" paralela "$t" "$i" "$ref" \
         ./bin/paralelo "$t" -g "$L" "$C" "$D" "$SEED"
    # opcional: mede tambem a versao anterior, se o caminho do binario for informado
    if [ -n "$PARALELO_ANTERIOR" ]; then
      mede matriz_grande "$L" "$C" paralela-v1 "$t" "$i" "$ref" \
           "$PARALELO_ANTERIOR" "$t" -g "$L" "$C" "$D" "$SEED"
    fi
  done
  i=$((i + 1))
done

# parte 2: tamanhos crescentes, para ver a partir de quando o paralelo compensa
echo "Parte 2: matrizes NxN (N em ${TAMANHOS}), densidade ${D}%, sequencial x 4 threads" | tee -a "$OUT"
for n in $TAMANHOS; do
  i=1
  while [ "$i" -le "$RUNS" ]; do
    echo "[escala_${n} sequencial 1 thread, repeticao $i]" >> "$OUT"
    set -- $(executa ./bin/sequencial -g "$n" "$n" "$D" "$SEED")
    ref=$1
    registra "escala_${n}" "$n" "$n" sequencial 1 "$i" "$1" "$2" "$ref"
    mede "escala_${n}" "$n" "$n" paralela 4 "$i" "$ref" \
         ./bin/paralelo 4 -g "$n" "$n" "$D" "$SEED"
    i=$((i + 1))
  done
done

if grep -q ',false$' "$CSV"; then
  echo "ATENCAO: alguma execucao paralela divergiu do sequencial (veja $CSV)" >&2
  exit 1
fi
echo "Todas as execucoes paralelas bateram com o sequencial. Dados em $CSV"
