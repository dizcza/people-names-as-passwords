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

#define   QUANTILE_HIST_BINS    80
#define   QUANTILE_HIST_HEIGHT  20

// temp buffer to generate password candidates; allocated once
char candidate[MAX_LINE_LENGTH];

// the first line in the names buffer with the specified length
int32_t first_line_of_length[MAX_LINE_LENGTH];


typedef struct MaskMode {
    uint8_t single_char_mode;
    uint8_t wpa_mode;
    uint8_t verbose;
    int32_t top_masks;
    int32_t toggle_id;
    uint8_t quantile;
} MaskMode;


/* Functions declaration */
int32_t get_column_offset(const char *line);


int8_t read_names(const char *path, char ***name_lines, int32_t *num_lines)
{
    FILE *names_file = fopen(path, "r");

    /* quit if the file does not exist */
    if (names_file == NULL)
        return 1;

    /* Get the number of bytes */
    fseek(names_file, 0L, SEEK_END);
    const int32_t names_buf_size = ftell(names_file);

    /* reset the file position indicator to 
       the beginning of the file */
    fseek(names_file, 0L, SEEK_SET);	

    char *names_buf = (char*)calloc(names_buf_size, sizeof(char));	

    /* memory error */
    if (names_buf == NULL)
        return 1;

    /* copy all the text into the names */
    size_t bytes_read;
    bytes_read = fread(names_buf, sizeof(char), names_buf_size, names_file);

    fclose(names_file);

    if (bytes_read != names_buf_size) {
        free(names_buf);
        return 1;
    }

    *num_lines = 0;
    int32_t i;
    for (i = 0; i < names_buf_size; i++) {
        *num_lines += names_buf[i] == '\n';
    }

    *name_lines = (char**) malloc(*num_lines * sizeof(char*));
    if (*name_lines == NULL) {
        free(names_buf);
        return 1;
    }

    int32_t lid = 0, start = 0, name_size;
    for (i = 0; i < names_buf_size; i++) {
        if (names_buf[i] == '\n') {
            names_buf[i] = '\0';
            name_size = i - start;
            (*name_lines)[lid] = (char*) calloc(name_size, sizeof(char));
            memmove((*name_lines)[lid], names_buf + start, name_size);
            start = i + 1;
            lid++;
        }
    }

    free(names_buf);

    return 0;
}


int32_t name_start_index(const char *name_line) {
    int32_t i = 0;
    const int32_t size = strlen(name_line);
    while (i < size && name_line[i] == ' ') {
        // strip left spaces
        i++;
    }
    while (i < size) {
        if (name_line[i++] == ' ') {
            return i;
        }
    }

    // make start at 0 by default
    return 0;
}


int compare_length(const void *pa, const void *pb) {
    const char *a = *(const char **)pa;
    const char *b = *(const char **)pb;
    int32_t fa = (int32_t) strlen(a);
    int32_t fb = (int32_t) strlen(b);
    return fb - fa; 
}

int compare_count(const void *pa, const void *pb) {
    const char *a = *(const char **)pa;
    const char *b = *(const char **)pb;
    char *ignore;
    const int32_t a_count = strtol(a, &ignore, 10);
    const int32_t b_count = strtol(b, &ignore, 10);
    if (a_count == 0 || b_count == 0) {
        // could not extract the count number
        // the input strings are probably without counts
        return 0;
    }
    if (b_count > a_count) {
        return 1;
    } else if (b_count < a_count) {
        return -1;
    } else {
        return 0;
    }
}


