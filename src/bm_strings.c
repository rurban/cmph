/* Benchmark all algos and hashes.
   With COMPILED dlopening the compiled search function */
/* Added by Reini Urban, 2025 */
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef COMPILED
#include <dlfcn.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/stat.h>
#endif

#include "config.h"
#include "cmph.h"
#include "cmph_benchmark.h"
#include "debug.h"
#include "linear_string_map.h"

// Generates a vector of random unique strings in typical URL length (40-80 chars)
static const char url_chars[] =
  "abcdefghijklmnopqrstuvwxyz"
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "0123456789"
  "-._~:/?#[]@!$&()*+,;=%";

char** random_strings_vector_new(cmph_uint32 size) {
  cmph_uint32 i;
  char** vec = (char**)malloc(sizeof(char*) * size);
  for (i = 0; i < size; ++i) {
    cmph_uint32 len = 40 + (random() % 41); // 40..80
    char* s = (char*)malloc(len + 1);
    cmph_uint32 j;
    // start with "http://" or "https://" prefix
    if (random() % 2) {
      memcpy(s, "https://", 8);
      j = 8;
    } else {
      memcpy(s, "http://", 7);
      j = 7;
    }
    for (; j < len; ++j)
      s[j] = url_chars[random() % (sizeof(url_chars) - 1)];
    s[len] = '\0';
    vec[i] = s;
  }
  // ensure uniqueness by appending index suffix to duplicates
  // (with 40-80 char random strings, collisions are astronomically unlikely,
  //  but the old code guaranteed uniqueness so we keep the contract)
  return vec;
}

void random_strings_vector_free(char** vec, cmph_uint32 size) {
  cmph_uint32 i;
  for (i = 0; i < size; ++i)
    free(vec[i]);
  free(vec);
}

char* create_lsmap_key(CMPH_ALGO algo, CMPH_HASH hash, int iters) {
  char mphf_name[128];
  assert(algo < CMPH_COUNT);
  assert(hash < CMPH_HASH_COUNT);
  snprintf(mphf_name, 128, "%s:%s:%u", cmph_names[algo], cmph_hash_names[hash], iters);
  return strdup(mphf_name);
}

#ifdef COMPILED
long file_size_kb(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return -1;      /* errno set by stat */
    return st.st_size / 1024;                 /* integer KB */
}
#endif

static cmph_uint32 g_strings_len = 0;
static char **g_strings = NULL;
static lsmap_t *g_created_mphf = NULL;
static lsmap_t *g_expected_probes = NULL;
static lsmap_t *g_mphf_probes = NULL;

