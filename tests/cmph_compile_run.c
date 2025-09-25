#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

uint32_t cmph_c_search(const char* key, uint32_t keylen);
uint32_t cmph_c_size(void);
uint32_t cmph_c_order(uint32_t index);

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: ./cmph_compile_run algo_base words [have_ordering_table]\n");
        exit(1);
    }
    FILE *f = fopen(argv[2], "r");
    if (!f) {
        perror("fopen");
        fprintf(stderr, "Usage: ./cmph_compile_run algo_base words [have_ordering_table]\n");
        exit(1);
    }
    // some funcs over-allocate. we need an exported cmph_c_size()
    uint32_t size = cmph_c_size();
    char key[128];
    unsigned l = 0;
    unsigned failed = 0;
    uint32_t h;
    uint32_t keys_per_bin = 1;
    uint32_t *hasharray = (uint32_t*)calloc(size, 4);
    uint8_t *hashtable = (uint8_t*)calloc(size, 1);
    char *algo = strdup(argv[1]);
    char *p;
    char is_order_preserving;
#ifdef ORDERING
    char has_ordering_table;
#endif

    if ((p = strchr(algo, '_')))
        *p = '\0';
    is_order_preserving = strcmp(algo, "chm") == 0;
#ifdef ORDERING
    has_ordering_table = argc == 4 && !is_order_preserving;
#endif

    while (fgets(key, sizeof(key), f)) {
        // delete the ending \n
        int len = (int)strlen(key);
        key[len-1] = '\0';

        h = cmph_c_search(key, len-1);

        if (h >= size) {
            fprintf(stderr, "Unknown key %*s, h %u too large\n", len-1, key, h);
            failed++;
        //} else if (hasharray[h]) {
        //    fprintf(stderr, "Duplicated or unknown key %*s in the input\n", len-1, key);
        //    failed++;
        } else if (hashtable[h] >= keys_per_bin) {
            fprintf(stderr, "More than %u keys were mapped to bin %u\n", keys_per_bin, h);
            fprintf(stderr, "Duplicated or unknown key %*s in the input\n", len-1, key);
            failed++;
        }
        else {
            hasharray[h] = 1;
            hashtable[h]++;
        }
        if (is_order_preserving && h != l) {
            if (failed < 10)
                printf("%*s => %u: FAIL should be at index %u\n", len-1, key, h, l);
            failed++;
        }
#ifdef ORDERING
        else if (has_ordering_table) {
            uint32_t o = cmph_c_order(h);
            if (o != l) {
                if (failed < 10)
                    printf("%*s => %u: FAIL should be at index %u\n", len-1, key, o, l);
                failed++;
            }
        }
#endif
        l++;
        if (l > size) {
            fprintf(stderr, "Too many keys in the input, cmph_c_size() was %u\n", size);
            failed++;
            break;
        }
        if (failed > 5)
            break;
    }
    free(hasharray);
    free(hashtable);
    free(algo);
    fclose(f);
    if (failed)
        return 1;
}
