#include "compare.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void free_filedata(FileData *file);

static int has_suffix(const char *name, const char *suffix) {
    size_t nlen;
    size_t slen;

    if (!name || !suffix) {
        return 0;
    }
    nlen = strlen(name);
    slen = strlen(suffix);
    if (nlen < slen) {
        return 0;
    }
    return strcmp(name + (nlen - slen), suffix) == 0;
}

static int is_hidden_name(const char *name) {
    return name && name[0] == '.';
}

static int add_file(FileData *file, FileData **files, size_t *count, size_t *cap) {
    if (!file || !files || !count || !cap) {
        return -1;
    }
    if (*count + 1 > *cap) {
        size_t newcap = (*cap == 0) ? 8 : (*cap * 2);
        FileData *tmp = (FileData *)realloc(*files, newcap * sizeof(FileData));
        if (!tmp) {
            return -1;
        }
        *files = tmp;
        *cap = newcap;
    }
    (*files)[*count] = *file;
    (*count)++;
    return 0;
}

static int add_file_by_path(const char *path, FileData **files, size_t *count, size_t *cap) {
    FileData *file = process_file(path);
    if (!file) {
        perror(path);
        return -1;
    }
    if (add_file(file, files, count, cap) != 0) {
        free_filedata(file);
        return -1;
    }
    free(file);
    return 0;
}

static int traverse_dir(const char *dirpath, const char *suffix, FileData **files, size_t *count, size_t *cap) {
    DIR *dir;
    struct dirent *ent;

    dir = opendir(dirpath);
    if (!dir) {
        perror(dirpath);
        return -1;
    }

    while ((ent = readdir(dir)) != NULL) {
        struct stat st;
        char *path;
        size_t path_len;

        if (is_hidden_name(ent->d_name)) {
            continue;
        }

        path_len = strlen(dirpath) + 1 + strlen(ent->d_name) + 1;
        path = (char *)malloc(path_len);
        if (!path) {
            perror("malloc");
            continue;
        }
        snprintf(path, path_len, "%s/%s", dirpath, ent->d_name);

        if (stat(path, &st) != 0) {
            perror(path);
            free(path);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            traverse_dir(path, suffix, files, count, cap);
            free(path);
            continue;
        }

        if (S_ISREG(st.st_mode)) {
            if (has_suffix(ent->d_name, suffix)) {
                add_file_by_path(path, files, count, cap);
            }
        }
        free(path);
    }

    closedir(dir);
    return 0;
}

int collect_files(int argc, char **argv, FileData **files, size_t *file_count) {
    size_t count = 0;
    size_t cap = 0;
    FileData *arr = NULL;

    if (!argv || !files || !file_count || argc < 2) {
        return -1;
    }

    for (int i = 1; i < argc; i++) {
        struct stat st;
        const char *path = argv[i];

        if (is_hidden_name(path)) {
            continue;
        }

        if (stat(path, &st) != 0) {
            perror(path);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            traverse_dir(path, DEFAULT_SUFFIX, &arr, &count, &cap);
        } else if (S_ISREG(st.st_mode)) {
            add_file_by_path(path, &arr, &count, &cap);
        }
    }

    *files = arr;
    *file_count = count;
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

static char *xstrdup(const char *s) {
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
    file->path = xstrdup(path);
    if (!file->path) {
        close(fd);
        free_filedata(file);
        return NULL;
    }

    while ((nread = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < nread; i++) {
            char c = buffer[i];
            if (c == '\'') {
                continue; /* ignore apostrophes within words */
            } else if (is_word_char(c)) {
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
    node->word = xstrdup(word);
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