void plot_cdf(const uint64_t *hist, const uint64_t total_count, const float icdf) {
    char hist_str[QUANTILE_HIST_HEIGHT + 2][QUANTILE_HIST_BINS + 1];
    const uint64_t bar_step = total_count / QUANTILE_HIST_HEIGHT;
    int32_t vline, hline;
    uint64_t height;
    float height_f;
    for (vline = 0; vline < QUANTILE_HIST_HEIGHT; vline++) {
        for (hline = 0; hline < QUANTILE_HIST_BINS; hline++) {
            height_f = ((float) hist[hline]) / bar_step;
            height = (uint64_t) height_f;
            if (height_f - height > 0.5f) {
                height++;
            }
            if (QUANTILE_HIST_HEIGHT - vline == height) {
                hist_str[vline][hline] = '*';
            } else {
                hist_str[vline][hline] = ' ';
            }
        }
        hist_str[vline][QUANTILE_HIST_BINS] = '\0';
    }

    // title
    strcpy(hist_str[QUANTILE_HIST_HEIGHT / 2] + 30, "CUMULATIVE DISTRIBUTION FUNCTION OF NAME COUNTS");
    
    // x axis
    for (hline = 0; hline < QUANTILE_HIST_BINS-1; hline++) {
        hist_str[QUANTILE_HIST_HEIGHT][hline] = '-';
    }
    hist_str[QUANTILE_HIST_HEIGHT][0] = '+';
    hist_str[QUANTILE_HIST_HEIGHT][QUANTILE_HIST_BINS - 1] = '>';

    // y axis
    //hist_str[0][0] = '^';
    for (vline = 0; vline < QUANTILE_HIST_HEIGHT; vline++) {
        hist_str[vline][0] = '|';
    }

    // quantile pointer
    for (hline = 0; hline < QUANTILE_HIST_BINS; hline++) {
        hist_str[QUANTILE_HIST_HEIGHT + 1][hline] = ' ';
    }
    int32_t quantile_pos = (int32_t) (icdf * QUANTILE_HIST_BINS);
    hist_str[QUANTILE_HIST_HEIGHT + 1][quantile_pos] = '^';

    // xlabel
    strcpy(hist_str[QUANTILE_HIST_HEIGHT + 1] + QUANTILE_HIST_BINS - 10, "quantile");
    
    hist_str[QUANTILE_HIST_HEIGHT][QUANTILE_HIST_BINS] = '\0';
    hist_str[QUANTILE_HIST_HEIGHT + 1][QUANTILE_HIST_BINS] = '\0';
    for (vline = 0; vline < QUANTILE_HIST_HEIGHT + 2; vline++) {
        fprintf(stderr, " %s\n", hist_str[vline]);
    }
 }


int8_t take_quantile(char ***name_lines, int32_t *num_names, const uint8_t quantile, const uint8_t verbose) {
    char *pname = (*name_lines)[0];
    if (name_start_index(pname) == 0) {
        // plain name strings without counts
        return 0;
    }
    qsort(*name_lines, *num_names, sizeof(pname), compare_count);

    const int32_t bin_width = *num_names / QUANTILE_HIST_BINS;
    uint64_t hist[QUANTILE_HIST_BINS];

    char *ignore;
    uint64_t total_count = 0;
    int32_t lid;
    int32_t hline = 0;
    hist[0] = 0U;
    for (lid = 0; lid < *num_names; lid++) {
        total_count += strtol((*name_lines)[lid], &ignore, 10);
        if (lid / bin_width > hline) {
            hline = lid / bin_width;
            hist[hline] = total_count;
        }
    }
    hist[QUANTILE_HIST_BINS - 1] = total_count;
    
    const uint64_t count_quantile = (uint64_t) (((float) total_count) / 100.f * quantile);
    
    lid = 0;
    uint64_t count = 0;
    while (lid < *num_names && count < count_quantile) {
        count += strtol((*name_lines)[lid++], &ignore, 10);
    }
  
    const int32_t new_num_names = lid; 
    if (verbose) {
        fprintf(stderr, "[INFO] Choosing top %d names that corresponds to the %d quantile.\n", new_num_names, quantile);
    }
    if (verbose >= 2) {
        const float icdf = ((float) new_num_names) / *num_names;
        plot_cdf(hist, total_count, icdf);
    }
     
    if (new_num_names == *num_names) {
        // do nothing
        return 0;
    }

    char **new_name_lines = (char**) malloc(new_num_names * sizeof(char*));
    if (new_name_lines == NULL) {
        return 1;
    }

    int32_t start, size;
    for (lid = 0; lid < new_num_names; lid++) {
        pname = (*name_lines)[lid];
        start = name_start_index(pname);
        size = strlen(pname + start);
        new_name_lines[lid] = (char*) calloc(size, sizeof(char));
        memmove(new_name_lines[lid], pname + start, size);
    }

    for (lid = 0; lid < *num_names; lid++) {
        free((*name_lines)[lid]);
    }
    free(*name_lines);

    *name_lines = new_name_lines;
    *num_names = new_num_names;


    return 0;
}


