#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
uint32_t cmph_search(const char* key, uint32_t keylen);

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: ./cmph_compile_run $a words\n");
        exit(1);
    }
    FILE *f = fopen(argv[2], "r");
    if (!f) {
        perror("fopen");
        exit(1);
    }
    printf("compile %s for %s\n", argv[1], argv[2]);
    char key[128];
    unsigned l = 0;
    unsigned failed = 0;
    uint32_t i;
    while (fgets(key, sizeof(key), f)) {
        // delete the ending \n
        size_t len = strlen(key);
        key[len-1] = '\0';
        i = cmph_search(key, len-1);
        if (i != l) {
            if (failed < 10)
                printf("%s => %u: FAIL should be at index %u\n", key, i, l);
            failed++;
        }
        l++;
    }
    fclose(f);
    if (failed)
        exit(1);
}
