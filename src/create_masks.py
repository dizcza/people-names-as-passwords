import argparse
import codecs
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
            stats[prefix] = node.count
        for key, child in node.children.items():
            self.collect_statistics(child, stats=stats, prefix=f"{prefix}{key}")
        return stats

    def write_matches(self, output_file, line: str):
        """
        :param output_file: output file to write masks
        :param line: a line to search for patterns and create the masks from
        """
        line_lower = line.lower()
        L = len(line)
        for start in range(L):
            node = self.root
            longest_index = start
            longest_match_node = None
            for index in range(start, L + 1):
                if node is None:
                    break
                if node.is_leaf:
                    longest_index = index
                    longest_match_node = node
                if index == L:
                    break
                char = line_lower[index]
                node = node.children.get(char)
            if longest_match_node is not None:
                longest_match_node.count += 1
                len_match = longest_index - start
                mask = f"{line[:start]}{REPLACE_CHAR * len_match}{line[longest_index:]}\n"
                output_file.write(mask)

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
            trie.write_matches(fmasks, line.rstrip('\n'))

    stats = trie.collect_statistics()
    with open(STATS_FILE, 'w') as f:
        for name, counts in stats.items():
            f.write(f"{counts} {name}\n")


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
