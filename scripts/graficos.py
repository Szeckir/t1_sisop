#!/usr/bin/env python3
"""gera os graficos svg de results/medicoes.csv so com a biblioteca padrao do python.
uso, a partir da raiz e depois do make benchmark: python3 scripts/graficos.py"""
import csv
import math
import statistics
from collections import defaultdict
from pathlib import Path
from xml.sax.saxutils import escape

RAIZ = Path(__file__).resolve().parent.parent
ARQ_CSV = RAIZ / "results" / "medicoes.csv"
DIR_SAIDA = RAIZ / "results"

AZUL, LARANJA, CINZA, CINZA_CLARO = "#1f6fb4", "#d9822b", "#444444", "#9a9a9a"


def br(v, casas):
    """numero com virgula decimal, como no restante da documentacao."""
    return f"{v:.{casas}f}".replace(".", ",")


class Grafico:
    """plano cartesiano minimo em svg, com eixos lineares ou logaritmicos."""

    LARG, ALT = 760, 460
    ESQ, DIR, TOPO, BASE = 90, 30, 56, 64

    def __init__(self, titulo, rot_x, rot_y, x_lim, y_lim, log_x=None, log_y=None):
        self.titulo, self.rot_x, self.rot_y = titulo, rot_x, rot_y
        self.x_lim, self.y_lim = x_lim, y_lim
        self.log_x, self.log_y = log_x, log_y  # base do logaritmo; vazio = eixo linear
        self.svg = []

    @staticmethod
    def _frac(v, lim, base):
        a, b = lim
        if base:
            return (math.log(v, base) - math.log(a, base)) / (math.log(b, base) - math.log(a, base))
        return (v - a) / (b - a)

    def px(self, x):
        return self.ESQ + self._frac(x, self.x_lim, self.log_x) * (self.LARG - self.ESQ - self.DIR)

    def py(self, y):
        return self.ALT - self.BASE - self._frac(y, self.y_lim, self.log_y) * (self.ALT - self.BASE - self.TOPO)

    def texto(self, x, y, s, tam=13, ancora="middle", peso="normal", cor="#222", girar=None):
        rot = f' transform="rotate({girar} {x:.1f} {y:.1f})"' if girar else ""
        self.svg.append(f'<text x="{x:.1f}" y="{y:.1f}" font-size="{tam}" text-anchor="{ancora}" '
                        f'font-weight="{peso}" fill="{cor}"{rot}>{escape(s)}</text>')

    def eixos(self, ticks_x, ticks_y):
        """ticks_x e ticks_y: listas de (valor, rotulo)."""
        x0, x1 = self.ESQ, self.LARG - self.DIR
        y0, y1 = self.ALT - self.BASE, self.TOPO
        for v, r in ticks_y:
            y = self.py(v)
            self.svg.append(f'<line x1="{x0}" y1="{y:.1f}" x2="{x1}" y2="{y:.1f}" stroke="#e3e3e3"/>')
            self.texto(x0 - 8, y + 4, r, ancora="end")
        for v, r in ticks_x:
            x = self.px(v)
            self.svg.append(f'<line x1="{x:.1f}" y1="{y0}" x2="{x:.1f}" y2="{y1}" stroke="#e3e3e3"/>')
            self.texto(x, y0 + 20, r)
        self.svg.append(f'<rect x="{x0}" y="{y1}" width="{x1 - x0}" height="{y0 - y1}" '
                        f'fill="none" stroke="#333"/>')
        self.texto(self.LARG / 2, 32, self.titulo, tam=17, peso="bold")
        self.texto((x0 + x1) / 2, self.ALT - 16, self.rot_x)
        self.texto(24, (y0 + y1) / 2, self.rot_y, girar=-90)

    def serie(self, pontos, cor, tracejado=False, desvios=None, rotulos=None):
        """pontos: [(x, y)]; desvios: desvio-padrao de cada y; rotulos: texto sobre cada ponto."""
        traco = ' stroke-dasharray="7 5"' if tracejado else ""
        coords = " ".join(f"{self.px(x):.1f},{self.py(y):.1f}" for x, y in pontos)
        self.svg.append(f'<polyline points="{coords}" fill="none" stroke="{cor}" '
                        f'stroke-width="2.5"{traco}/>')
        if tracejado:
            return
        for i, (x, y) in enumerate(pontos):
            cx, cy = self.px(x), self.py(y)
            if desvios and desvios[i] > 0:
                ya, yb = self.py(y - desvios[i]), self.py(y + desvios[i])
                self.svg.append(f'<line x1="{cx:.1f}" y1="{ya:.1f}" x2="{cx:.1f}" y2="{yb:.1f}" '
                                f'stroke="{cor}" stroke-width="1.5"/>')
                for yy in (ya, yb):
                    self.svg.append(f'<line x1="{cx - 5:.1f}" y1="{yy:.1f}" x2="{cx + 5:.1f}" '
                                    f'y2="{yy:.1f}" stroke="{cor}" stroke-width="1.5"/>')
            self.svg.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="4.5" fill="{cor}"/>')
            if rotulos and rotulos[i]:
                self.texto(cx, cy - 11, rotulos[i], tam=12, peso="bold", cor=cor)

    def legenda(self, itens, canto="esq"):
        """itens: [(rotulo, cor, tracejado)]."""
        larg = max(len(r) for r, _, _ in itens) * 7.4 + 52
        x = self.ESQ + 12 if canto == "esq" else self.LARG - self.DIR - 12 - larg
        y = self.TOPO + 12
        self.svg.append(f'<rect x="{x:.1f}" y="{y}" width="{larg:.1f}" height="{len(itens) * 22 + 10}" '
                        f'fill="white" fill-opacity="0.92" stroke="#bbb"/>')
        for i, (r, cor, tracejado) in enumerate(itens):
            ly = y + 20 + i * 22
            traco = ' stroke-dasharray="7 5"' if tracejado else ""
            self.svg.append(f'<line x1="{x + 10:.1f}" y1="{ly - 4}" x2="{x + 38:.1f}" y2="{ly - 4}" '
                            f'stroke="{cor}" stroke-width="2.5"{traco}/>')
            self.texto(x + 46, ly, r, ancora="start")

    def salva(self, nome):
        corpo = "\n".join(self.svg)
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.LARG}" height="{self.ALT}" '
               f'viewBox="0 0 {self.LARG} {self.ALT}" font-family="Segoe UI, Helvetica, Arial, sans-serif">\n'
               f'<rect width="100%" height="100%" fill="white"/>\n{corpo}\n</svg>\n')
        caminho = DIR_SAIDA / nome
        caminho.write_text(svg, encoding="utf-8")
        print(f"gerado: {caminho.relative_to(RAIZ)}")


