#include <stdint.h>
#include <stdio.h>

void bytes_compile(FILE *out, const char *name, const uint8_t *buf, unsigned len);
void bytes_2_compile(FILE *out, const char *name, const uint8_t *buf,
                  unsigned len1, unsigned len2);
void uint32_compile(FILE *out, const char *name, const uint32_t *buf, unsigned len);
