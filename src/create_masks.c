/*
 * Find people names in a wordlist and replace matches with a mask REPLACE_CHAR.
 * Brute force algorithm:
 * 
 * for each line in wordlist do
 *   for each person_name in people_names_list do
 *     if person_name is in the line
 *       replace person_name with a mask
 *       append the masked line into a file
 *   done
 * done
 *
 * A trie is used to avoid the brute force.
 *
 * Danylo Ulianych
 * Jul 1, 2020
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/stat.h>

#define   MAX_LINE_LENGTH    512   // Max record size
#define   REPLACE_CHAR       '|'
#define   PROGRESS_EACH      100000
#define   ALPHABET_SIZE      26    // 26 English lowercase letters

char line_mask[MAX_LINE_LENGTH];
char line_lower[MAX_LINE_LENGTH];

// Counts num. of times a char was toggled (uppercase).
// The last value of each mask size line counts the total number
// of masks of such size. 
uint64_t toggle_stat[MAX_LINE_LENGTH][MAX_LINE_LENGTH + 1];

typedef struct TrieNode TrieNode;

struct TrieNode {
    // The Trie Node Structure
    // Each node has N children, starting from the root
    // and a flag to check if it's a leaf node
    int8_t is_leaf;
    const TrieNode *parent;
    char letter;
    TrieNode* children[ALPHABET_SIZE];
    uint64_t count;  // num. of times the name occured in a password
};

TrieNode* make_trienode(const TrieNode *parent, const char letter) {
    // Allocate memory for a TrieNode
    TrieNode* node = (TrieNode*) calloc (1, sizeof(TrieNode));
    int32_t i;
    for (i=0; i<ALPHABET_SIZE; i++) {
        node->children[i] = NULL;
    }
    node->parent = parent;
    node->letter = letter;
    node->is_leaf = 0;
    node->count = 0U;
    return node;
}

void free_trienode(TrieNode* node) {
    // Free the trienode sequence
    int32_t i;
    for(i=0; i<ALPHABET_SIZE; i++) {
        if (node->children[i] != NULL) {
            free_trienode(node->children[i]);
        } 
    }
    free(node);
}

/**
 * Build a trie from names buffer.
 * Names in the names buffer do not need to be sorted by length.
 */
void build_trie(TrieNode *root, const char *names_buf, const int32_t buf_size) {
    int32_t i, node_id;
    TrieNode *node = root;
    for (i=0; i<buf_size; i++) {
        if (names_buf[i] == '\n') {
            node->is_leaf = 1;
            // reset
            node = root;
        } else {
            node_id = (int32_t) names_buf[i] - 'a';
            if (node->children[node_id] == NULL) {
                node->children[node_id] = make_trienode(node, names_buf[i]);
            }
            node = node->children[node_id];
        }
    }
}


/**
 * Read names from 'path' and build a trie.
 */
int8_t build_trie_from_path(TrieNode *root, const char *path)
{
    /* declare a file pointer */
    FILE    *names_file;

    /* open an existing file for reading */
    names_file = fopen(path, "r");

    /* quit if the file does not exist */
    if(names_file == NULL)
        return 1;

    /* Get the number of bytes */
    fseek(names_file, 0L, SEEK_END);
    const int32_t names_buf_size = ftell(names_file);

    /* reset the file position indicator to 
       the beginning of the file */
    fseek(names_file, 0L, SEEK_SET);	

    /* grab sufficient memory for the 
       names to hold the text */
    char *names = (char*)calloc(names_buf_size, sizeof(char));	

    /* memory error */
    if(names == NULL) {
        fclose(names_file);
        return 1;
    }

    /* copy all the text into the names */
    size_t bytes_read;
    bytes_read = fread(names, sizeof(char), names_buf_size, names_file);
    
    fclose(names_file);
    
    if (bytes_read != names_buf_size) {
        free(names);
        return 1;
    }

    // build a trie
    build_trie(root, names, names_buf_size);

    free(names);

    return 0;
}

