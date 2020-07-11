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

char candidate[MAX_LINE_LENGTH];
int32_t first_line_of_length[MAX_LINE_LENGTH];


char* read_names(const char *path, int32_t *names_buf_size)
{
    /* declare a file pointer */
    FILE    *names_file;

    /* open an existing file for reading */
    names_file = fopen(path, "r");

    /* quit if the file does not exist */
    if (names_file == NULL)
        return NULL;

    /* Get the number of bytes */
    fseek(names_file, 0L, SEEK_END);
    *names_buf_size = ftell(names_file);

    /* reset the file position indicator to 
       the beginning of the file */
    fseek(names_file, 0L, SEEK_SET);	

    /* grab sufficient memory for the 
       names to hold the text */
    char *names = (char*)calloc(*names_buf_size, sizeof(char));	

    /* memory error */
    if(names == NULL)
        return NULL;

    /* copy all the text into the names */
    size_t bytes_read;
    bytes_read = fread(names, sizeof(char), *names_buf_size, names_file);

    // cleanup
    fclose(names_file);

    if (bytes_read != *names_buf_size) {
        free(names);
        return NULL;
    }

    return names;
}


int32_t* make_names_end_positions(const char *names_buf, const int32_t names_buf_size) {
    int32_t char_id;
    int32_t names_num = 0;
    for (char_id = 0; char_id < names_buf_size; char_id++) {
        names_num += names_buf[char_id] == '\n';
    }
    int32_t *names_end = (int32_t*)calloc(names_num, sizeof(int32_t));

    if (names_end == NULL)
        return NULL;

    int32_t lid = 0, start=0, size=0, size_upper=MAX_LINE_LENGTH;
    for (size = 0; size < MAX_LINE_LENGTH; size++){
        first_line_of_length[size] = -1;
    }

    for (char_id = 0; char_id < names_buf_size; char_id++) {
        if (names_buf[char_id] == '\n') {
            names_end[lid] = char_id;
            size = char_id - start;
            if (size < size_upper) {
                first_line_of_length[size] = lid;
                size_upper = size;
            }
            lid++;
            start = char_id + 1;
        }
    }

    // point empty line to the end of the list
    first_line_of_length[0] = names_num;

    return names_end;
}


void generate_passwords(const char *line, const int32_t offset, const char *names_buf, const int32_t *names_end) {
    const size_t line_size = strlen(line);
    int32_t mask_start=0;
    while (mask_start<line_size && line[mask_start] != REPLACE_CHAR) {
        mask_start++;
    }
    if (mask_start == line_size) {
        // not found
        return;
    }
    int32_t mask_end=mask_start;
    while (mask_end<line_size && line[mask_end] == REPLACE_CHAR) {
        mask_end++;
    }
    // mask_end points to the next char after REPLACE_CHAR
    const int32_t mask_size = mask_end - mask_start;

    const int32_t first_line = first_line_of_length[mask_size];
    if (first_line == -1) {
        // no pattern names with such mask length
        return;
    }
    
    int32_t size_smaller = mask_size - 1;
    while (size_smaller>0 && first_line_of_length[size_smaller] == -1) {
        size_smaller--;
    }
    const int32_t stop_line = first_line_of_length[size_smaller];

    const int32_t candidate_size = line_size - offset;
    mask_start -= offset;
    mask_end -= offset;
    memmove(candidate, (char*) (line + offset), candidate_size);
    candidate[candidate_size] = '\0';
    
    int32_t lid, name_start, i;
    if (first_line > 0) {
        name_start = names_end[first_line-1] + 1;
    } else {
        name_start = 0;
    }
    char *pname;
    for (lid = first_line; lid < stop_line; lid++) {
        pname = (char*) (names_buf + name_start);
        for (i = mask_start; i < mask_end; i++) {
            candidate[i] = *pname;
            pname++;
        }
        printf("%s", candidate);

        // names_end[lid] points to a new line char
        name_start = names_end[lid] + 1;
    }

}


int32_t get_line_column_offset(const char *line) {
    int32_t i = 0;
    const size_t line_size = strlen(line);
    while (i < line_size && line[i] == ' ') {
        // string left spaces
        i++;
    }
    while (i < line_size && line[i] != ' ') {
        // string word count
        i++;
    }

    if (i == 0 || i >= line_size - 1) {
        fprintf(stderr, "Did you pass masks.raw (wrong) or masks.stats (correct) file path?\n");
        return 0;
    }

    return i + 1;
}


int8_t generate_passwords_from_path(const char *masks_stats_path, const char *names_buf, const int32_t *names_end) {
    FILE *masks_stats_file = fopen(masks_stats_path, "r");
    if (masks_stats_file == NULL) {
        return 1;
    }
    char mask_line[MAX_LINE_LENGTH];

    int32_t offset = -1;
    while (fgets(mask_line, sizeof(mask_line), masks_stats_file)) {
        if (offset == -1 ) {
            // calculated only once
            offset = get_line_column_offset(mask_line);
        }
        generate_passwords(mask_line, offset, names_buf, names_end);
    }

    fclose(masks_stats_file);

    return 0;
}


int main(int argc, char* argv[]) {
    // usage: ./create_masks.o /path/to/names /path/to/wordlist

    char *names_buf;
    int32_t names_buf_size;
    names_buf = read_names(argv[1], &names_buf_size);
    if (names_buf == NULL) {
        printf("Error while reading names file %s\n", argv[1]);
        return 1;
    }

    int32_t *names_end;
    names_end = make_names_end_positions(names_buf, names_buf_size);
    if (names_end == NULL) {
        free(names_buf);
        return 1;
    }

    int8_t status_code;
    status_code = generate_passwords_from_path(argv[2], names_buf, names_end);
    

    // cleanup
    free(names_buf);
    free(names_end);

    if (status_code != 0) {
        fprintf(stderr, "Error orrured during password candidates generation.");
    }

    return status_code;
}
