import re
from pathlib import Path
import subprocess

import matplotlib.pyplot as plt
import numpy as np

EVALUATION_DIR = Path(__file__).absolute().parent
WORDLISTS_DIR = EVALUATION_DIR.parent / "wordlists"


def count_lines(fpath):
    fpath = str(fpath)
    completed = subprocess.run(['wc', '-l', fpath], capture_output=True,
                               universal_newlines=True)
    if completed.stderr:
        raise RuntimeError("Run 'evaluate.sh' first.")
    counter = 0
    if re.fullmatch(f"\d+ {fpath}\n?", completed.stdout):
        counter, path = completed.stdout.split(' ')
    return int(counter)


def plot_combined():
    N_BEST64 = count_lines(
        WORDLISTS_DIR / "Top304Thousand-probable-v2.txt.best64")
    N_TOP29M = count_lines(WORDLISTS_DIR / "Top29Million-probable-v2.txt")

    fig, ax = plt.subplots()
    matches_combined = np.genfromtxt(EVALUATION_DIR / "matches.combined",
                                     delimiter=' ', dtype=int)
    matches_combined = np.vstack(
        [np.zeros(matches_combined.shape[1], dtype=matches_combined.dtype),
         matches_combined])
    n, n_combined, hits, n_on_top, hits_on_top_best64 = matches_combined.T
    hits_best64 = int((EVALUATION_DIR / "matches.best64").read_text())

    hits = hits / N_TOP29M * 100
    hits_on_top_best64 = hits_on_top_best64 / N_TOP29M * 100
    hits_best64 = hits_best64 / N_TOP29M * 100

    ax.plot(n_combined, hits, label='names', marker='o', ms=3)
    ax.plot(n_on_top + N_BEST64, hits_best64 + hits_on_top_best64,
            label='names+best64', marker='o', ms=3)
    ax.scatter(N_BEST64, hits_best64, marker='*', label='best64', s=100,
               c='green')
    limits = ax.get_ylim()
    ax.vlines(x=N_TOP29M, ymin=limits[0], ymax=limits[1], linestyles='--',
              label='Top29M total')
    ax.legend()
    ax.set_xlim(xmin=0)
    ax.set_xlabel('num. generated password candidates')
    ax.set_ylabel('num. recovered / total, %')
    ax.set_title(
        "Evaluation of people-names VS rule-best64 on Top29 from Top304k wordlist",
        fontsize=10)
    plt.savefig(EVALUATION_DIR / "evaluation.png")
    plt.show()


if __name__ == '__main__':
    plot_combined()