/**
 * Find all name substrings in a line, replace matches with REPLACE_CHAR,
 * and write output masks in a file. The search is case insensitive.
 * Example:
 *   line: 3oLeg!michel_92#
 *   names in 'root' trie: michel, tanya, sofi, oleg, ...
 *   output masks: 3||||!michel_92#, 3oLeg!||||||_92#
 */
void write_matches(TrieNode *root, FILE *output_file, const char *line) {
    // line ends with a new line
    const size_t L = strlen(line);
    int32_t i;
    for (i=0; i<L; i++) {
        line_lower[i] = tolower(line[i]);
    }
    line_mask[L] = '\0';

    TrieNode *node, *longest_match_node;
    int32_t start, index, char_id, longest_index;
    uint64_t *toggle;
    for (start=0; start<L; start++) {
        node = root;
        longest_match_node = NULL;
        for (index=start; index<L+1; index++) {
            if (node == NULL) {
                break;
            }
            if (node->is_leaf) {
                longest_index = index;
                longest_match_node = node;
            }
            if (index == L) {
                break;
            }
            char_id = (int32_t) line_lower[index] - 'a';
            if (char_id < 0 || char_id >= ALPHABET_SIZE) {
                // unknown character
                break;
            }
            node = node->children[char_id];
        }
        if (longest_match_node != NULL) {
            longest_match_node->count++;
            memmove(line_mask, line, L);
            toggle = toggle_stat[longest_index - start];
            toggle[MAX_LINE_LENGTH]++;
            for (i=start; i<longest_index; i++) {
                line_mask[i] = REPLACE_CHAR;
                toggle[i - start] += line[i] != line_lower[i];
            }
            // line and line_mask already end with '\n'
            fputs(line_mask, output_file);
        }
    }
}


/**
 * Write matches for each line in 'wordlist_path' file.
 */
int8_t write_matches_from_wordlist(TrieNode *root, char *wordlist_path) { 
    struct stat st = {0};
    if (stat("masks", &st) == -1) {
        // directory does not exist
        if (mkdir("masks", 0700) != 0) {
            fprintf(stderr, "Could not create 'masks' folder.\n");
            return 1;
        }
    }

    const char output_file_path[] = "masks/masks.raw";
    FILE *output_file = fopen(output_file_path, "w");
    if (output_file == NULL) {
        fprintf(stderr, "Couldn't open '%s' file for writing.\n", output_file_path);
        return 1;
    }

    FILE* wordlist_file = fopen(wordlist_path, "r");
    if (wordlist_file == NULL) {
        fprintf(stderr, "Wrong wordlist path: %s\n,", wordlist_path);
        return 1;
    }

    char line[MAX_LINE_LENGTH];
    int64_t count = 0;
    while (fgets(line, sizeof(line), wordlist_file)) {
        /* note that fgets don't strip the terminating \n, checking its
           presence would allow to handle lines longer that sizeof(line) */
        write_matches(root, output_file, line);
        if (++count % PROGRESS_EACH == 0) {
            printf("\rProcessed %ld lines", count);
            fflush(stdout);
        }
    }
    printf("\rProcessed %ld lines\n", count);
    fflush(stdout);

    if (ferror(output_file) != 0) {
        fprintf(stderr, "Error is occured while writing to the output file. Please re-run the script.");
    }
    clearerr(output_file);
    printf("Wrote masked lines in %s\n", output_file_path);

    fclose(output_file);
    fclose(wordlist_file);

    return 0;
}


void TrieNode_CountLeafs(const TrieNode *node, uint32_t *count) {
    if (node == NULL) {
        return;
    }
    if (node->is_leaf && node->count > 0) {
        (*count)++;
    }
    int32_t node_id;
    TrieNode *child;
    for (node_id=0; node_id < ALPHABET_SIZE; node_id++) {
        child = node->children[node_id];
        if (child != NULL) {
            TrieNode_CountLeafs(child, count);
        }
    } 
}

void TrieNode_WriteLeafs(const TrieNode *node, TrieNode const **list, uint32_t *cur_list_item_id) {
    if (node == NULL) {
        return;
    }
    if (node->is_leaf && node->count > 0) {
        list[*cur_list_item_id] = node;
        (*cur_list_item_id)++;
    }
    int32_t node_id;
    TrieNode *child;
    for (node_id=0; node_id < ALPHABET_SIZE; node_id++) {
        child = node->children[node_id];
        if (child != NULL) {
            TrieNode_WriteLeafs(child, list, cur_list_item_id);
        }
    }
}


