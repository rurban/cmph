#include "compile.h"

void bytes_compile(FILE *out, const char *name, const uint8_t *buf, unsigned len) {
  fprintf(out, "#define %s_size %u\n", name, len);
  fprintf(out, "const uint8_t %s[%u] = {\n    ", name, len);
  if (len > 0) {
    for (unsigned i = 0; i < len - 1; i++) {
      if (i % 16 == 15)
        fprintf(out, "%u,\n    ", buf[i]);
      else
        fprintf(out, "%u, ", buf[i]);
    }
    fprintf(out, "%u", buf[len - 1]);
  }
  fprintf(out, "\n};\n");
}

void uint32_compile(FILE *out, const char *name, const uint32_t *buf, unsigned len) {
  fprintf(out, "#define %s_size %u\n", name, len);
  fprintf(out, "const uint32_t %s[%u] = {\n    ", name, len);
  if (len > 0) {
    for (unsigned i = 0; i < len - 1; i++) {
      if (i % 16 == 15)
        fprintf(out, "%u,\n    ", buf[i]);
      else
        fprintf(out, "%u, ", buf[i]);
    }
    fprintf(out, "%u", buf[len - 1]);
  }
  fprintf(out, "\n};\n");
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
