import argparse
import codecs
import math
import re
import subprocess
from collections import defaultdict
from pathlib import Path
from collections import UserString
import string
import logging
import os
import time

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
NAMES_ASCII_1000 = GENDER_DIR / "names" / "ascii.valid.1000"
WORDLIST_PATH = ROOT_DIR / "wordlists" / "Top304Thousand-probable-v2.txt"

MASKS_LENGTH = MASKS_DIR / "masks.length"
MASKS_SINGLE = MASKS_DIR / "masks.single"
STATS_FILE = MASKS_DIR / "stats.txt"

FILE_A = ROOT_DIR / "ignored" / "test" / "fileA"
FILE_B = ROOT_DIR / "ignored" / "test" / "fileB"

LOGS_DIR = ROOT_DIR / "ignored" / "logs"


def create_logger(logging_level=logging.DEBUG):
    LOGS_DIR.mkdir(exist_ok=True, parents=True)
    new_logger = logging.getLogger(__name__)
    new_logger.disabled = True
    new_logger.setLevel(logging_level)

    formatter = logging.Formatter('%(levelname)s - %(message)s')

    log_path = LOGS_DIR / time.strftime('%Y-%b-%d.log')
    log_path.unlink(missing_ok=True)
    file_handler = logging.FileHandler(log_path)
    file_handler.setLevel(logging_level)
    file_handler.setFormatter(formatter)
    new_logger.addHandler(file_handler)

    return new_logger


class StringLine(UserString):
    def __init__(self, seq):
        super().__init__(seq)
        self.visited = [False] * len(seq)


class Node:
    def __init__(self, char=''):
        self.is_end = None
        self.next = {}
        self.char = char

    def __repr__(self):
        return f"{self.__class__.__name__}(" \
               f"is_end={self.is_end}, char={self.char}, " \
               f"next={''.join(sorted(self.next.keys()))})"


class Tries:
    REPLACE_CHAR = '|'

    def __init__(self):
        self.root = Node(char='root')
        self.stats = defaultdict(int)
        self.logger = create_logger()

    def traverse(self, file, key: str):
        for start, char in enumerate(key):
            # if self.root.next.get(char.lower()):
            self._traverse(file, node=self.root, key=key, d=0, start=start)

    def _traverse(self, file, node: Node, key: str, d: int, start: int):
        if node is None:
            return
        pos = d + start

        if node.is_end:
            match = key[start: pos].lower()
            self.stats[match] += 1
            ml = f"{key[:start]}{self.REPLACE_CHAR * len(match)}{key[start + len(match):]}\n"
            self.logger.info(f"Replaced {key=} with {ml=}, {match=}")
            file.write(ml)

        if pos == len(key):
            # traversed all way down
            return

        self.logger.debug(f"{node=}, {key=}, {d=}, {start=}['{key[start]}'], key[s:s+d]='{key[start: start+d]}'")
        char = key[pos].lower()
        next_node = node.next.get(char)
        self._traverse(file, node=next_node, key=key, d=d+1, start=start)

    def put(self, key: str):
        self.root = self._put(node=self.root, key=key, d=0)

    def _put(self, node: Node, key: str, d: int):
        if node is None:
            node = Node(char=key[d-1])
        if d == len(key):
            node.is_end = True
            return node
        char = key[d]
        assert char in string.ascii_lowercase, char
        if char not in node.next:
            node.next[char] = Node(char=char)
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


def create_masks_tries(wordlist=WORDLIST_PATH, patterns=NAMES_ASCII_PATH):
    n_words = count_wordlist(wordlist)

    tries = Tries()
    # fileA lowercase
    names = Path(patterns).read_text().splitlines()
    for name in names:
        tries.put(name)

    MASKS_DIR.mkdir(exist_ok=True, parents=True)

    with codecs.open(wordlist, 'r', encoding='utf-8', errors='ignore') as infile, \
            open(MASKS_LENGTH, 'w') as fmasks:
        for line in tqdm(infile, total=n_words, desc="Creating masks"):
            if tries.REPLACE_CHAR in line:
                # skip lines with 'replace_char'
                continue
            tries.logger.debug(f"Scanning {line=}")
            tries.traverse(fmasks, line.rstrip('\n'))


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
    # create_masks(patterns=NAMES_ASCII_1000)
    # create_masks_tries(wordlist=FILE_B, patterns=FILE_A)
    create_masks_tries(wordlist=WORDLIST_PATH, patterns=NAMES_ASCII_1000)
    # parser = argparse.ArgumentParser(description="Create masks, given a wordlist and a pattern list.")
    # parser.add_argument("pattern", help="path to people names")
    # parser.add_argument("wordlist", help="wordlist path")
    # args = parser.parse_args()
    # create_masks(wordlist=args.wordlist, patterns=args.pattern)
