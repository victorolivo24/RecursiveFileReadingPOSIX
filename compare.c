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
#include <math.h>

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

static int already_collected(FileData *files, size_t count, const char *path) {
    if (!files || !path) {
        return 0;
    }
    for (size_t i = 0; i < count; i++) {
        if (files[i].path && strcmp(files[i].path, path) == 0) {
            return 1;
        }
    }
    return 0;
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
    if (already_collected(*files, *count, path)) {
        return 0;
    }
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

void free_word_list(WordNode *head) {
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
    const WordNode *pa;
    const WordNode *pb;
    double kld_a = 0.0;
    double kld_b = 0.0;

    if (!a || !b) {
        return 0.0;
    }

    pa = a->word_list;
    pb = b->word_list;
    while (pa || pb) {
        double fa = 0.0;
        double fb = 0.0;
        int cmp;

        if (pa && pb) {
            cmp = strcmp(pa->word, pb->word);
        } else if (pa) {
            cmp = -1;
        } else {
            cmp = 1;
        }

        if (cmp == 0) {
            fa = pa->freq;
            fb = pb->freq;
            pa = pa->next;
            pb = pb->next;
        } else if (cmp < 0) {
            fa = pa->freq;
            fb = 0.0;
            pa = pa->next;
        } else {
            fa = 0.0;
            fb = pb->freq;
            pb = pb->next;
        }

        if (fa > 0.0 || fb > 0.0) {
            double f = 0.5 * (fa + fb);
            if (fa > 0.0) {
                kld_a += fa * log2(fa / f);
            }
            if (fb > 0.0) {
                kld_b += fb * log2(fb / f);
            }
        }
    }

    return sqrt(0.5 * kld_a + 0.5 * kld_b);
}

Comparison *build_comparisons(FileData *files, size_t file_count, size_t *comparison_count) {
    size_t n;
    size_t idx = 0;
    size_t total;
    Comparison *comps;

    if (!files || !comparison_count) {
        return NULL;
    }

    if (file_count < 2) {
        if (comparison_count) {
            *comparison_count = 0;
        }
        return NULL;
    }

    n = file_count;
    total = (n * (n - 1)) / 2;
    comps = (Comparison *)calloc(total, sizeof(Comparison));
    if (!comps) {
        return NULL;
    }

    for (size_t i = 0; i < n; i++) {
        for (size_t j = i + 1; j < n; j++) {
            comps[idx].file1 = &files[i];
            comps[idx].file2 = &files[j];
            comps[idx].combined_word_count = files[i].total_words + files[j].total_words;
            comps[idx].jsd = compute_jsd(&files[i], &files[j]);
            idx++;
        }
    }

    *comparison_count = total;
    return comps;
}

void print_results(const Comparison *comps, size_t count) {
    if (!comps) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        printf("%.5f %s %s\n",
               comps[i].jsd,
               comps[i].file1 ? comps[i].file1->path : "",
               comps[i].file2 ? comps[i].file2->path : "");
    }
}

void free_filedata_array(FileData *files, size_t file_count) {
    if (!files) {
        return;
    }
    for (size_t i = 0; i < file_count; i++) {
        free(files[i].path);
        free_word_list(files[i].word_list);
    }
    free(files);
}

void free_comparisons(Comparison *comps) {
    free(comps);
}

static int compare_comparisons(const void *a, const void *b) {
    const Comparison *ca = (const Comparison *)a;
    const Comparison *cb = (const Comparison *)b;

    if (ca->combined_word_count < cb->combined_word_count) {
        return 1;
    }
    if (ca->combined_word_count > cb->combined_word_count) {
        return -1;
    }

    if (ca->file1 && cb->file1 && ca->file1->path && cb->file1->path) {
        int cmp = strcmp(ca->file1->path, cb->file1->path);
        if (cmp != 0) {
            return cmp;
        }
    }
    if (ca->file2 && cb->file2 && ca->file2->path && cb->file2->path) {
        return strcmp(ca->file2->path, cb->file2->path);
    }
    return 0;
}

void sort_comparisons(Comparison *comps, size_t count) {
    if (!comps || count == 0) {
        return;
    }
    qsort(comps, count, sizeof(Comparison), compare_comparisons);
}
