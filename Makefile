# t1 sistemas operacionais: contagem de objetos sequencial e paralela
CC = cc
CFLAGS = -std=c89 -Wall -Wextra -pedantic -O2 -Isrc
PTHREAD = -pthread
BIN = bin

all: $(BIN)/sequencial $(BIN)/paralelo

$(BIN):
	mkdir -p $(BIN)

$(BIN)/sequencial: src/conta-objetos-sequencial.c src/matriz.c src/matriz.h | $(BIN)
	$(CC) $(CFLAGS) src/conta-objetos-sequencial.c src/matriz.c -o $@

$(BIN)/paralelo: src/conta-objetos-paralelo.c src/matriz.c src/matriz.h | $(BIN)
	$(CC) $(CFLAGS) $(PTHREAD) src/conta-objetos-paralelo.c src/matriz.c -o $@

test: all
	./$(BIN)/sequencial
	./$(BIN)/paralelo 2
	./$(BIN)/paralelo 4

benchmark: all
	sh scripts/benchmark.sh

graficos:
	python3 scripts/graficos.py

clean:
	rm -rf $(BIN)

help:
	@echo "alvos do make:"
	@echo "  make                 compila bin/sequencial e bin/paralelo"
	@echo "  make test            roda as 5 matrizes obrigatorias: sequencial, 2 e 4 threads"
	@echo "  make benchmark       mede desempenho e gera results/benchmark.txt e results/medicoes.csv"
	@echo "  make graficos        gera os graficos results/grafico-*.svg a partir do csv"
	@echo "  make clean           apaga a pasta bin/"
	@echo "  make help            mostra esta lista"
	@echo ""
	@echo "execucao direta (depois de compilar):"
	@echo "  ./bin/sequencial                           5 matrizes obrigatorias"
	@echo "  ./bin/sequencial <arquivo>                 uma matriz de arquivo"
	@echo "  ./bin/sequencial -g <L> <C> <dens> <seed>  matriz gerada"
	@echo "  ./bin/paralelo <threads>                   5 matrizes obrigatorias"
	@echo "  ./bin/paralelo <threads> <arquivo>         uma matriz de arquivo"
	@echo "  ./bin/paralelo <threads> -g <L> <C> <dens> <seed>"
	@echo ""
	@echo "exemplos:"
	@echo "  ./bin/paralelo 4 tests/obrigatorios/ex3.txt"
	@echo "  ./bin/paralelo 4 tests/adicionais/a3_diagonal.txt"
	@echo "  ./bin/sequencial -g 4000 4000 35 2026"
	@echo "  ./bin/paralelo 8 -g 4000 4000 35 2026"

.PHONY: all test benchmark graficos clean help
