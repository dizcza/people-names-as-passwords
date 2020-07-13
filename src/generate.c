/*
 * Generate password candidates from masks and chosen people names.
 *
 * Danylo Ulianych
 * Jul 11, 2020
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

#define   MAX_LINE_LENGTH    512   // Max record size
#define   REPLACE_CHAR       '|'

// temp buffer to generate password candidates; allocated once
char candidate[MAX_LINE_LENGTH];

// the first line in the names buffer with the specified length
int32_t first_line_of_length[MAX_LINE_LENGTH];


typedef struct MaskMode {
    uint8_t single_char_mode;
    uint8_t wpa_mode;
    int32_t top_masks;
} MaskMode;


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


int compare_length(const void *pa, const void *pb) {
    const char *a = *(const char **)pa;
    const char *b = *(const char **)pb;
    int32_t fa = (int32_t) strlen(a);
    int32_t fb = (int32_t) strlen(b);
    return fb - fa; 
}


int8_t sort_names_by_length(char *names_buf, const int32_t names_buf_size) {
    int32_t char_id;
    int32_t names_num = 0, start = 0, max_length = 0;
    for (char_id = 0; char_id < names_buf_size; char_id++) {
        if (names_buf[char_id] == '\n') {
            names_num++;
            if (char_id - start > max_length) {
                max_length = char_id - start;
            }
            start = char_id + 1;
        }
    }
    char **names_start = (char**) malloc(sizeof(char*) * names_num);

    if (names_start == NULL)
        return 1;

    // each line of names_start is a fixed-length string
    int32_t lid = 0;
    start = 0;
    for (char_id = 0; char_id < names_buf_size; char_id++) {
        if (names_buf[char_id] == '\n') {
            names_buf[char_id] = '\0';
            names_start[lid] = (char*) calloc(max_length, sizeof(char));
            memmove(names_start[lid], names_buf + start, char_id - start);
            start = char_id + 1;
            lid++;
        }
    }

    // sort strings by length in descending order
    qsort(names_start, names_num, sizeof(names_start[0]), compare_length);

    // populate sorted names into contiguous input buffer
    size_t line_size;
    char *pname;
    char_id = 0;
    for (lid = 0; lid < names_num; lid++) {
        pname = names_start[lid];
        line_size = strlen(pname);
        memmove(names_buf + char_id, pname, line_size);
        names_buf[char_id + line_size] = '\n';  // keep original word delimiter
        char_id += line_size + 1;
    }

    for (lid = 0; lid < names_num; lid++) {
        free(names_start[lid]);
    }
    free(names_start);

    return 0;
}

int32_t* make_names_end_positions(const char *names_buf, const int32_t names_buf_size) {
    int32_t char_id;
    int32_t names_num = 0;
    for (char_id = 0; char_id < names_buf_size; char_id++) {
        names_num += names_buf[char_id] == '\n';
    }
    int32_t *names_end = (int32_t*) calloc(names_num, sizeof(int32_t));

    if (names_end == NULL)
        return NULL;

    int32_t size=0, size_upper=MAX_LINE_LENGTH;
    for (size = 0; size < MAX_LINE_LENGTH; size++){
        first_line_of_length[size] = -1;
    }

    // populate names_end buffer with ending positions and save
    // find the first line of a given length in the sorted array
    int32_t lid=0, start=0;
    for (char_id = 0; char_id < names_buf_size; char_id++) {
        if (names_buf[char_id] == '\n') {
            names_end[lid] = char_id;
            size = char_id - start;
            if (size < size_upper) {
                first_line_of_length[size] = lid;
                size_upper = size;
            } else if (size > size_upper) {
                fprintf(stderr, "Input 'names_buf' array must be sorted by length in descending order. Exiting.\n");
                free(names_end);
                return NULL;
            }
            lid++;
            start = char_id + 1;
        }
    }

    // point empty line to the end of the list
    first_line_of_length[0] = names_num;

    return names_end;
}


int8_t generate_passwords(const char *line, const int32_t offset, const char *names_buf, const int32_t *names_end, const MaskMode *mask_mode) {
    const size_t line_size = strlen(line);
    const int32_t candidate_size = line_size - offset;
    if (!mask_mode->single_char_mode && mask_mode->wpa_mode && candidate_size < 8) {
        // not valid for WPA passwords
        return 0;
    }

    int32_t mask_start = 0;
    while (mask_start<line_size && line[mask_start] != REPLACE_CHAR) {
        mask_start++;
    }
    if (mask_start == line_size) {
        // not found
        return 0;
    }
    int32_t mask_end = mask_start;
    while (mask_end < line_size && line[mask_end] == REPLACE_CHAR) {
        mask_end++;
    }
    // mask_end points to the next char after REPLACE_CHAR
    const int32_t mask_size = mask_end - mask_start;
    if (mask_mode->single_char_mode && mask_size != 1) {
        fprintf(stderr, "[ERROR] Expected single-char masks when the single-char mode is enabled. Found masked line %s\n", line);
        return 1;
    }

    int32_t first_line, stop_line, size_smaller;

    if (mask_mode->single_char_mode) {
        // loop through all names
        first_line = 0;
        size_smaller = 0;
        if (mask_mode->wpa_mode) {
            size_smaller = 8 - (candidate_size - 1);
            if (size_smaller < 0) size_smaller = 0;
        
            const int32_t first_line_size = names_end[0];
            if (first_line_size < size_smaller) {
                // all name lengths are smaller than needed for WPA attack
                return 0;
            }
        }
    } else {
        first_line = first_line_of_length[mask_size];
        if (first_line == -1) {
            // no pattern names with such mask length
            return 0;
        } 
        size_smaller = mask_size - 1;
    }
    while (size_smaller>0 && first_line_of_length[size_smaller] == -1) {
        size_smaller--;
    }
    stop_line = first_line_of_length[size_smaller];


    mask_start -= offset;
    mask_end -= offset;
    memmove(candidate, (char*) (line + offset), candidate_size);
    candidate[candidate_size] = '\0';  // candidate[candidate_size-1] points to a new line
    
    int32_t lid, name_start, name_size, suffix_size;
    if (first_line > 0) {
        name_start = names_end[first_line-1] + 1;
    } else {
        name_start = 0;
    }
    for (lid = first_line; lid < stop_line; lid++) {
        name_size = names_end[lid] - name_start;
        memmove((char*)(candidate + mask_start), (char*)(names_buf + name_start), name_size);
        if (mask_mode->single_char_mode) {
            suffix_size = candidate_size - mask_end + 1;
            memmove((char*)(candidate + mask_start + name_size),
                    (char*)(line + offset + mask_end),
                    suffix_size);
        }
        
        printf("%s", candidate);

        // names_end[lid] points to a new line char
        name_start = names_end[lid] + 1;
    }

    return 0;
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


int8_t generate_passwords_from_path(const char *masks_stats_path, const char *names_buf, const int32_t *names_end, const MaskMode *mask_mode) {
    FILE *masks_stats_file = fopen(masks_stats_path, "r");
    if (masks_stats_file == NULL) {
        return 1;
    }
    char mask_line[MAX_LINE_LENGTH];

    int32_t offset = -1, lid = 0;
    int8_t status_code = 0;
    while (fgets(mask_line, sizeof(mask_line), masks_stats_file)) {
        if (mask_mode->top_masks != -1 && lid >= mask_mode->top_masks) {
            break;
        }
        if (offset == -1 ) {
            // calculated only once
            offset = get_line_column_offset(mask_line);
        }
        status_code = generate_passwords(mask_line, offset, names_buf, names_end, mask_mode);
        if (status_code != 0) {
            return status_code;
        }
        lid++;
    }

    fclose(masks_stats_file);

    return 0;
}


void MaskMode_PrintChosenOptions(const MaskMode *mask_mode, const char *masks_stats_path) {
    if (mask_mode->top_masks != -1) {
        fprintf(stderr, "[INFO] Choosing %d top masks from '%s'\n", mask_mode->top_masks, masks_stats_path);
    } else {
        fprintf(stderr, "[INFO] Choosing all masks from '%s'\n", masks_stats_path);
    }
    if (mask_mode->single_char_mode) {
        fprintf(stderr, "[INFO] Enabling single-char mode. Make sure that all masks in '%s' are single-char masks\n", masks_stats_path);
    }
    if (mask_mode->wpa_mode) {
        fprintf(stderr, "[INFO] Enabling WPA mode\n");
    }
}


/**
 * Generate password candidates from names and masks.
 *
 * Usage: ./generate [-s] [-w] [-m top_masks] /path/to/names.ascii /path/to/masks.stats
 * Option flags:
 *   -s               Enable single char mode masks: each mask must constist of exactly 1 REPLACE_CHAR.
 *                    In single mode, the names of all lengths are substituted. Example:
 *                    '12|1992'  --> 12jan1992 12alex1992 12simon1992 12martina1992 12michael1992
 *
 *   -w               Enable WPA mode: print passwords that are at least 8 characters long.
 *
 *   -m <top_masks>   Choose only top 'm' masks from /path/to/masks.stats.
 *                    By default, all masks are chosen. Example:
 *                    ./generate -m 1000 ...
 */
