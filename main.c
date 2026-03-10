#include "compare.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef TEST_TOKENIZER
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\\n", argv[0]);
        return 1;
    }

    FileData *file = process_file(argv[1]);
    if (!file) {
        fprintf(stderr, "Error: failed to process file.\\n");
        return 1;
    }

    printf("File: %s\\n", file->path);
    printf("Total words: %zu\\n", file->total_words);
    for (WordNode *node = file->word_list; node; node = node->next) {
        printf("%s\\t%zu\\t%.6f\\n", node->word, node->count, node->freq);
    }

    return 0;
}
#else
int main(int argc, char **argv) {
    FileData *files = NULL;
    size_t file_count = 0;
    Comparison *comps = NULL;
    size_t comp_count = 0;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <file1> <file2> [file3 ...]\n", argv[0]);
        return 1;
    }

    if (collect_files(argc, argv, &files, &file_count) != 0) {
        fprintf(stderr, "Error: failed to collect files.\n");
        return 1;
    }

    comps = build_comparisons(files, file_count, &comp_count);
    print_results(comps, comp_count);

    /* TODO: cleanup resources */
    (void)comps;
    (void)files;

    return 0;
}
#endif