def carrega():
    """devolve {(matriz, versao, threads): [tempos em ms]} e {matriz: celulas}."""
    tempos = defaultdict(list)
    celulas = {}
    with open(ARQ_CSV, newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            tempos[(r["matriz"], r["versao"], int(r["trabalhadores"]))].append(float(r["tempo_ms"]))
            celulas[r["matriz"]] = int(r["linhas"]) * int(r["colunas"])
    return tempos, celulas


def resumo(amostras):
    return statistics.median(amostras), (statistics.stdev(amostras) if len(amostras) > 1 else 0.0)


def main():
    tempos, celulas = carrega()
    t_seq, dp_seq = resumo(tempos[("matriz_grande", "sequencial", 1)])
    versoes = [(v, AZUL if v == "paralela" else LARANJA,
                "Paralela (versão atual)" if v == "paralela" else "Paralela v1 (antes da otimização)")
               for v in ("paralela", "paralela-v1")
               if any(k[0] == "matriz_grande" and k[1] == v for k in tempos)]
    threads = sorted({k[2] for k in tempos if k[0] == "matriz_grande" and k[1] == "paralela"})

    print("\nmatriz_grande: versao | p | mediana (ms) | desvio (ms) | S | E")
    print(f"sequencial | 1 | {br(t_seq, 3)} | {br(dp_seq, 3)} | 1,000 | 1,000")
    dados = {}
    for v, _, _ in versoes:
        dados[v] = []
        for p in threads:
            med, dp = resumo(tempos[("matriz_grande", v, p)])
            s = t_seq / med
            dados[v].append((p, med, dp, s, s / p))
            print(f"{v} | {p} | {br(med, 3)} | {br(dp, 3)} | {br(s, 3)} | {br(s / p, 3)}")

    ticks_p = [(p, str(p)) for p in [1] + threads]
    x_lim = (0.8, threads[-1] * 1.25)

    g = Grafico("Tempo de execução (matriz 4000 x 4000)", "Threads (1 = sequencial)",
                "Mediana do tempo (ms)", x_lim, (0, t_seq * 1.6), log_x=2)
    passo = 50 if t_seq < 400 else 100
    g.eixos(ticks_p, [(y, str(y)) for y in range(0, int(t_seq * 1.6) + 1, passo)])
    g.serie([(1, t_seq), (threads[-1], t_seq)], CINZA, tracejado=True)
    for v, cor, _ in versoes:
        g.serie([(p, med) for p, med, _, _, _ in dados[v]], cor,
                desvios=[dp for _, _, dp, _, _ in dados[v]])
    g.legenda([("Sequencial", CINZA, True)] + [(nome, cor, False) for _, cor, nome in versoes], "dir")
    g.salva("grafico-tempo.svg")

    g = Grafico("Aceleração S(p) = Tseq / Tpar(p)", "Threads (p)", "Aceleração S(p)",
                x_lim, (0.5, threads[-1] * 1.25), log_x=2, log_y=2)
    g.eixos(ticks_p, [(y, br(y, 1) if y < 1 else str(int(y))) for y in (0.5, 1, 2, 4, 8, 16)
                      if y <= threads[-1]])
    g.serie([(1, 1), (threads[-1], threads[-1])], CINZA_CLARO, tracejado=True)
    for v, cor, _ in versoes:
        g.serie([(1, 1)] + [(p, s) for p, _, _, s, _ in dados[v]], cor,
                rotulos=[""] + [br(s, 2) for _, _, _, s, _ in dados[v]])
    g.legenda([("Ideal S(p) = p", CINZA_CLARO, True)] + [(nome, cor, False) for _, cor, nome in versoes])
    g.salva("grafico-aceleracao.svg")

    g = Grafico("Eficiência E(p) = S(p) / p", "Threads (p)", "Eficiência E(p)",
                x_lim, (0, 1.2), log_x=2)
    g.eixos(ticks_p, [(y / 10, br(y / 10, 1)) for y in range(0, 13, 2)])
    g.serie([(1, 1), (threads[-1], 1)], CINZA_CLARO, tracejado=True)
    for v, cor, _ in versoes:
        g.serie([(1, 1)] + [(p, e) for p, _, _, _, e in dados[v]], cor,
                rotulos=[""] + [br(e, 2) for _, _, _, _, e in dados[v]])
    g.legenda([("Ideal E(p) = 1", CINZA_CLARO, True)] + [(nome, cor, False) for _, cor, nome in versoes], "dir")
    g.salva("grafico-eficiencia.svg")

    escala = sorted((celulas[m], m) for m in celulas if m.startswith("escala_"))
    if escala:
        print("\nescala: matriz | celulas | seq mediana (ms) | 4 threads mediana (ms) | S")
        seq_pts, par_pts = [], []
        for n_cel, m in escala:
            ts, _ = resumo(tempos[(m, "sequencial", 1)])
            tp, _ = resumo(tempos[(m, "paralela", 4)])
            seq_pts.append((n_cel, ts / 1000))
            par_pts.append((n_cel, tp / 1000))
            print(f"{m} | {n_cel} | {br(ts, 4)} | {br(tp, 4)} | {br(ts / tp, 2)}")
        g = Grafico("Tempo x tamanho da matriz (densidade 35%)", "Células da matriz (L x C)",
                    "Mediana do tempo (s)", (50, 5e7), (1e-6, 2), log_x=10, log_y=10)
        g.eixos([(10 ** k, r) for k, r in zip(range(2, 8), ("100", "1 mil", "10 mil", "100 mil", "1 mi", "10 mi"))],
                [(10 ** k, r) for k, r in zip(range(-6, 1), ("1 µs", "10 µs", "100 µs", "1 ms", "10 ms", "100 ms", "1 s"))])
        g.serie(seq_pts, CINZA)
        g.serie(par_pts, AZUL)
        g.legenda([("Sequencial", CINZA, False), ("Paralela, 4 threads", AZUL, False)])
        g.salva("grafico-escala.svg")


if __name__ == "__main__":
    main()
