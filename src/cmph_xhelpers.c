#include "cmph_xhelpers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* stdlib helpers */
void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p && n != 0) {          /* 0 is silently allowed */
        perror("malloc");
        fprintf(stderr, "fatal: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

void *xcalloc(size_t nmemb, size_t size) {
    void *p = calloc(nmemb, size);
    if (!p && nmemb != 0 && size != 0) {
        perror("calloc");
        fprintf(stderr, "fatal: out of memory\n");
        abort();
    }
    return p;
}

void *xrealloc(void *p, size_t size) {
    void *newp = realloc(p, size);
    if (!newp && size != 0) {
        perror("realloc");
        fprintf(stderr, "fatal: out of memory\n");
        abort();
    }
    return newp;
}

void *xstrdup(char *s) {
    void *p = strdup(s);
    if (!p) {
        perror("strdup");
        fprintf(stderr, "fatal: out of memory\n");
        abort();
    }
    return p;
}
