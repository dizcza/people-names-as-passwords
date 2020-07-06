import argparse
import codecs
import math
from pathlib import Path

try:
    from tqdm import tqdm
except ImportError:
    print("To see the progress bar, run 'pip install tqdm'")

    def tqdm(iterable, *args, **kwargs):
        return iterable

REPLACE_CHAR = '|'

ROOT_DIR = Path(__file__).absolute().parent.parent
MASKS_DIR = ROOT_DIR / "masks"

MASKS_LENGTH = MASKS_DIR / "masks.raw"
STATS_FILE = MASKS_DIR / "most_used_names.txt"


class Node:
    def __init__(self):
        self.is_leaf = False  # word end
        self.children = {}  # links to next Nodes down the tree
        self.count = 0  # how many lines matched the pattern

    def __repr__(self):
        return f"{self.__class__.__name__}(" \
               f"is_leaf={self.is_leaf}, " \
               f"next={sorted(self.children.keys())})"


class Trie:
    """
    A prefix tree with ASCII character nodes.
    """

    def __init__(self):
        self.root = Node()

    def collect_statistics(self, node=None, stats={}, prefix=''):
        """
        :param node: a Node (root Node as the start)
        :param stats: accumulated statistics of prefix match counts
        :param prefix: the prefix (person name)
        :return: stats
        """
        if node is None:
            node = self.root
        if node.is_leaf and node.count > 0:
            assert prefix not in stats, prefix
            stats[prefix] = node.count
        for key, child in node.children.items():
            self.collect_statistics(child, stats=stats, prefix=f"{prefix}{key}")
        return stats

    def find_matches(self, file, key: str):
        """
        :param file: output file to write masks
        :param key: a line to search for patterns and create the masks from
        """
        key_lower = key.lower()
        L = len(key)
        for start in range(L):
            node = self.root
            for index in range(start, L + 1):
                if node is None:
                    break
                if node.is_leaf:
                    node.count += 1
                    len_match = index - start
                    mask = f"{key[:start]}{REPLACE_CHAR * len_match}{key[index:]}\n"
                    file.write(mask)
                if index == L:
                    break
                char = key_lower[index]
                node = node.children.get(char)

    def put(self, key: str):
        """
        :param key: a person name to insert in the current trie
        """
        node = self.root
        for index, char in enumerate(key):
            if char not in node.children:
                node.children[char] = Node()
            node = node.children[char]
        node.is_leaf = True


def write_stats(stats: dict):
    # writes the statistics of the most used people names as passwords
    tab_counts = math.ceil(math.log10(max(stats.values())))
    stats_sorted = sorted(stats.items(), key=lambda x: x[1], reverse=True)
    STATS_FILE.parent.mkdir(exist_ok=True, parents=True)
    with open(STATS_FILE, 'w') as f:
        for name, counts in stats_sorted:
            f.write(f"{counts:{tab_counts}d} {name}\n")


def create_masks_tries(wordlist, patterns):
    # build a trie with names
    trie = Trie()
    names = Path(patterns).read_text().splitlines()
    for name in names:
        trie.put(name)

    MASKS_DIR.mkdir(exist_ok=True, parents=True)

    with codecs.open(wordlist, 'r', encoding='utf-8',
                     errors='ignore') as infile, \
            open(MASKS_LENGTH, 'w') as fmasks:
        for line in tqdm(infile, desc="Creating masks"):
            if REPLACE_CHAR in line:
                # skip lines with REPLACE_CHAR
                continue
            trie.find_matches(fmasks, line.rstrip('\n'))

    stats = trie.collect_statistics()
    write_stats(stats)


if __name__ == '__main__':
    # names_ascii = ROOT_DIR / "gender" / "names" / "ascii.valid"
    # wordlist = ROOT_DIR / "wordlists" / "Top304Thousand-probable-v2.txt"
    # create_masks_tries(wordlist=wordlist, patterns=names_ascii)
    parser = argparse.ArgumentParser(
        description="Create masks, given a wordlist and a pattern list.")
    parser.add_argument("pattern", help="path to people names")
    parser.add_argument("wordlist", help="wordlist path")
    args = parser.parse_args()
    create_masks_tries(wordlist=args.wordlist, patterns=args.pattern)
