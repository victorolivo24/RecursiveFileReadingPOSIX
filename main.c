#include "compare.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef TEST_JSD
static char *xstrdup_main(const char *s) {
    size_t len;
    char *copy;

    if (!s) {
        return NULL;
    }
    len = strlen(s);
    copy = (char *)malloc(len + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, s, len + 1);
    return copy;
}

static FileData *make_filedata_from_words(const char *path, const char **words, const size_t *counts, size_t n) {
    FileData *file = (FileData *)calloc(1, sizeof(FileData));
    if (!file) {
        return NULL;
    }
    file->path = xstrdup_main(path);
    if (!file->path) {
        free(file);
        return NULL;
    }
    for (size_t i = 0; i < n; i++) {
        for (size_t k = 0; k < counts[i]; k++) {
            insert_or_increment_word(&file->word_list, words[i]);
            file->total_words++;
        }
    }
    for (WordNode *node = file->word_list; node; node = node->next) {
        node->freq = (double)node->count / (double)file->total_words;
    }
    return file;
}

int main(void) {
    const char *w1[] = {"hi", "there"};
    size_t c1[] = {2, 2};
    const char *w2[] = {"hi", "out", "there"};
    size_t c2[] = {2, 1, 1};
    FileData *f1 = make_filedata_from_words("f1", w1, c1, 2);
    FileData *f2 = make_filedata_from_words("f2", w2, c2, 3);
    double jsd;

    if (!f1 || !f2) {
        fprintf(stderr, "Error: test setup failed.\n");
        return 1;
    }

    jsd = compute_jsd(f1, f2);
    printf("JSD test: %.4f (expected about 0.3945)\n", jsd);
    return 0;
}
#elif defined(TEST_TOKENIZER)
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    FileData *file = process_file(argv[1]);
    if (!file) {
        fprintf(stderr, "Error: failed to process file.\n");
        return 1;
    }

    printf("File: %s\n", file->path);
    printf("Total words: %zu\n", file->total_words);
    for (WordNode *node = file->word_list; node; node = node->next) {
        printf("%s\t%zu\t%.6f\n", node->word, node->count, node->freq);
    }

    return 0;
}
#else
int main(int argc, char **argv) {
    FileData *files = NULL;
    size_t file_count = 0;
    Comparison *comps = NULL;
    size_t comp_count = 0;
    int had_error = 0;

    /* Example: collect_files uses argv[1..] as file/dir inputs. */
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path1> [path2 ...]\n", argv[0]);
        return 1;
    }

    {
        int rc = collect_files(argc, argv, &files, &file_count);
        if (rc < 0) {
            fprintf(stderr, "Error: failed to collect files.\n");
            return 1;
        }
        if (rc > 0) {
            had_error = 1;
        }
    }

    if (file_count < 2) {
        fprintf(stderr, "Error: need at least two files to compare.\n");
        free_filedata_array(files, file_count);
        return 1;
    }

    comps = build_comparisons(files, file_count, &comp_count);
    if (!comps) {
        free_filedata_array(files, file_count);
        return 1;
    }
    sort_comparisons(comps, comp_count);
    print_results(comps, comp_count);

    free_comparisons(comps);
    free_filedata_array(files, file_count);

    return had_error ? 1 : 0;
}
#endif
