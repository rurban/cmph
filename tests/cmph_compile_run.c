#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

//uint32_t cmph_size(void);
uint32_t cmph_search(const char* key, uint32_t keylen);

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: ./cmph_compile_run algo words nkeys\n");
        exit(1);
    }
    FILE *f = fopen(argv[2], "r");
    if (!f) {
        perror("fopen");
        exit(1);
    }
    uint32_t nkeys = strtol(argv[3], NULL, 10);
    printf("compile %s for %s [%u]\n", argv[1], argv[2], nkeys);
    int is_order_preserving = strcmp(argv[1], "chm") == 0;
    char key[128];
    unsigned l = 0;
    unsigned failed = 0;
    uint32_t h;
    // FIXME: some funcs over-allocate. we need an exported cmph_size()
    uint32_t *hasharray = (uint32_t*)calloc(nkeys, 4);
    while (fgets(key, sizeof(key), f)) {
        // delete the ending \n
        int len = (int)strlen(key);
        key[len-1] = '\0';
        h = cmph_search(key, len-1);
        if (h >= nkeys) {
            fprintf(stderr, "Unknown key %*s, h %u too large\n", len-1, key, h);
            failed++;
        } else if (hasharray[h]) {
            fprintf(stderr, "Duplicated or unknown key %*s in the input\n", len-1, key);
            failed++;
        } else
            hasharray[h] = 1;
        if (is_order_preserving && h != l) {
            if (failed < 10)
                printf("%*s => %u: FAIL should be at index %u\n", len-1, key, h, l);
            failed++;
        }
        l++;
    }
    fclose(f);
    if (failed)
        exit(1);
}