int compare_node_counts(const void *pa, const void *pb) {
    const TrieNode *a = *(const TrieNode **)pa;
    const TrieNode *b = *(const TrieNode **)pb;
    if (b->count > a->count) {
        return 1;
    } else if (b->count < a->count) {
        return -1;
    } else {
        return 0;
    }
}


/**
 * Write most used people names with counts.
 */
int8_t write_names_count(const TrieNode *root, const char *path) {
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        fprintf(stderr, "Couldn't open '%s' to write names count\n", path); 
        return 1;
    }

    uint32_t num_names = 0;
    TrieNode_CountLeafs(root, &num_names);
    
    TrieNode const **list = (TrieNode const**) malloc(num_names * sizeof(TrieNode*));

    uint32_t cur_list_item_id = 0;
    TrieNode_WriteLeafs(root, list, &cur_list_item_id);

    qsort(list, num_names, sizeof(list[0]), compare_node_counts);
    
    char buffer_info[2*MAX_LINE_LENGTH];
    char name_reversed[MAX_LINE_LENGTH];

    uint32_t name_id;
    int32_t i;
    const TrieNode *node;
    char *pbuffer;
    for (name_id = 0; name_id < num_names; name_id++) {
        node = list[name_id];
        pbuffer = buffer_info;
        pbuffer += sprintf(buffer_info, "%lu ", node->count);
        i = 0;
        while (node != root) {
            name_reversed[i++] = node->letter;
            node = node->parent;
        }
        while (i > 0) {
            *(pbuffer++) = name_reversed[--i];
        }
        *(pbuffer++) = '\n';
        *pbuffer = '\0';
        fputs(buffer_info, file);
    }

    free(list);
    fclose(file);

    printf("Wrote most used names in %s\n", path);

    return 0;
}


int8_t write_toggle_count(const char *path) {
    FILE *toggle_file = fopen(path, "w");
    if (toggle_file == NULL) {
        return 1;
    }
    int32_t line_size, offset, i;
    uint64_t *toggle;
    char info[1024];
    fputs("Toggle statistics.\nColumns:\n\t(1) mask size;\n\t(2) masks count;\n\t(3+) num. of times the positional character has been toggled (to uppercase).\n", toggle_file);
    for (line_size = 0; line_size < MAX_LINE_LENGTH; line_size++) {
        toggle = toggle_stat[line_size];
        if (toggle[MAX_LINE_LENGTH] > 0) {
            offset = sprintf(info, "size %d (total %lu): ", line_size, toggle[MAX_LINE_LENGTH]);
            for (i = 0; i < line_size; i++) {
                offset += sprintf(info + offset, "%lu ", toggle[i]);
            }
            info[offset] = '\n';
            info[offset + 1] = '\0';
            fputs(info, toggle_file);
        }
    }

    fclose(toggle_file);

    printf("Wrote toggle characters statistics in %s\n", path);

    return 0;
}


/**
 * Create masks of how people names are used as passwords.
 * 
 * Usage: ./create_masks.o /path/to/names /path/to/wordlist
 * The output masked lines will be stored in masks/masks.raw.
 */
int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: ./create_masks.o /path/to/names.ascii /path/to/wordlist\n");
        return 1; 
    }

    TrieNode* root = make_trienode(NULL, '\0');
    int8_t status_code;
    status_code = build_trie_from_path(root, argv[1]);
    if (status_code != 0) {
        fprintf(stderr, "Error while reading names file %s\n", argv[1]);
        return status_code;
    }

    status_code = write_matches_from_wordlist(root, argv[2]);
    if (status_code != 0) {
        free_trienode(root);
        return status_code;
    }

    // write most used names statistics
    status_code = write_names_count(root, "names/names.count");

    status_code |= write_toggle_count("masks/toggle_statistics.txt");
    
    /* free the used memory */
    free_trienode(root);

    return status_code;
}
