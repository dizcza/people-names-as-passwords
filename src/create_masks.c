/*
   Danylo Ulianych
   Jul 1, 2020
   */

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define   MAX_LINE_LENGTH    512 // Max record size
#define   REPLACE_CHAR  ('|')
#define   DEBUG 0
#define   PROGRESS_EACH 10000

char     *names;
int32_t   names_buf_size;
int32_t  *names_end;
int32_t   names_num=0;

char line_lower[MAX_LINE_LENGTH];  // temp buffer, allocated once
char line_mask[MAX_LINE_LENGTH];

FILE *output_file;

int8_t save_names_end_positions() {
    int32_t char_id;
    for (char_id = 0; char_id < names_buf_size; char_id++) {
        names_num += names[char_id] == '\n';
    }
    names_end = (int32_t*)calloc(names_num, sizeof(int32_t));

    if (names_end == NULL)
        return 1;

    int32_t lid = 0;
    for (char_id = 0; char_id < names_buf_size; char_id++) {
        if (names[char_id] == '\n') {
            names_end[lid++] = char_id;
            names[char_id] = '\0';
        }
    }
    return 0;
}


int8_t read_names(char *path)
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
    names_buf_size = ftell(names_file);

    /* reset the file position indicator to 
       the beginning of the file */
    fseek(names_file, 0L, SEEK_SET);	

    /* grab sufficient memory for the 
       names to hold the text */
    names = (char*)calloc(names_buf_size, sizeof(char));	

    /* memory error */
    if(names == NULL)
        return 1;

    /* copy all the text into the names */
    fread(names, sizeof(char), names_buf_size, names_file);
    fclose(names_file);

    return 0;
}


int8_t process_line(char *line) {
    size_t size = strlen(line);
    int32_t start_match;
    int32_t replace_id;
    int32_t size_name;
    int32_t start=0, end=1;
    char *p_mask;

    size_t lid;

    // lowercase the string
    memmove(line_lower, line, size);
    line_lower[size] = '\0';

    char *p = line_lower;
    for ( ; *p; ++p) {
        if (*p == REPLACE_CHAR) {
            // don't process a line with the REPLACE_CHAR
            return 0;
        }
        *p = tolower(*p);
    }

    if (DEBUG)
        printf("Scanning %s", line);

    start = 0;
    for (lid = 0; lid < names_num; lid++) {
        // find the location 'p' of a substring 'name'
        // on the interval between names + start and names + start + end
        end = names_end[lid];
        if (DEBUG) {
            printf("\t\tchecking name %s", (char*)(names + start));
        }
        p = strstr(line_lower, (char*) (names + start));
        if (p != NULL) {
            memmove(line_mask, line, size);
            line_mask[size] = '\0';
            start_match = (int32_t)(p - line_lower);
            if (DEBUG) {
                printf("\tMatch '%s' at pos=%d in %s", names+start, start_match, line);
            }
            size_name = end - start;
            p_mask = (char*) (line_mask + start_match);
            for (replace_id = 0; replace_id < size_name; replace_id++) {
                p_mask[replace_id] = REPLACE_CHAR;
            }
            //printf("%s", line_mask);
            fputs(line_mask, output_file);
        }
        start = end + 1;


    }

    return 0;
}


int8_t read_wordlist(char *wordlist_path) {
    FILE* wordlist_file = fopen(wordlist_path, "r");
    if (wordlist_file == NULL) {
        return 1;
    }
    char line[MAX_LINE_LENGTH];

    struct stat st = {0};
    if (stat("masks", &st) == -1) {
        // directory does not exist
        if (mkdir("masks", 0700) != 0) {
            printf("Could not create 'masks' folder.\n");
            return 1;
        }
    }

    output_file = fopen("masks/masks.raw", "w");
    if (output_file == NULL) {
        printf("Couldn't open the output file for writing. Exiting\n");
        return 1;
    }

    int64_t count = 0;
    while (fgets(line, sizeof(line), wordlist_file)) {
        /* note that fgets don't strip the terminating \n, checking its
           presence would allow to handle lines longer that sizeof(line) */
        process_line(line);
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


int main(int argc, char* argv[]) {
    // usage: ./create_masks.o /path/to/names /path/to/wordlist

    int8_t status_code;
    status_code = read_names(argv[1]);
    if (status_code != 0) {
        printf("Error while reading names file. Exiting\n");
        return status_code;
    }

    status_code = save_names_end_positions();
    if (status_code != 0) {
        printf("Error while preprocessing names pattern strings. Exiting\n");
        return status_code;
    }

    status_code = read_wordlist(argv[2]);
    if (status_code != 0) {
        printf("Error while reading wordlist file. Exiting\n");
        return status_code;
    }

    /* free the used memory */
    free(names);
    free(names_end);

    return 0;
}