int bm_create(CMPH_ALGO algo, CMPH_HASH *hashes, int iters) {
  cmph_io_adapter_t* source = NULL;
  cmph_config_t* config = NULL;
  cmph_t* mphf = NULL;
  cmph_uint32 nhashes = 0;
  CMPH_HASH hash = hashes ? hashes[0] : 0;
#ifdef COMPILED
  char c_file[24];
  char cmd[256];
  int result;
#endif
#ifdef HAVE_CRC32_HW
  const char hw[] = "HW ";
#else
  const char hw[] = "SW ";
#endif

  if (iters > (int)g_strings_len) {
    fprintf(stderr, "No input with proper size.");
    exit(-1);
  }
  if (hashes)
    for (; *hashes; ++nhashes) {
      if (hashes[nhashes] == CMPH_HASH_COUNT) break;
    }

  source = cmph_io_vector_adapter(g_strings, iters);
  config = cmph_config_new(source);
  cmph_config_set_algo(config, algo);
  if (nhashes) cmph_config_set_hashfuncs(config, hashes);
#ifdef COMPILED
  snprintf(c_file, sizeof(c_file), "bm_%s_%s.c", cmph_names[algo], cmph_hash_names[hash]);
#endif

  cmph_config_set_graphsize(config, 5);
  mphf = cmph_new(config);
  if (!mphf) {
    fprintf(stderr, "Failed to create mphf for algorithm %s with %shash %s and %u keys\n",
            cmph_names[algo], hash == CMPH_HASH_CRC32 ? hw : "",
            cmph_hash_names[hash], iters);
    cmph_config_destroy(config);
    cmph_io_vector_adapter_destroy(source);
    //g_created_mphf = NULL;
    return 1;
  }
#ifdef COMPILED
  DEBUGP("cmph_compile(mphf, config, \"%s\", \"<>\")\n", c_file);
  result = cmph_compile(mphf, config, c_file, "<>");
  if (!result) {
    fprintf(stderr, "Failed to compile mphf for algorithm %s with %shash %s\n",
            cmph_names[algo], hash == CMPH_HASH_CRC32 ? hw : "",
            cmph_hash_names[hash]);
    cmph_config_destroy(config);
    cmph_io_vector_adapter_destroy(source);
    return 1;
  }
  long c_sz = file_size_kb(c_file);
  printf("Created %s: %lu KB\n", c_file, c_sz);
#if defined(__GNUC__) && defined(__x86_64__)
  #ifdef DEBUG
    #define OPTIMS "-fstack-usage -march=native"
  #else
    #define OPTIMS "-march=native"
  #endif
#else
  #define OPTIMS ""
#endif
  snprintf(cmd, sizeof(cmd),
           "cc -shared -fPIC -DNDEBUG -O2 %s -g -I\"%s/src\" -o bm_%s_%s.so %s",
           OPTIMS, top_srcdir, cmph_names[algo],
           cmph_hash_names[hash], c_file);
  DEBUGP("%s\n", cmd);
  if (system(cmd)) {
    fprintf(stderr, "Failed to run %s\n", cmd);
    cmph_config_destroy(config);
    cmph_io_vector_adapter_destroy(source);
    return 1;
  }
#endif

  cmph_config_destroy(config);
  cmph_io_vector_adapter_destroy(source);
  lsmap_append(g_created_mphf, create_lsmap_key(algo, hash, iters), mphf);
  return 0;
}

int bm_search(CMPH_ALGO algo, CMPH_HASH *hashes, int iters) {
  int i = 0;
  char *mphf_name;
  cmph_t* mphf = NULL;
  CMPH_HASH hash = hashes ? hashes[0] : 0;

  assert(g_created_mphf);
  mphf_name = create_lsmap_key(algo, hash, iters);
  mphf = (cmph_t*)lsmap_search(g_created_mphf, mphf_name);
  free(mphf_name);

  if (!mphf)
    return 1;
#ifdef COMPILED
  char so_file[80];
  snprintf(so_file, sizeof(so_file), "./bm_%s_%s.so", cmph_names[algo],
           cmph_hash_names[hash]);
  DEBUGP("dlopen \"%s\"\n", so_file);
  void* dl = dlopen(so_file, RTLD_NOW);
  if (!dl) {
    fprintf(stderr, "Failed to dlopen %s\n", so_file);
    //unlink(so_file);
    return 1;
  }
  DEBUGP("cmph_c_search\n");
  uint32_t (*search_fn)(const char*,uint32_t) =
    (uint32_t (*)(const char*,uint32_t))dlsym(dl, "cmph_c_search");
  if (!search_fn) {
    fprintf(stderr, "Failed to find symbol cmph_c_search in %s\n", so_file);
    dlclose(dl);
    //unlink(so_file);
    return 1;
  }
#endif

  cmph_uint32* count = (cmph_uint32*)calloc(iters, sizeof(cmph_uint32));
  cmph_uint32 size = cmph_size(mphf);
  cmph_uint32* hash_count = (cmph_uint32*)calloc(size, sizeof(cmph_uint32));

  for (i = 0; i < iters * 100; ++i) {
    cmph_uint32 pos = random() % iters;
    const char* buf = g_strings[pos];
    cmph_uint32 len = (cmph_uint32)strlen(buf);
#ifdef COMPILED
    cmph_uint32 h = search_fn(buf, len);
#else
    cmph_uint32 h = cmph_search(mphf, buf, len);
#endif
    ++count[pos];
    assert(h < size && "h out of bounds");
    ++hash_count[h];
  }

  // Verify correctness later. NYI
  lsmap_append(g_expected_probes, create_lsmap_key(algo, hash, iters), count);
  lsmap_append(g_mphf_probes, create_lsmap_key(algo, hash, iters), hash_count);
#ifdef COMPILED
  dlclose(dl);
  unlink(so_file);
#endif
  return 0;
}