int8_t process_first_line_of_lengths(const char **name_lines, const int32_t num_lines, const uint8_t verbose) {
    int32_t size;
    for (size = 0; size < MAX_LINE_LENGTH; size++){
        first_line_of_length[size] = -1;
    }

    // find the first line of a given length in the sorted array
    int32_t lid, size_upper = MAX_LINE_LENGTH;
    for (lid = 0; lid < num_lines; lid++) {
        size = strlen(name_lines[lid]); 
        if (size < size_upper) {
            first_line_of_length[size] = lid;
            size_upper = size;
        } else if (size > size_upper) {
            fprintf(stderr, "Input name strings must be sorted by length in descending order. Exiting.\n");
            return 1;
        }
    }

    if (verbose == 3) {
        fprintf(stderr, "[DEBUG] first_line_of_length:\n");
        int32_t i, max_size = MAX_LINE_LENGTH - 1;
        while (max_size > 0 && first_line_of_length[max_size] == -1) {
            max_size--;
        }
        for (i = 0; i <= max_size; i++) {
            fprintf(stderr, "%d ", first_line_of_length[i]);
        }
        fprintf(stderr, "\n");
    }

    return 0;
}


int8_t generate_passwords(const char *line, const char **name_lines, const int32_t num_lines, const MaskMode *mask_mode) {
    const size_t line_size = strlen(line);
    if (line_size == 1) {
        // only a new line char; skip
        return 0;
    }
    const int32_t offset = get_column_offset(line);
    if (offset == -1) {
        // no valid mask characters found
        return 0;
    }
    const int32_t candidate_size = line_size - offset;  // including a new line char
    if (!mask_mode->single_char_mode && mask_mode->wpa_mode && candidate_size < 9) {
        // not valid for WPA passwords
        return 0;
    }

    int32_t mask_start = offset;
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
        fprintf(stderr, "[ERROR] Expected single-char masks when the single-char mode is enabled. Found masked line '%s'\n", line);
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
        
            if (strlen(name_lines[0]) < size_smaller) {
                // the first line in (sorted by length) strings is smaller
                // than what is needed for WPA attack. Exiting.
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
    if (stop_line == -1) stop_line = num_lines;


    mask_start -= offset;
    mask_end -= offset;
    memmove(candidate, (char*) (line + offset), candidate_size);
    candidate[candidate_size] = '\0';  // candidate[candidate_size-1] points to a new line
    
    int32_t lid, suffix_size, toggle_pos;
    int32_t name_size = strlen(name_lines[first_line]);
    for (lid = first_line; lid < stop_line; lid++) {
        if (mask_mode->single_char_mode) {
            name_size = strlen(name_lines[lid]);
        }
        memmove((char*)(candidate + mask_start), name_lines[lid], name_size);
        if (mask_mode->single_char_mode) {
            suffix_size = candidate_size - mask_end + 1;
            memmove((char*)(candidate + mask_start + name_size),
                    (char*)(line + offset + mask_end),
                    suffix_size);
        }
        
        if (mask_mode->toggle_id != -1 && name_size > mask_mode->toggle_id) {
            toggle_pos = mask_start + mask_mode->toggle_id;
            candidate[toggle_pos] = toupper(candidate[toggle_pos]);
        }

        printf("%s", candidate);
    }

    return 0;
}


int32_t get_column_offset(const char *line) {
    const size_t line_size = strlen(line);
    if (line_size == 1) {
        // only a new line char; skip
        return -1;
    }
    
    int32_t i = 0;
    while (i < line_size && line[i] != REPLACE_CHAR) {
        i++;
    }
    if (i == line_size) {
        fprintf(stderr, "No valid mask characters '%c' in the input line.\n", REPLACE_CHAR);
        return -1;
    }
    while (i >= 0 && line[i] != ' ') {
        i--;
    }
    if (i == -1) {
        // no leading spaces in the input masks
        i = 0;
    } else {
        i++;
        // now 'i' points to the start of a mask
    }
    return i;
}


int8_t generate_passwords_from_path(const char *masks_stats_path, const char **name_lines, const int32_t num_lines, const MaskMode *mask_mode) {
    FILE *masks_stats_file = fopen(masks_stats_path, "r");
    if (masks_stats_file == NULL) {
        return 1;
    }
    char mask_line[MAX_LINE_LENGTH];

    int32_t lid = 0;
    int8_t status_code = 0;
    while (fgets(mask_line, sizeof(mask_line), masks_stats_file)) {
        if (mask_mode->top_masks != -1 && lid >= mask_mode->top_masks) {
            break;
        }
        status_code = generate_passwords(mask_line, name_lines, num_lines, mask_mode);
        if (status_code != 0) {
            return status_code;
        }
        lid++;
    }

    fclose(masks_stats_file);

    return 0;
}


void MaskMode_PrintChosenOptions(const MaskMode *mask_mode, const char *masks_stats_path) {
    if (mask_mode->verbose == 0) {
        return;
    }
    if (mask_mode->top_masks != -1) {
        fprintf(stderr, "[INFO] Choosing top %d masks from '%s'\n", mask_mode->top_masks, masks_stats_path);
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
 *   -m <top-masks>   Choose only top 'm' masks from /path/to/masks.stats.
 *                    By default, all masks are chosen. Example:
 *                    ./generate -m 1000 ...
 *
 *   -T <toggle-id>   Toggle character by id.
 */
int main(int argc, char* argv[]) {
    opterr = 0;
    int c;
    char *ignore;
    MaskMode mask_mode = {
        .single_char_mode=0U,
        .wpa_mode=0U,
        .verbose=0U,
        .top_masks=-1,
        .toggle_id=-1,
        .quantile=98
    };

    while ((c = getopt(argc, argv, "swvm:T:q:")) != -1) {
        switch (c) {
            case 's':
                mask_mode.single_char_mode = 1U;
                break;
            case 'w':
                mask_mode.wpa_mode = 1U;
                break;
            case 'v':
                mask_mode.verbose++;
                break;
            case 'm':
                mask_mode.top_masks = strtol(optarg, &ignore, 10);
                break;
            case 'T':
                mask_mode.toggle_id = strtol(optarg, &ignore, 10);
                break;
            case 'q':
                mask_mode.quantile = strtol(optarg, &ignore, 10);
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
        fprintf(stderr, "Usage: ./generate [-s] [-w] [-m top-masks] [-T toggle-id] /path/to/names.all /path/to/masks.stats\n");
        return 1;
    }

    char *names_file_path = argv[optind];
    char *masks_stats_path = argv[optind + 1];

    MaskMode_PrintChosenOptions(&mask_mode, masks_stats_path);
  
    int32_t num_lines, lid;
    char **name_lines;
    if (read_names(names_file_path, &name_lines, &num_lines) != 0) {
        fprintf(stderr, "Error while reading names file %s\n", names_file_path);
        return 1;
    }

    if (take_quantile(&name_lines, &num_lines, mask_mode.quantile, mask_mode.verbose) != 0) {
        fprintf(stderr, "[ERROR] Error while estimating the quantile.\n");
        goto MAIN_CLEANUP;
    }

    qsort(name_lines, num_lines, sizeof(name_lines[0]), compare_length);

    if (process_first_line_of_lengths((const char**) name_lines, num_lines, mask_mode.verbose) != 0) {
        goto MAIN_CLEANUP;
    }

    int8_t status_code = generate_passwords_from_path(masks_stats_path, (const char**) name_lines, num_lines, &mask_mode);


MAIN_CLEANUP:

    for (lid = 0; lid < num_lines; lid++) {
        free(name_lines[lid]);
    }
    free(name_lines);

    if (status_code != 0) {
        fprintf(stderr, "Error orrured during password candidates generation.\n");
    }

    return status_code;
}
