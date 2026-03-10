#include "compare.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int collect_files(int argc, char **argv, FileData **files, size_t *file_count) {
    (void)argc;
    (void)argv;
    (void)files;
    (void)file_count;
    return 0;
}

static void free_word_list(WordNode *head) {
    while (head) {
        WordNode *next = head->next;
        free(head->word);
        free(head);
        head = next;
    }
}

static void free_filedata(FileData *file) {
    if (!file) {
        return;
    }
    free(file->path);
    free_word_list(file->word_list);
    free(file);
}

FileData *process_file(const char *path) {
    int fd = -1;
    FileData *file = NULL;
    char *word = NULL;
    size_t wlen = 0;
    size_t wcap = 0;
    ssize_t nread;
    char buffer[4096];

    if (!path) {
        return NULL;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }

    file = (FileData *)calloc(1, sizeof(FileData));
    if (!file) {
        close(fd);
        return NULL;
    }
    file->path = strdup(path);
    if (!file->path) {
        close(fd);
        free_filedata(file);
        return NULL;
    }

    while ((nread = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < nread; i++) {
            char c = buffer[i];
            if (is_word_char(c)) {
                char nc = normalize_char(c);
                if (wlen + 1 >= wcap) {
                    size_t newcap = (wcap == 0) ? 32 : (wcap * 2);
                    char *tmp = (char *)realloc(word, newcap);
                    if (!tmp) {
                        close(fd);
                        free(word);
                        free_filedata(file);
                        return NULL;
                    }
                    word = tmp;
                    wcap = newcap;
                }
                word[wlen++] = nc;
            } else if (wlen > 0) {
                word[wlen] = '\0';
                if (!insert_or_increment_word(&file->word_list, word)) {
                    close(fd);
                    free(word);
                    free_filedata(file);
                    return NULL;
                }
                file->total_words++;
                wlen = 0;
            }
        }
    }

    if (nread < 0) {
        close(fd);
        free(word);
        free_filedata(file);
        return NULL;
    }

    if (wlen > 0) {
        word[wlen] = '\0';
        if (!insert_or_increment_word(&file->word_list, word)) {
            close(fd);
            free(word);
            free_filedata(file);
            return NULL;
        }
        file->total_words++;
    }

    close(fd);
    free(word);

    if (file->total_words > 0) {
        for (WordNode *node = file->word_list; node; node = node->next) {
            node->freq = (double)node->count / (double)file->total_words;
        }
    }

    return file;
}

WordNode *insert_or_increment_word(WordNode **head, const char *word) {
    WordNode *prev = NULL;
    WordNode *curr = NULL;
    int cmp = 0;

    if (!head || !word) {
        return NULL;
    }

    curr = *head;
    while (curr && (cmp = strcmp(curr->word, word)) < 0) {
        prev = curr;
        curr = curr->next;
    }

    if (curr && cmp == 0) {
        curr->count++;
        return curr;
    }

    WordNode *node = (WordNode *)calloc(1, sizeof(WordNode));
    if (!node) {
        return NULL;
    }
    node->word = strdup(word);
    if (!node->word) {
        free(node);
        return NULL;
    }
    node->count = 1;
    node->freq = 0.0;
    node->next = curr;

    if (prev) {
        prev->next = node;
    } else {
        *head = node;
    }

    return node;
}

int is_word_char(char c) {
    unsigned char uc = (unsigned char)c;
    return isalnum(uc) || c == '-';
}

char normalize_char(char c) {
    unsigned char uc = (unsigned char)c;
    if (isalpha(uc)) {
        return (char)tolower(uc);
    }
    return c;
}

double compute_jsd(const FileData *a, const FileData *b) {
    (void)a;
    (void)b;
    return 0.0;
}

Comparison *build_comparisons(FileData *files, size_t file_count, size_t *comparison_count) {
    (void)files;
    (void)file_count;
    if (comparison_count) {
        *comparison_count = 0;
    }
    return NULL;
}

void print_results(const Comparison *comps, size_t count) {
    (void)comps;
    (void)count;
}