// not yet
void verify() { }

#define DECLARE_ALGO(algo)                                            \
  int bm_create_ ## algo(int iters) { return bm_create(algo, NULL, iters); } \
  int bm_search_ ## algo(int iters) { return bm_search(algo, NULL, iters); }

#define DECLARE_ALGO_HASH(algo, hash)                                   \
  int bm_create_ ## algo ## _ ## hash(int iters) {                      \
    CMPH_HASH hashes[3] = {CMPH_HASH_ ## hash, CMPH_HASH_COUNT, CMPH_HASH_COUNT}; \
    return bm_create(algo, hashes, iters); }                            \
  int bm_search_ ## algo ## _ ## hash(int iters) {                      \
    CMPH_HASH hashes[3] = {CMPH_HASH_ ## hash, CMPH_HASH_COUNT, CMPH_HASH_COUNT}; \
    return bm_search(algo, hashes, iters); }

DECLARE_ALGO(CMPH_BMZ);
DECLARE_ALGO(CMPH_BMZ8);
DECLARE_ALGO(CMPH_CHM);
DECLARE_ALGO(CMPH_BDZ);
DECLARE_ALGO(CMPH_BDZ_PH);
DECLARE_ALGO(CMPH_FCH);
DECLARE_ALGO(CMPH_CHD);
DECLARE_ALGO(CMPH_CHD_PH);
DECLARE_ALGO(CMPH_BRZ);

DECLARE_ALGO_HASH(CMPH_BMZ, WYHASH);
DECLARE_ALGO_HASH(CMPH_BMZ8, WYHASH);
DECLARE_ALGO_HASH(CMPH_CHM, WYHASH);
DECLARE_ALGO_HASH(CMPH_BDZ, WYHASH);
DECLARE_ALGO_HASH(CMPH_BDZ_PH, WYHASH);
DECLARE_ALGO_HASH(CMPH_FCH, WYHASH);
DECLARE_ALGO_HASH(CMPH_CHD, WYHASH);
DECLARE_ALGO_HASH(CMPH_CHD_PH, WYHASH);
DECLARE_ALGO_HASH(CMPH_BRZ, WYHASH);

DECLARE_ALGO_HASH(CMPH_BMZ, DJB2);
DECLARE_ALGO_HASH(CMPH_BMZ8, DJB2);
DECLARE_ALGO_HASH(CMPH_CHM, DJB2);
DECLARE_ALGO_HASH(CMPH_BDZ, DJB2);
DECLARE_ALGO_HASH(CMPH_BDZ_PH, DJB2);
DECLARE_ALGO_HASH(CMPH_FCH, DJB2);
DECLARE_ALGO_HASH(CMPH_CHD, DJB2);
DECLARE_ALGO_HASH(CMPH_CHD_PH, DJB2);
DECLARE_ALGO_HASH(CMPH_BRZ, DJB2);

DECLARE_ALGO_HASH(CMPH_BMZ, FNV);
DECLARE_ALGO_HASH(CMPH_BMZ8, FNV);
DECLARE_ALGO_HASH(CMPH_CHM, FNV);
DECLARE_ALGO_HASH(CMPH_BDZ, FNV);
DECLARE_ALGO_HASH(CMPH_BDZ_PH, FNV);
DECLARE_ALGO_HASH(CMPH_FCH, FNV);
DECLARE_ALGO_HASH(CMPH_CHD, FNV);
DECLARE_ALGO_HASH(CMPH_CHD_PH, FNV);
DECLARE_ALGO_HASH(CMPH_BRZ, FNV);

DECLARE_ALGO_HASH(CMPH_BMZ, SDBM);
DECLARE_ALGO_HASH(CMPH_BMZ8, SDBM);
DECLARE_ALGO_HASH(CMPH_CHM, SDBM);
DECLARE_ALGO_HASH(CMPH_BDZ, SDBM);
DECLARE_ALGO_HASH(CMPH_BDZ_PH, SDBM);
//DECLARE_ALGO_HASH(CMPH_FCH, SDBM); // too bad
DECLARE_ALGO_HASH(CMPH_CHD, SDBM);
DECLARE_ALGO_HASH(CMPH_CHD_PH, SDBM);
DECLARE_ALGO_HASH(CMPH_BRZ, SDBM);

