/* benchmark all algos for creation and run-time search time costs */
/* with all hash variants also */
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "bitbool.h"
#include "cmph.h"
#include "cmph_benchmark.h"
#include "linear_string_map.h"

// Generates a vector with random unique 32 bits integers
cmph_uint32* random_numbers_vector_new(cmph_uint32 size) {
  cmph_uint32 i = 0;
  cmph_uint32 dup_bits = sizeof(cmph_uint32)*size*8;
  char* dup = (char*)malloc(dup_bits/8);
  cmph_uint32* vec = (cmph_uint32 *)malloc(sizeof(cmph_uint32)*size);
  memset(dup, 0, dup_bits/8);
  for (i = 0; i < size; ++i) {
    cmph_uint32 v = random();
    while (GETBIT(dup, v % dup_bits)) { v = random(); }
    SETBIT(dup, v % dup_bits);
    vec[i] = v;
  }
  free(dup);
  return vec;
}

int cmph_uint32_cmp(const void *a, const void *b) {
  return *(const cmph_uint32*)a - *(const cmph_uint32*)b;
}

char* create_lsmap_key(CMPH_ALGO algo, CMPH_HASH hash, int iters) {
  char mphf_name[128];
  assert(algo < CMPH_COUNT);
  assert(hash < CMPH_HASH_COUNT);
  snprintf(mphf_name, 128, "%s:%s:%u", cmph_names[algo], cmph_hash_names[hash], iters);
  return strdup(mphf_name);
}

static cmph_uint32 g_numbers_len = 0;
static cmph_uint32 *g_numbers = NULL;
static lsmap_t *g_created_mphf = NULL;
static lsmap_t *g_expected_probes = NULL;
static lsmap_t *g_mphf_probes = NULL;

