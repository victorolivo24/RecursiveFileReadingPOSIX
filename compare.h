#ifndef COMPARE_H
#define COMPARE_H

#include <stddef.h>

#define DEFAULT_SUFFIX ".txt"

/* Core data structures */
typedef struct WordNode {
    char *word;
    size_t count;
    double freq;
    struct WordNode *next;
} WordNode;

typedef struct FileData {
    char *path;
    size_t total_words;
    WordNode *word_list; /* lexicographically sorted list */
} FileData;

typedef struct Comparison {
    FileData *file1;
    FileData *file2;
    size_t combined_word_count;
    double jsd;
} Comparison;

/* Collection phase */
int collect_files(int argc, char **argv, FileData **files, size_t *file_count);
FileData *process_file(const char *path);

/* Tokenization */
int is_word_char(char c);
char normalize_char(char c);

/* Word map */
WordNode *insert_or_increment_word(WordNode **head, const char *word);

/* Analysis phase */
double compute_jsd(const FileData *a, const FileData *b);
Comparison *build_comparisons(FileData *files, size_t file_count, size_t *comparison_count);
void sort_comparisons(Comparison *comps, size_t count);

/* Output */
void print_results(const Comparison *comps, size_t count);

/* Cleanup */
void free_word_list(WordNode *head);
void free_filedata_array(FileData *files, size_t file_count);
void free_comparisons(Comparison *comps);

#endif /* COMPARE_H */