DECLARE_ALGO_HASH(CMPH_BMZ, CRC32);
DECLARE_ALGO_HASH(CMPH_BMZ8, CRC32);
DECLARE_ALGO_HASH(CMPH_CHM, CRC32);
DECLARE_ALGO_HASH(CMPH_BDZ, CRC32);
DECLARE_ALGO_HASH(CMPH_BDZ_PH, CRC32);
DECLARE_ALGO_HASH(CMPH_FCH, CRC32);
DECLARE_ALGO_HASH(CMPH_CHD, CRC32);
DECLARE_ALGO_HASH(CMPH_CHD_PH, CRC32);
DECLARE_ALGO_HASH(CMPH_BRZ, CRC32);

int main(int argc, char **argv) {
  char algo[12] = {0};
  char hash[12] = {0};
#define SIZE 1000 * 1000
  g_strings_len = SIZE;
  cmph_uint32 iters = SIZE;
  for (int i=1; i < argc; i++) {
    if (strcmp(argv[i], "-a") == 0)  // all hashes for this algo
      strncpy(algo, argv[++i], sizeof(algo)-1);
    else if (strcmp(argv[i], "-h") == 0)  // all algos for this hash
      strncpy(hash, argv[++i], sizeof(algo)-1);
    else if (strcmp(argv[i], "-s") == 0) // size (number of keys)
      g_strings_len = (cmph_uint32)strtol(argv[++i], NULL, 10);
    else if (strcmp(argv[i], "-i") == 0) // iters
      iters = (cmph_uint32)strtol(argv[++i], NULL, 10);
    else {
      fprintf(stderr, "Usage:  bm_strings [-a algo] [-h hash] [-s size] [-i iters]\n");
      fprintf(stderr, "algos:  bmz chm bdz bdz_ph fch chd chd_ph brz\n");
      fprintf(stderr, "hashes: jenkins wyhash djb2 fnv sdbm crc32\n");
      fprintf(stderr, "size:   %u\n", SIZE);
      fprintf(stderr, "iters:  %u\n", SIZE);
      exit(1);
    }
  }

#ifdef COMPILED
  if (iters > 1000000) {
    // not relevant anymore. can be disabled
    const rlim_t kStackSize = (2 * iters) + 48;   // stack size = 16 MB
    struct rlimit rl;
    int result = getrlimit(RLIMIT_STACK, &rl);
    if (result == 0) {
      if (rl.rlim_cur < kStackSize) {
        DEBUGP("setrlimit stack-size from %lu KB to %lu KB\n", rl.rlim_cur, kStackSize);
        rl.rlim_cur = kStackSize;
        result = setrlimit(RLIMIT_STACK, &rl);
        if (result != 0)
          fprintf(stderr, "setrlimit failed with %d\n", result);
      } else {
        DEBUGP("getrlimit stack-size: %lu KB >= %lu KB\n", rl.rlim_cur, kStackSize);
      }
    }
  }
#endif

  g_strings = random_strings_vector_new(g_strings_len);
  g_created_mphf = lsmap_new();
  g_expected_probes = lsmap_new();
  g_mphf_probes = lsmap_new();

  if (!*hash || strcmp(hash, "jenkins") == 0) {
    // -a all hashes for this algo
    if (!*algo || strcmp(algo, "bmz") == 0) {
      BM_REGISTER(bm_create_CMPH_BMZ, iters);
      BM_REGISTER(bm_search_CMPH_BMZ, iters);
    }
    if (!*algo || strcmp(algo, "chm") == 0) {
      BM_REGISTER(bm_create_CMPH_CHM, iters);
      BM_REGISTER(bm_search_CMPH_CHM, iters);
    }
    if (!*algo || strcmp(algo, "bdz") == 0) {
      BM_REGISTER(bm_create_CMPH_BDZ, iters);
      BM_REGISTER(bm_search_CMPH_BDZ, iters);
    }
    if (!*algo || strcmp(algo, "fch") == 0) {
      BM_REGISTER(bm_create_CMPH_FCH, iters);
      BM_REGISTER(bm_search_CMPH_FCH, iters);
    }
    if (!*algo || strcmp(algo, "bdz_ph") == 0) {
      BM_REGISTER(bm_create_CMPH_BDZ_PH, iters);
      BM_REGISTER(bm_search_CMPH_BDZ_PH, iters);
    }
    if (!*algo || strcmp(algo, "chd_ph") == 0) {
      BM_REGISTER(bm_create_CMPH_CHD_PH, iters);
      BM_REGISTER(bm_search_CMPH_CHD_PH, iters);
    }
    if (!*algo || strcmp(algo, "chd") == 0) {
      BM_REGISTER(bm_create_CMPH_CHD, iters);
      BM_REGISTER(bm_search_CMPH_CHD, iters);
    }
#ifndef COMPILED
    if (!*algo || strcmp(algo, "brz") == 0) {
      // instable:
      BM_REGISTER(bm_create_CMPH_BRZ, iters);
      BM_REGISTER(bm_search_CMPH_BRZ, iters);
    }
#endif
    if (!*algo || strcmp(algo, "bmz8") == 0) {
      //BM_REGISTER(bm_create_CMPH_BMZ8, 255); // only really needed for BRZ subgraphs
      //BM_REGISTER(bm_search_CMPH_BMZ8, 255);
    }
  }

  if (!*hash || strcmp(hash, "wyhash") == 0) {
    if (!*algo || strcmp(algo, "bmz") == 0) {
      BM_REGISTER(bm_create_CMPH_BMZ_WYHASH, iters);
      BM_REGISTER(bm_search_CMPH_BMZ_WYHASH, iters);
    }
    if (!*algo || strcmp(algo, "chm") == 0) {
      BM_REGISTER(bm_create_CMPH_CHM_WYHASH, iters);
      BM_REGISTER(bm_search_CMPH_CHM_WYHASH, iters);
    }
    if (!*algo || strcmp(algo, "bdz") == 0) {
      BM_REGISTER(bm_create_CMPH_BDZ_WYHASH, iters);
      BM_REGISTER(bm_search_CMPH_BDZ_WYHASH, iters);
    }
    if (!*algo || strcmp(algo, "fch") == 0) {
      BM_REGISTER(bm_create_CMPH_FCH_WYHASH, iters);
      BM_REGISTER(bm_search_CMPH_FCH_WYHASH, iters);
    }
    if (!*algo || strcmp(algo, "bdz_ph") == 0) {
      BM_REGISTER(bm_create_CMPH_BDZ_PH_WYHASH, iters);
      BM_REGISTER(bm_search_CMPH_BDZ_PH_WYHASH, iters);
    }
    if (!*algo || strcmp(algo, "chd_ph") == 0) {
      BM_REGISTER(bm_create_CMPH_CHD_PH_WYHASH, iters);
      BM_REGISTER(bm_search_CMPH_CHD_PH_WYHASH, iters);
    }
    if (!*algo || strcmp(algo, "chd") == 0) {
      BM_REGISTER(bm_create_CMPH_CHD_WYHASH, iters);
      BM_REGISTER(bm_search_CMPH_CHD_WYHASH, iters);
    }
#ifndef COMPILED
    if (!*algo || strcmp(algo, "brz") == 0) {
      // instable:
      BM_REGISTER(bm_create_CMPH_BRZ_WYHASH, iters);
      BM_REGISTER(bm_search_CMPH_BRZ_WYHASH, iters);
    }
#endif
  }

  if (!*hash || strcmp(hash, "djb2") == 0) {
    if (!*algo || strcmp(algo, "bmz") == 0) {
      BM_REGISTER(bm_create_CMPH_BMZ_DJB2, iters);
      BM_REGISTER(bm_search_CMPH_BMZ_DJB2, iters);
    }
    if (!*algo || strcmp(algo, "chm") == 0) {
      BM_REGISTER(bm_create_CMPH_CHM_DJB2, iters);
      BM_REGISTER(bm_search_CMPH_CHM_DJB2, iters);
    }
    if (!*algo || strcmp(algo, "bdz") == 0) {
      BM_REGISTER(bm_create_CMPH_BDZ_DJB2, iters);
      BM_REGISTER(bm_search_CMPH_BDZ_DJB2, iters);
    }
    if (strcmp(algo, "fch") == 0) {
      // unstable
      BM_REGISTER(bm_create_CMPH_FCH_DJB2, iters);
      BM_REGISTER(bm_search_CMPH_FCH_DJB2, iters);
    }
    if (strcmp(algo, "bdz_ph") == 0) {
      // unstable
      BM_REGISTER(bm_create_CMPH_BDZ_PH_DJB2, iters);
      BM_REGISTER(bm_search_CMPH_BDZ_PH_DJB2, iters);
    }
    if (strcmp(algo, "chd_ph") == 0) {
      // unstable
      BM_REGISTER(bm_create_CMPH_CHD_PH_DJB2, iters);
      BM_REGISTER(bm_search_CMPH_CHD_PH_DJB2, iters);
    }
    if (strcmp(algo, "chd") == 0) {
      // unstable
      BM_REGISTER(bm_create_CMPH_CHD_DJB2, iters);
      BM_REGISTER(bm_search_CMPH_CHD_DJB2, iters);
    }
#ifndef COMPILED
    if (!*algo || strcmp(algo, "brz") == 0) {
      // instable:
      BM_REGISTER(bm_create_CMPH_BRZ_DJB2, iters);
      BM_REGISTER(bm_search_CMPH_BRZ_DJB2, iters);
    }
#endif
  }

  if (!*hash || strcmp(hash, "fnv") == 0) {
    if (!*algo || strcmp(algo, "bmz") == 0) {
      BM_REGISTER(bm_create_CMPH_BMZ_FNV, iters);
      BM_REGISTER(bm_search_CMPH_BMZ_FNV, iters);
    }
    if (!*algo || strcmp(algo, "chm") == 0) {
      BM_REGISTER(bm_create_CMPH_CHM_FNV, iters);
      BM_REGISTER(bm_search_CMPH_CHM_FNV, iters);
    }
    if (!*algo || strcmp(algo, "bdz") == 0) {
      BM_REGISTER(bm_create_CMPH_BDZ_FNV, iters);
      BM_REGISTER(bm_search_CMPH_BDZ_FNV, iters);
    }
    if (!*algo || strcmp(algo, "fch") == 0) {
      BM_REGISTER(bm_create_CMPH_FCH_FNV, iters);
      BM_REGISTER(bm_search_CMPH_FCH_FNV, iters);
    }
    if (!*algo || strcmp(algo, "bdz_ph") == 0) {
      BM_REGISTER(bm_create_CMPH_BDZ_PH_FNV, iters);
      BM_REGISTER(bm_search_CMPH_BDZ_PH_FNV, iters);
    }
    if (!*algo || strcmp(algo, "chd_ph") == 0) {
      BM_REGISTER(bm_create_CMPH_CHD_PH_FNV, iters);
      BM_REGISTER(bm_search_CMPH_CHD_PH_FNV, iters);
    }
    if (!*algo || strcmp(algo, "chd") == 0) {
      BM_REGISTER(bm_create_CMPH_CHD_FNV, iters);
      BM_REGISTER(bm_search_CMPH_CHD_FNV, iters);
    }
#ifndef COMPILED
    if (!*algo || strcmp(algo, "brz") == 0) {
      // instable:
      BM_REGISTER(bm_create_CMPH_BRZ_FNV, iters);
      BM_REGISTER(bm_search_CMPH_BRZ_FNV, iters);
    }
#endif
  }

  if (!*hash || strcmp(hash, "sdbm") == 0) {
    if (!*algo || strcmp(algo, "bmz") == 0) {
      BM_REGISTER(bm_create_CMPH_BMZ_SDBM, iters);
      BM_REGISTER(bm_search_CMPH_BMZ_SDBM, iters);
    }
    if (!*algo || strcmp(algo, "chm") == 0) {
      BM_REGISTER(bm_create_CMPH_CHM_SDBM, iters);
      BM_REGISTER(bm_search_CMPH_CHM_SDBM, iters);
    }
    if (!*algo || strcmp(algo, "bdz") == 0) {
      BM_REGISTER(bm_create_CMPH_BDZ_SDBM, iters);
      BM_REGISTER(bm_search_CMPH_BDZ_SDBM, iters);
    }
    if (strcmp(algo, "fch") == 0) {
      // too bad
      //BM_REGISTER(bm_create_CMPH_FCH_SDBM, iters);
      //BM_REGISTER(bm_search_CMPH_FCH_SDBM, iters);
    }
    if (strcmp(algo, "bdz_ph") == 0) {
      // unstable
      BM_REGISTER(bm_create_CMPH_BDZ_PH_SDBM, iters);
      BM_REGISTER(bm_search_CMPH_BDZ_PH_SDBM, iters);
    }
    if (strcmp(algo, "chd_ph") == 0) {
      // unstable
      BM_REGISTER(bm_create_CMPH_CHD_PH_SDBM, iters);
      BM_REGISTER(bm_search_CMPH_CHD_PH_SDBM, iters);
    }
    if (strcmp(algo, "chd") == 0) {
      // unstable
      BM_REGISTER(bm_create_CMPH_CHD_SDBM, iters);
      BM_REGISTER(bm_search_CMPH_CHD_SDBM, iters);
    }
#ifndef COMPILED
    if (!*algo || strcmp(algo, "brz") == 0) {
      // instable:
      BM_REGISTER(bm_create_CMPH_BRZ_SDBM, iters);
      BM_REGISTER(bm_search_CMPH_BRZ_SDBM, iters);
    }
#endif
  }

  if (!*hash || strcmp(hash, "crc32") == 0) {
    if (!*algo || strcmp(algo, "bmz") == 0) {
      BM_REGISTER(bm_create_CMPH_BMZ_CRC32, iters);
      BM_REGISTER(bm_search_CMPH_BMZ_CRC32, iters);
    }
    if (!*algo || strcmp(algo, "chm") == 0) {
      BM_REGISTER(bm_create_CMPH_CHM_CRC32, iters);
      BM_REGISTER(bm_search_CMPH_CHM_CRC32, iters);
    }
    if (!*algo || strcmp(algo, "bdz") == 0) {
      BM_REGISTER(bm_create_CMPH_BDZ_CRC32, iters);
      BM_REGISTER(bm_search_CMPH_BDZ_CRC32, iters);
    }
    if (!*algo || strcmp(algo, "fch") == 0) {
      BM_REGISTER(bm_create_CMPH_FCH_CRC32, iters);
      BM_REGISTER(bm_search_CMPH_FCH_CRC32, iters);
    }
    if (!*algo || strcmp(algo, "bdz_ph") == 0) {
      BM_REGISTER(bm_create_CMPH_BDZ_PH_CRC32, iters);
      BM_REGISTER(bm_search_CMPH_BDZ_PH_CRC32, iters);
    }
    if (!*algo || strcmp(algo, "chd_ph") == 0) {
      BM_REGISTER(bm_create_CMPH_CHD_PH_CRC32, iters);
      BM_REGISTER(bm_search_CMPH_CHD_PH_CRC32, iters);
    }
    if (!*algo || strcmp(algo, "chd") == 0) {
      BM_REGISTER(bm_create_CMPH_CHD_CRC32, iters);
      BM_REGISTER(bm_search_CMPH_CHD_CRC32, iters);
    }
#ifndef COMPILED
    if (!*algo || strcmp(algo, "brz") == 0) {
      // instable:
      BM_REGISTER(bm_create_CMPH_BRZ_CRC32, iters);
      BM_REGISTER(bm_search_CMPH_BRZ_CRC32, iters);
    }
#endif
  }

  run_benchmarks();

  verify();
  random_strings_vector_free(g_strings, g_strings_len);
  lsmap_foreach_key(g_created_mphf, (void(*)(const char*))free);
  lsmap_foreach_value(g_created_mphf, (void(*)(void*))cmph_destroy);
  lsmap_destroy(g_created_mphf);
  return 0;
}