int main(int argc, char* argv[]) {
    opterr = 0;
    int c;
    char *ignore;
    MaskMode mask_mode = {
        .single_char_mode=0U,
        .wpa_mode=0U,
        .top_masks=-1
    };

    while ((c = getopt(argc, argv, "swm:")) != -1) {
        switch (c) {
            case 's':
                mask_mode.single_char_mode = 1U;
                break;
            case 'w':
                mask_mode.wpa_mode = 1U;
                break;
            case 'm':
                mask_mode.top_masks = strtol(optarg, &ignore, 10);
                break;
            case '?':
                if (optopt == 't')
                  fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                  fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                else
                  fprintf (stderr,
                           "Unknown option character `\\x%x'.\n",
                           optopt);
                return 1;
            default:
                abort();
        }
    }

    if (argc - optind != 2) {
        fprintf(stderr, "Usage: ./generate [-s] [-w] [-m top_masks] /path/to/names.all /path/to/masks.stats\n");
        return 1;
    }

    char *names_file_path = argv[optind];
    char *masks_stats_path = argv[optind + 1];

    MaskMode_PrintChosenOptions(&mask_mode, masks_stats_path);
  
    int32_t names_buf_size;
    char *names_buf = read_names(names_file_path, &names_buf_size);
    if (names_buf == NULL) {
        fprintf(stderr, "Error while reading names file %s\n", names_file_path);
        return 1;
    }

    if (sort_names_by_length(names_buf, names_buf_size) != 0) {
        free(names_buf);
        fprintf(stderr, "Could not allocate memory to sort the 'names_buf' array.\n");
        return 1;
    }

    int32_t *names_end = make_names_end_positions(names_buf, names_buf_size);
    if (names_end == NULL) {
        free(names_buf);
        return 1;
    }

    int8_t status_code = generate_passwords_from_path(masks_stats_path, names_buf, names_end, &mask_mode);


MAIN_CLEANUP:
    free(names_buf);
    free(names_end);

    if (status_code != 0) {
        fprintf(stderr, "Error orrured during password candidates generation.\n");
    }

    return status_code;
}
