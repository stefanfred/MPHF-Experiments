#!/usr/bin/env python3
"""
Extracts Pareto fronts from the output of `Comparison` (RESULT / SETRESULT lines) for every (k, loadFactor).

  ./scripts/pareto.py results.log                    # table of the fronts on stdout
  ./scripts/pareto.py results.log --csv fronts.csv   # all configurations, flagged if on the front
  ./scripts/pareto.py results.log --plot out/        # one plot per (k, alpha); needs matplotlib
  ./scripts/pareto.py results.log --y construction   # space vs. construction time
  ./scripts/pareto.py results.log --set              # space vs. static-set query time instead of PHF query time

Space (x) is minimized with the chosen time (y); the front is computed per (k, loadFactor) over all contenders
and contender configurations, and each point keeps the name of the configuration that produced it.
"""
import argparse, collections, csv, os, re, sys

NAME = re.compile(r"^(RESULT|SETRESULT) name=(.*?) (\w+=.*)$")


def parse(path, set_mode):
    rows = []
    for line in open(path, errors="replace"):
        m = NAME.match(line.rstrip("\n"))
        if not m or (m.group(1) == "SETRESULT") != set_mode:
            continue
        fields = dict(kv.split("=", 1) for kv in m.group(3).split())
        fields["name"] = m.group(2)
        rows.append(fields)
    return rows


def point(row, y, set_mode):
    x = float(row["bitsPerElement"])
    if set_mode:
        t = float(row["nsPerQuery"])
    elif y == "construction":
        t = float(row["constructionTimeMilliseconds"]) * 1e6 / float(row["N"])  # ns per key
    else:
        total = float(row["numQueriesTotal"])
        t = float(row["queryTimeMilliseconds"]) * 1e6 / total if total > 0 else float("nan")
    return x, t


def front(points):
    """points: list of (x, y, row). Returns the non-dominated ones (minimizing both)."""
    best, out = float("inf"), []
    for x, y, r in sorted(points, key=lambda p: (p[0], p[1])):
        if y == y and y < best:
            best = y
            out.append((x, y, r))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--y", choices=["query", "construction"], default="query")
    ap.add_argument("--set", action="store_true", help="use SETRESULT lines (static hash set benchmark)")
    ap.add_argument("--mode", default="prefetch", help="static set only: loop or prefetch")
    ap.add_argument("--positive", default="0.5", help="static set only: fraction of positive queries")
    ap.add_argument("--csv")
    ap.add_argument("--plot")
    a = ap.parse_args()

    rows = [r for p in a.logs for r in parse(p, a.set)]
    if a.set:
        rows = [r for r in rows if r["mode"] == a.mode and abs(float(r["positiveFraction"]) - float(a.positive)) < 1e-9]
    groups = collections.defaultdict(list)
    for r in rows:
        x, y = point(r, a.y, a.set)
        groups[(int(r["k"]) if "k" in r else 1, float(r["loadFactor"]))].append((x, y, r))
    ylabel = "ns / query" if (a.set or a.y == "query") else "ns / key (construction)"

    writer = None
    if a.csv:
        f = open(a.csv, "w", newline="")
        writer = csv.writer(f)
        writer.writerow(["k", "loadFactor", "name", "bitsPerElement", "ns", "pareto"])
    for (k, alpha), pts in sorted(groups.items()):
        fr = front(pts)
        on = {id(r) for _, _, r in fr}
        print(f"\n== k={k} loadFactor={alpha}: {len(pts)} configurations, {len(fr)} on the front")
        for x, y, r in fr:
            print(f"  {x:8.3f} bits/key  {y:10.2f} ns  {r['name']}")
        if writer:
            for x, y, r in pts:
                writer.writerow([k, alpha, r["name"], x, y, int(id(r) in on)])
        if a.plot:
            import matplotlib
            matplotlib.use("Agg")
            import matplotlib.pyplot as plt
            os.makedirs(a.plot, exist_ok=True)
            fig, ax = plt.subplots(figsize=(6, 4))
            byc = collections.defaultdict(list)
            for x, y, r in pts:
                byc[r["name"].split(" ")[0]].append((x, y))
            for c, v in sorted(byc.items()):
                ax.scatter(*zip(*v), s=8, alpha=0.5, label=c)
            ax.step([x for x, _, _ in fr], [y for _, y, _ in fr], where="post", color="black", lw=1)
            ax.set_xlabel("bits per key"); ax.set_ylabel(ylabel); ax.set_title(f"k={k}, load factor {alpha}")
            ax.legend(fontsize=6)
            fig.tight_layout(); fig.savefig(os.path.join(a.plot, f"pareto_k{k}_alpha{alpha}.pdf")); plt.close(fig)


if __name__ == "__main__":
    sys.exit(main())
