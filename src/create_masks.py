import argparse
import codecs
import math
import re
import string
import subprocess
from collections import defaultdict
from pathlib import Path

try:
    from tqdm import tqdm
except ImportError:
    print("To see the progress bar, run 'pip install tqdm'")

    def tqdm(iterable, *args, **kwargs):
        return iterable

REPLACE_CHAR = '|'

ROOT_DIR = Path(__file__).absolute().parent.parent
GENDER_DIR = ROOT_DIR / "gender"
MASKS_DIR = ROOT_DIR / "masks"

NAMES_ASCII_PATH = GENDER_DIR / "names" / "ascii.valid"
WORDLIST_PATH = ROOT_DIR / "wordlists" / "Top304Thousand-probable-v2.txt"

MASKS_LENGTH = MASKS_DIR / "masks.length"
MASKS_SINGLE = MASKS_DIR / "masks.single"
STATS_FILE = MASKS_DIR / "most_used_names.txt"


class Node:
    def __init__(self):
        self.is_end = None
        self.next = {}

    def __repr__(self):
        return f"{self.__class__.__name__}(" \
               f"is_end={self.is_end}, " \
               f"next={''.join(sorted(self.next.keys()))})"


class Tries:

    def __init__(self):
        self.root = Node()
        self.stats = defaultdict(int)

    def traverse(self, file, key: str):
        for start, char in enumerate(key):
            self._traverse(file, node=self.root, key=key, d=0, start=start)

    def _traverse(self, file, node: Node, key: str, d: int, start: int):
        if node is None:
            return
        pos = d + start

        if node.is_end:
            match = key[start: pos].lower()
            self.stats[match] += 1
            ml = f"{key[:start]}{REPLACE_CHAR * len(match)}{key[start + len(match):]}\n"
            file.write(ml)

        if pos == len(key):
            # traversed all way down
            return

        char = key[pos].lower()
        next_node = node.next.get(char)
        self._traverse(file, node=next_node, key=key, d=d+1, start=start)

    def put(self, key: str):
        self.root = self._put(node=self.root, key=key, d=0)

    def _put(self, node: Node, key: str, d: int):
        if node is None:
            node = Node()
        if d == len(key):
            node.is_end = True
            return node
        char = key[d]
        assert char in string.ascii_lowercase, char
        if char not in node.next:
            node.next[char] = Node()
        node.next[char] = self._put(node.next[char], key=key, d=d+1)
        return node


def count_wordlist(wordlist):
    wordlist = str(wordlist)
    completed = subprocess.run(['wc', '-l', wordlist], capture_output=True, text=True)
    out = completed.stdout.rstrip('\n')
    counter = None
    if re.fullmatch(f"\d+ {wordlist}", out):
        counter, path = out.split(' ')
        counter = int(counter)
    return counter


def write_stats(stats: dict):
    tab_counts = math.ceil(math.log10(max(stats.values())))
    stats_sorted = sorted(stats.items(), key=lambda x: x[1], reverse=True)
    STATS_FILE.parent.mkdir(exist_ok=True, parents=True)
    with open(STATS_FILE, 'w') as f:
        for name, counts in stats_sorted:
            f.write(f"{counts:{tab_counts}d} {name}\n")



def create_masks_tries(wordlist=WORDLIST_PATH, patterns=NAMES_ASCII_PATH):
    n_words = count_wordlist(wordlist)

    # build Tries with names
    tries = Tries()
    names = Path(patterns).read_text().splitlines()
    for name in names:
        tries.put(name)

    MASKS_DIR.mkdir(exist_ok=True, parents=True)

    with codecs.open(wordlist, 'r', encoding='utf-8', errors='ignore') as infile, \
            open(MASKS_LENGTH, 'w') as fmasks:
        for line in tqdm(infile, total=n_words, desc="Creating masks"):
            if REPLACE_CHAR in line:
                # skip lines with REPLACE_CHAR
                continue
            tries.traverse(fmasks, line.rstrip('\n'))

    write_stats(tries.stats)


if __name__ == '__main__':
    # create_masks_tries(wordlist=WORDLIST_PATH, patterns=NAMES_ASCII_PATH)
    parser = argparse.ArgumentParser(description="Create masks, given a wordlist and a pattern list.")
    parser.add_argument("pattern", help="path to people names")
    parser.add_argument("wordlist", help="wordlist path")
    args = parser.parse_args()
    create_masks_tries(wordlist=args.wordlist, patterns=args.pattern)
