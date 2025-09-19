#include "compile.h"

void bytes_compile(FILE *out, const char *name, const uint8_t *buf, unsigned len) {
  fprintf(out, "const uint8_t %s[%u] = {\n    ", name, len);
  for (unsigned i = 0; i < len - 1; i++) {
    fprintf(out, "%u, ", buf[i]);
    if (i % 16 == 15)
      fprintf(out, "\n    ");
  }
  fprintf(out, "%u\n};\n", buf[len - 1]);
}

void uint32_compile(FILE *out, const char *name, const uint32_t *buf, unsigned len) {
  fprintf(out, "const uint32_t %s[%u] = {\n    ", name, len);
  for (unsigned i = 0; i < len - 1; i++) {
    fprintf(out, "%uU, ", buf[i]);
    if (i % 16 == 15)
      fprintf(out, "\n    ");
  }
  fprintf(out, "%uU\n};\n", buf[len - 1]);
}

void bytes_2_compile(FILE *out, const char *name, const uint8_t *buf,
                  unsigned len1, unsigned len2) {
  fprintf(out, "const uint8_t %s[%u][%u] = {\n    ", name, len1, len2);
  for (unsigned i = 0; i < len1; i++) {
    fprintf(out, "{");
    for (unsigned j = 0; j < len2 - 1; j++) {
      fprintf(out, "%3u, ", *buf++);
    }
    if (i == len1 - 1)
      fprintf(out, "%3u}\t/* %u */", *buf++, i);
    else
      fprintf(out, "%3u},\t/* %u */\n    ", *buf++, i);
  }
  fprintf(out, "\n};\n");
}
