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
char buffer_info[2*MAX_LINE_LENGTH];

typedef struct TrieNode TrieNode;

struct TrieNode {
    // The Trie Node Structure
    // Each node has N children, starting from the root
    // and a flag to check if it's a leaf node
    int8_t is_leaf;
    TrieNode* children[ALPHABET_SIZE];
    uint64_t count;  // to allow running on >4B wordlists
};

TrieNode* make_trienode() {
    // Allocate memory for a TrieNode
    TrieNode* node = (TrieNode*) calloc (1, sizeof(TrieNode));
    int32_t i;
    for (i=0; i<ALPHABET_SIZE; i++)
        node->children[i] = NULL;
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
                node->children[node_id] = make_trienode();
            }
            node = node->children[node_id];
        }
    }
}


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
    if(names == NULL)
        return 1;

    /* copy all the text into the names */
    size_t bytes_read;
    bytes_read = fread(names, sizeof(char), names_buf_size, names_file);
    if (bytes_read != names_buf_size) {
        return 1;
    }

    fclose(names_file);

    // build a trie
    build_trie(root, names, names_buf_size);

    free(names);

    return 0;
}


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
            for (i=start; i<longest_index; i++) {
                line_mask[i] = REPLACE_CHAR;
            }
            // line and line_mask already end with '\n'
            fputs(line_mask, output_file);
        }
    }
}


int8_t write_matches_from_wordlist(TrieNode *root, char *wordlist_path) { 
    FILE* wordlist_file = fopen(wordlist_path, "r");
    if (wordlist_file == NULL) {
        return 1;
    }

    struct stat st = {0};
    if (stat("masks", &st) == -1) {
        // directory does not exist
        if (mkdir("masks", 0700) != 0) {
            printf("Could not create 'masks' folder.\n");
            return 1;
        }
    }

    FILE *output_file = fopen("masks/masks.raw", "w");
    if (output_file == NULL) {
        printf("Couldn't open the output file for writing. Exiting\n");
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

    fclose(output_file);
    fclose(wordlist_file);

    return 0;
}


void write_statistics(const TrieNode *node, FILE *matches_file, char *prefix, const int32_t prefix_len) {
    if (node == NULL) {
        return;
    }
    if (node->is_leaf && node->count > 0) {
        prefix[prefix_len] = '\0';
        sprintf(buffer_info, "%lu %s\n", node->count, prefix);
        fputs(buffer_info, matches_file);
    }
    int32_t node_id;
    TrieNode *child;
    for (node_id=0; node_id < ALPHABET_SIZE; node_id++) {
        child = node->children[node_id];
        if (child != NULL) {
            prefix[prefix_len] = (char) (node_id + 'a');
            write_statistics(child, matches_file, prefix, prefix_len+1);
        }
    }
}


int main(int argc, char* argv[]) {
    // usage: ./create_masks.o /path/to/names /path/to/wordlist

    TrieNode* root = make_trienode();
    int8_t status_code;
    status_code = build_trie_from_path(root, argv[1]);
    if (status_code != 0) {
        printf("Error while reading names file %s\n", argv[1]);
        return status_code;
    }

    status_code = write_matches_from_wordlist(root, argv[2]);
    if (status_code != 0) {
        printf("Error while parsing wordlist file %s\n", argv[2]);
        return status_code;
    }

    // write statistics
    FILE* matches_file = fopen("masks/most_used_names.txt", "w");
    if (matches_file == NULL) {
        fprintf(stderr, "Couldn't write statistics in 'masks/most_used_names.txt'\n");
        return 1;
    }
    char prefix[MAX_LINE_LENGTH];
    int32_t i;
    for (i=0; i<MAX_LINE_LENGTH; i++) {
        prefix[i] = 0;
    }
    write_statistics(root, matches_file, prefix, 0);
    fclose(matches_file);

    /* free the used memory */
    free_trienode(root);

    return 0;
}
