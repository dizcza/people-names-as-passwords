import argparse
import codecs
import math
import re
import subprocess
from collections import defaultdict
from pathlib import Path

try:
    from tqdm import tqdm
except ImportError:
    print("To see the progress bar, run 'pip install tqdm'")

    def tqdm(iterable, *args, **kwargs):
        return iterable


ROOT_DIR = Path(__file__).absolute().parent.parent
GENDER_DIR = ROOT_DIR / "gender"
MASKS_DIR = ROOT_DIR / "masks" / "py"

NAMES_ASCII_PATH = GENDER_DIR / "names" / "ascii.valid"
WORDLIST_PATH = ROOT_DIR / "wordlists" / "Top304Thousand-probable-v2.txt"

MASKS_LENGTH = MASKS_DIR / "masks.length"
MASKS_SINGLE = MASKS_DIR / "masks.single"
STATS_FILE = MASKS_DIR / "stats.txt"


def count_wordlist(wordlist):
    wordlist = str(wordlist)
    completed = subprocess.run(['wc', '-l', wordlist], capture_output=True, text=True)
    out = completed.stdout.rstrip('\n')
    counter = None
    if re.fullmatch(f"\d+ {wordlist}", out):
        counter, path = out.split(' ')
        counter = int(counter)
    return counter


def create_masks(wordlist=WORDLIST_PATH, patterns=NAMES_ASCII_PATH, replace_char='|'):
    n_words = count_wordlist(wordlist)

    # fileA lowercase
    names = Path(patterns).read_text().splitlines()

    stats = defaultdict(int)
    masks_length = []
    masks_single = []
    with codecs.open(wordlist, 'r', encoding='utf-8', errors='ignore') as infile:
        for line in tqdm(infile, total=n_words, desc="Creating masks"):
            if replace_char in line:
                # skip lines with 'replace_char'
                continue
            line_lower = line.lower()
            for name in names:
                i = line_lower.find(name)
                if i != -1:
                    stats[name] += 1
                    ml = f"{line[:i]}{replace_char * len(name)}{line[i + len(name):]}"
                    ms = f"{line[:i]}{replace_char}{line[i + len(name):]}"
                    masks_length.append(ml)
                    masks_single.append(ms)

    MASKS_DIR.mkdir(exist_ok=True, parents=True)

    tab_counts = math.ceil(math.log10(max(stats.values())))
    stats_sorted = sorted(stats.items(), key=lambda x: x[1], reverse=True)
    with open(STATS_FILE, 'w') as f:
        for name, counts in stats_sorted:
            f.write(f"{counts:{tab_counts}d} {name}\n")

    with open(MASKS_LENGTH, 'w') as f:
        f.writelines(masks_length)
    with open(MASKS_SINGLE, 'w') as f:
        f.writelines(masks_single)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Create masks, given a wordlist and a pattern list.")
    parser.add_argument("pattern", help="path to people names")
    parser.add_argument("wordlist", help="wordlist path")
    args = parser.parse_args()
    create_masks(wordlist=args.wordlist, patterns=args.pattern)