int bm_create(CMPH_ALGO algo, CMPH_HASH *hashes, int iters) {
  cmph_io_adapter_t* source = NULL;
  cmph_config_t* config = NULL;
  cmph_t* mphf = NULL;
  cmph_uint32 nhashes = 0;
  CMPH_HASH hash = hashes ? hashes[0] : 0;

  if (iters > (int)g_numbers_len) {
    fprintf(stderr, "No input with proper size.");
    exit(-1);
  }
  if (hashes)
    for (; *hashes; ++nhashes) {
      if (hashes[nhashes] == CMPH_HASH_COUNT) break;
    }

  source = cmph_io_struct_vector_adapter(
      (void*)g_numbers, sizeof(cmph_uint32),
      0, sizeof(cmph_uint32), iters);
  config = cmph_config_new(source);
  cmph_config_set_algo(config, algo);
  if (nhashes) cmph_config_set_hashfuncs(config, hashes);

  cmph_config_set_graphsize(config, 5);
  mphf = cmph_new(config);
  if (!mphf) {
#ifdef HAVE_CRC32_HW
    const char hw[] = "HW ";
#else
    const char hw[] = "SW ";
#endif
    fprintf(stderr, "Failed to create mphf for algorithm %s with %shash %s and %u keys\n",
            cmph_names[algo], hash == CMPH_HASH_CRC32 ? hw : "",
            cmph_hash_names[hash], iters);
    cmph_config_destroy(config);
    cmph_io_struct_vector_adapter_destroy(source);
    //g_created_mphf = NULL;
    return 1;
  }
  cmph_config_destroy(config);
  cmph_io_struct_vector_adapter_destroy(source);
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

  if (!mphf) {
//#ifdef HAVE_CRC32_HW
//    const char hw[] = "HW ";
//#else
//    const char hw[] = "SW ";
//#endif
//    fprintf(stderr, "No mphf found for algorithm %s with %shash %s and %u keys\n",
//            cmph_names[algo], hash == CMPH_HASH_CRC32 ? hw : "",
//            cmph_hash_names[hash], iters);
    return 1;
  }

  cmph_uint32* count = (cmph_uint32*)calloc(iters, sizeof(cmph_uint32));
  cmph_uint32 size = cmph_size(mphf);
  cmph_uint32* hash_count = (cmph_uint32*)calloc(size, sizeof(cmph_uint32));

  for (i = 0; i < iters * 100; ++i) {
    cmph_uint32 pos = random() % iters;
    const char* buf = (const char*)(g_numbers + pos);
    cmph_uint32 h = cmph_search(mphf, buf, sizeof(cmph_uint32));
    ++count[pos];
    assert(h < size && "h out of bounds");
    ++hash_count[h];
  }

  // Verify correctness later.
  lsmap_append(g_expected_probes, create_lsmap_key(algo, hash, iters), count);
  lsmap_append(g_mphf_probes, create_lsmap_key(algo, hash, iters), hash_count);
  return 0;
}

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
  g_numbers_len = SIZE;
  cmph_uint32 iters = SIZE;
  for (int i=1; i < argc; i++) {
    if (strcmp(argv[i], "-a") == 0)  // all hashes for this algo
      strncpy(algo, argv[++i], sizeof(algo)-1);
    else if (strcmp(argv[i], "-h") == 0)  // all algos for this hash
      strncpy(hash, argv[++i], sizeof(algo)-1);
    else if (strcmp(argv[i], "-s") == 0) // size
      g_numbers_len = (cmph_uint32)strtol(argv[++i], NULL, 10);
    else if (strcmp(argv[i], "-i") == 0) // iters
      iters = (cmph_uint32)strtol(argv[++i], NULL, 10);
    else {
      fprintf(stderr, "Usage:  bm_numbers [-a algo] [-h hash] [-s size] [-i iters]\n");
      fprintf(stderr, "algos:  bmz chm bdz bdz_ph fch chd chd_ph brz\n");
      fprintf(stderr, "hashes: jenkins wyhash djb2 fnv sdbm crc32\n");
      fprintf(stderr, "size:   %u\n", SIZE);
      fprintf(stderr, "iters:  %u\n", SIZE);
      exit(1);
    }
  }
  g_numbers = random_numbers_vector_new(g_numbers_len);
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
    if (!*algo || strcmp(algo, "chd") == 0) {
      BM_REGISTER(bm_create_CMPH_CHD, iters);
      BM_REGISTER(bm_search_CMPH_CHD, iters);
    }
    if (!*algo || strcmp(algo, "chd_ph") == 0) {
      BM_REGISTER(bm_create_CMPH_CHD_PH, iters);
      BM_REGISTER(bm_search_CMPH_CHD_PH, iters);
    }
    if (!*algo || strcmp(algo, "brz") == 0) {
      // instable:
      BM_REGISTER(bm_create_CMPH_BRZ, iters);
      BM_REGISTER(bm_search_CMPH_BRZ, iters);
    }
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
    if (!*algo || strcmp(algo, "chd") == 0) {
      BM_REGISTER(bm_create_CMPH_CHD_WYHASH, iters);
      BM_REGISTER(bm_search_CMPH_CHD_WYHASH, iters);
    }
    if (!*algo || strcmp(algo, "chd_ph") == 0) {
      BM_REGISTER(bm_create_CMPH_CHD_PH_WYHASH, iters);
      BM_REGISTER(bm_search_CMPH_CHD_PH_WYHASH, iters);
    }
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
    if (strcmp(algo, "chd") == 0) {
      // unstable
      BM_REGISTER(bm_create_CMPH_CHD_DJB2, iters);
      BM_REGISTER(bm_search_CMPH_CHD_DJB2, iters);
    }
    if (strcmp(algo, "chd_ph") == 0) {
      // unstable
      BM_REGISTER(bm_create_CMPH_CHD_PH_DJB2, iters);
      BM_REGISTER(bm_search_CMPH_CHD_PH_DJB2, iters);
    }
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
    if (!*algo || strcmp(algo, "chd") == 0) {
      BM_REGISTER(bm_create_CMPH_CHD_FNV, iters);
      BM_REGISTER(bm_search_CMPH_CHD_FNV, iters);
    }
    if (!*algo || strcmp(algo, "chd_ph") == 0) {
      BM_REGISTER(bm_create_CMPH_CHD_PH_FNV, iters);
      BM_REGISTER(bm_search_CMPH_CHD_PH_FNV, iters);
    }
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
    if (strcmp(algo, "chd") == 0) {
      // unstable
      BM_REGISTER(bm_create_CMPH_CHD_SDBM, iters);
      BM_REGISTER(bm_search_CMPH_CHD_SDBM, iters);
    }
    if (strcmp(algo, "chd_ph") == 0) {
      // unstable
      BM_REGISTER(bm_create_CMPH_CHD_PH_SDBM, iters);
      BM_REGISTER(bm_search_CMPH_CHD_PH_SDBM, iters);
    }
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
    if (!*algo || strcmp(algo, "chd") == 0) {
      BM_REGISTER(bm_create_CMPH_CHD_CRC32, iters);
      BM_REGISTER(bm_search_CMPH_CHD_CRC32, iters);
    }
    if (!*algo || strcmp(algo, "chd_ph") == 0) {
      BM_REGISTER(bm_create_CMPH_CHD_PH_CRC32, iters);
      BM_REGISTER(bm_search_CMPH_CHD_PH_CRC32, iters);
    }
  }

  run_benchmarks();

  verify();
  free(g_numbers);
  lsmap_foreach_key(g_created_mphf, (void(*)(const char*))free);
  lsmap_foreach_value(g_created_mphf, (void(*)(void*))cmph_destroy);
  lsmap_destroy(g_created_mphf);
  return 0;
}
