#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bitbool.h"
#include "chd.h"
#include "chd_structs.h"
#include "cmph_structs.h"
#include "hash.h"
#include "compile.h"
// #define DEBUG
#include "debug.h"

chd_config_data_t *chd_config_new(cmph_config_t *mph) {
  cmph_io_adapter_t *key_source = mph->key_source;
  chd_config_data_t *chd;
  chd = (chd_config_data_t *)malloc(sizeof(chd_config_data_t));
  if (!chd)
    return NULL;
  memset(chd, 0, sizeof(chd_config_data_t));

  chd->chd_ph = cmph_config_new(key_source);
  cmph_config_set_algo(chd->chd_ph, CMPH_CHD_PH);

  return chd;
}

void chd_config_destroy(cmph_config_t *mph) {
  chd_config_data_t *data = (chd_config_data_t *)mph->data;
  DEBUGP("Destroying algorithm dependent data\n");
  if (data->chd_ph) {
    cmph_config_destroy(data->chd_ph);
    data->chd_ph = NULL;
  }
  free(data);
}

void chd_config_set_hashfuncs(cmph_config_t *mph, CMPH_HASH *hashfuncs) {
  chd_config_data_t *data = (chd_config_data_t *)mph->data;
  for (unsigned i = 0; i < mph->nhashfuncs; i++) {
    mph->hashfuncs[i] = hashfuncs[i];
  }
  for (unsigned i = mph->nhashfuncs; i < 3; i++) {
    mph->hashfuncs[i] = CMPH_HASH_COUNT;
  }
  cmph_config_set_hashfuncs(data->chd_ph, hashfuncs);
}

void chd_config_set_b(cmph_config_t *mph, cmph_uint32 keys_per_bucket) {
  chd_config_data_t *data = (chd_config_data_t *)mph->data;
  cmph_config_set_b(data->chd_ph, keys_per_bucket);
}

void chd_config_set_keys_per_bin(cmph_config_t *mph, cmph_uint32 keys_per_bin) {
  chd_config_data_t *data = (chd_config_data_t *)mph->data;
  cmph_config_set_keys_per_bin(data->chd_ph, keys_per_bin);
}

cmph_t *chd_new(cmph_config_t *mph, double c) {
  DEBUGP("Creating new chd");
  cmph_t *mphf = NULL;
  chd_data_t *chdf = NULL;
  chd_config_data_t *chd = (chd_config_data_t *)mph->data;
  chd_ph_config_data_t *chd_ph = (chd_ph_config_data_t *)chd->chd_ph->data;
  compressed_rank_t cr;

  cmph_t *chd_phf = NULL;
  cmph_uint32 packed_chd_phf_size = 0;
  cmph_uint8 *packed_chd_phf = NULL;

  cmph_uint32 packed_cr_size = 0;
  cmph_uint8 *packed_cr = NULL;

  cmph_uint32 i, idx, nkeys, nvals, nbins;
  cmph_uint32 *vals_table = NULL;
  cmph_uint32 *occup_table = NULL;
#ifdef CMPH_TIMING
  double construction_time_begin = 0.0;
  double construction_time = 0.0;
  ELAPSED_TIME_IN_SECONDS(&construction_time_begin);
#endif

  cmph_config_set_verbosity(chd->chd_ph, mph->verbosity);
  cmph_config_set_graphsize(chd->chd_ph, c);

  if (mph->verbosity)
    fprintf(stderr,
            "Generating a CHD_PH perfect hash function with a load factor "
            "equal to %.3f\n",
            c);

  chd_phf = cmph_new(chd->chd_ph);

  if (chd_phf == NULL)
    return NULL;

  packed_chd_phf_size = cmph_packed_size(chd_phf);
  DEBUGP("packed_chd_phf_size = %u\n", packed_chd_phf_size);

  /* Make sure that we have enough space to pack the mphf. */
  packed_chd_phf = (cmph_uint8 *)calloc((size_t)packed_chd_phf_size, (size_t)1);

  /* Pack the chd_ph */
  cmph_pack(chd_phf, packed_chd_phf);
  cmph_destroy(chd_phf);

  if (mph->verbosity)
    fprintf(stderr, "Compressing the range of the resulting CHD_PH perfect "
                    "hash function\n");

  compressed_rank_init(&cr);
  nbins = chd_ph->n;
  nkeys = chd_ph->m;
  nvals = nbins - nkeys;

  vals_table = (cmph_uint32 *)calloc(nvals, sizeof(cmph_uint32));
  occup_table = (cmph_uint32 *)chd_ph->occup_table;

  for (i = 0, idx = 0; i < nbins; i++) {
    if (!GETBIT32(occup_table, i)) {
      vals_table[idx++] = i;
    }
  }

  compressed_rank_generate(&cr, vals_table, nvals);
  free(vals_table);

  packed_cr_size = compressed_rank_packed_size(&cr);
  packed_cr = (cmph_uint8 *)calloc(packed_cr_size, sizeof(cmph_uint8));
  compressed_rank_pack(&cr, packed_cr);
  compressed_rank_destroy(&cr);

  mphf = (cmph_t *)malloc(sizeof(cmph_t));
  mphf->algo = mph->algo;
  if (mph->do_ordering_table) {
    if (mph->verbosity)
      fprintf(stderr, "Create ordering table\n");
    mphf->o = (cmph_uint32 *)malloc(nkeys * sizeof(cmph_uint32));
    memset(mphf->o, 0xFF, nkeys * sizeof(cmph_uint32));
    mph->key_source->rewind(mph->key_source->data);
    for (i = 0; i < nkeys; ++i) // all edges
    {
      cmph_uint32 keylen, bin_idx, rank;
      char *key = NULL;
      mph->key_source->read(mph->key_source->data, &key, &keylen);
      bin_idx = cmph_search_packed(packed_chd_phf, key, keylen);
      rank = compressed_rank_query_packed(packed_cr, bin_idx);
      mphf->o[bin_idx - rank] = i;
      mph->key_source->dispose(key);
    }
  }
  else
    mphf->o = NULL;

  chdf = (chd_data_t *)malloc(sizeof(chd_data_t));

  chdf->packed_cr = packed_cr;
  packed_cr = NULL; // transfer memory ownership

  chdf->packed_chd_phf = packed_chd_phf;
  packed_chd_phf = NULL; // transfer memory ownership

  chdf->packed_chd_phf_size = packed_chd_phf_size;
  chdf->packed_cr_size = packed_cr_size;

  mphf->data = chdf;
  mphf->size = nkeys;

  DEBUGP("Successfully generated minimal perfect hash\n");
  if (mph->verbosity)
	  fprintf(stderr, "Successfully generated minimal perfect hash function\n");
#ifdef CMPH_TIMING
  ELAPSED_TIME_IN_SECONDS(&construction_time);
  cmph_uint32 space_usage = chd_packed_size(mphf) * 8;
  construction_time = construction_time - construction_time_begin;
  fprintf(stdout, "%u\t%.2f\t%u\t%.4f\t%.4f\n", nkeys, c,
          chd_ph->keys_per_bucket, construction_time,
          space_usage / (double)nkeys);
#endif

  return mphf;
}

void chd_load(FILE *fd, cmph_t *mphf) {
  chd_data_t *chd = (chd_data_t *)malloc(sizeof(chd_data_t));

  DEBUGP("Loading chd mphf\n");
  mphf->data = chd;

  CHK_FREAD(&chd->packed_chd_phf_size, sizeof(cmph_uint32), (size_t)1, fd);
  DEBUGP("Loading CHD_PH perfect hash function with %u bytes from disk\n",
         chd->packed_chd_phf_size);
  chd->packed_chd_phf =
      (cmph_uint8 *)calloc((size_t)chd->packed_chd_phf_size, (size_t)1);
  CHK_FREAD(chd->packed_chd_phf, chd->packed_chd_phf_size, (size_t)1, fd);

  CHK_FREAD(&chd->packed_cr_size, sizeof(cmph_uint32), (size_t)1, fd);
  DEBUGP("Loading Compressed rank structure, which has %u bytes\n",
         chd->packed_cr_size);
  chd->packed_cr = (cmph_uint8 *)calloc((size_t)chd->packed_cr_size, (size_t)1);
  CHK_FREAD(chd->packed_cr, chd->packed_cr_size, (size_t)1, fd);

  // if more room in f than current position, TODO use fstat and ftell instead
  mphf->o = (cmph_uint32 *)malloc(sizeof(cmph_uint32) * mphf->size);
  cmph_uint32 nread = fread(mphf->o, sizeof(cmph_uint32), (size_t)mphf->size, fd);
  if (nread != mphf->size){
	  free(mphf->o);
	  mphf->o = NULL;
  }
#ifdef DEBUG
  if (mphf->o) {
	  fprintf(stderr, "O: ");
	  for (cmph_uint32 i = 0; i < mphf->size; ++i) fprintf(stderr, "%u ", mphf->o[i]);
	  fprintf(stderr, "\n");
  }
#endif
}

//void chd_ph_search_packed_compile(FILE *out, hash_state_t *state);

int chd_compile(cmph_t *mphf, cmph_config_t *mph, FILE *out) {
    chd_data_t *data = (chd_data_t *)mphf->data;
    chd_config_data_t *config = (chd_config_data_t *)mph->data;
    chd_ph_config_data_t *chd_ph = (chd_ph_config_data_t *)config->chd_ph->data;

    chd_ph_data_t chd_ph_data = {0};
    hash_state_t state = {0};
    compressed_seq_t cs = {0};
    compressed_rank_t cr = {0};
    select_t sel = {0};
    chd_ph_unpack((uint32_t *)data->packed_chd_phf, &chd_ph_data, &state, &cs);
    compressed_rank_unpack(data->packed_cr, &cr, &sel);

    hash_state_t *states = &state;
    DEBUGP("Compiling chd\n");
    fprintf(out, "#include <assert.h>\n");
#ifdef DEBUG
    fprintf(out, "#include \"debug.h\"\n");
#else
    //fprintf(out, "#define DEBUGP(...)\n");
#endif
    hash_state_compile(1, (hash_state_t **)&states, 1, out);

    compressed_seq_data_compile(out, "cs", &cs);
    compressed_seq_query_compile(out, &cs);
    compressed_rank_data_compile(out, "cr", &cr);
    compressed_rank_query_compile(out, &cr);

    fprintf(out, "\nuint32_t %s_search(const char* key, uint32_t keylen) {\n", mph->c_prefix);
    fprintf(out, "    /* n: %u */\n", chd_ph->n);
    fprintf(out, "    /* m: %u */\n", chd_ph->m);
    fprintf(out, "    /* nbuckets: %u */\n", chd_ph->nbuckets);
    fprintf(out, "    uint32_t disp, p0, p1, f, g, h;\n");
    fprintf(out, "    uint32_t bin_idx, rank;\n");
    fprintf(out, "    uint32_t hv[3];\n");
#ifdef DEBUG
    fprintf(out, "    DEBUGP(\"n: %u, m: %u, nbuckets: %u, seed: %u\\n\");\n",
            chd_ph->n, chd_ph->m, chd_ph->nbuckets, states[0].seed);
#endif
    fprintf(out, "    %s_hash_vector(%u, (const unsigned char*)key, keylen, hv);\n",
            cmph_hash_names[states[0].hashfunc], states[0].seed);
#ifdef DEBUG
    fprintf(out, "    DEBUGP(\"hv: {%%u, %%u, %%u}\\n\", hv[0], hv[1], hv[2]);\n");
#endif
    fprintf(out, "    g = hv[0] %% %u;\n", chd_ph->nbuckets);
    fprintf(out, "    f = hv[1] %% %u;\n", chd_ph->n);
    fprintf(out, "    h = hv[2] %% %u + 1;\n", chd_ph->n - 1);
    fprintf(out, "    disp = compressed_seq_query(&cs, g);\n");
#ifdef DEBUG
    fprintf(out, "    DEBUGP(\"g: %%u, disp: %%u\\n\", g, disp);\n");
#endif
    fprintf(out, "    p0 = disp %% %u;\n", chd_ph->n);
    fprintf(out, "    p1 = disp / %u;\n", chd_ph->n);
#ifdef DEBUG
    fprintf(out, "    DEBUGP(\"p0: %%u, p1: %%u\\n\", p0, p1);\n");
#endif
    fprintf(out, "    bin_idx = (uint32_t)((f + ((uint64_t)h)*p0 + p1) %% %u);\n",
            chd_ph->n);
#ifdef DEBUG
    fprintf(out, "    DEBUGP(\"bin_idx: %%u\\n\", bin_idx);\n");
#endif
    fprintf(out, "    rank = compressed_rank_query(bin_idx);\n");
#ifdef DEBUG
    fprintf(out, "    DEBUGP(\"rank: %%u\\n\", rank);\n");
#endif
    fprintf(out, "    return bin_idx - rank;\n");
    fprintf(out, "};\n");
    if (mphf->o) {
      uint32_compile(out, "ordering_table", mphf->o, chd_ph->m);
      fprintf(out, "uint32_t %s_order(uint32_t id) {\n", mph->c_prefix);
      fprintf(out, "    assert(id < %u);\n", chd_ph->m);
      fprintf(out, "    return ordering_table[id];\n");
      fprintf(out, "}\n");
    }
    fprintf(out, "uint32_t %s_size(void) {\n", mph->c_prefix);
    fprintf(out, "    return %u;\n}\n", chd_ph->m);
    fclose(out);
    return 1;
}

int chd_dump(cmph_t *mphf, FILE *fd) {
  chd_data_t *data = (chd_data_t *)mphf->data;

  __cmph_dump(mphf, fd);
  // Dumping CHD_PH perfect hash function

  DEBUGP("Dumping CHD_PH perfect hash function with %u bytes to disk\n",
         data->packed_chd_phf_size);
  CHK_FWRITE(&data->packed_chd_phf_size, sizeof(cmph_uint32), (size_t)1, fd);
  CHK_FWRITE(data->packed_chd_phf, data->packed_chd_phf_size, (size_t)1, fd);

  DEBUGP("Dumping compressed rank structure with %u bytes to disk\n", 1);
  CHK_FWRITE(&data->packed_cr_size, sizeof(cmph_uint32), (size_t)1, fd);
  CHK_FWRITE(data->packed_cr, data->packed_cr_size, (size_t)1, fd);

  if (mphf->o) {
    DEBUGP("Dumping ordering_table\n");
    CHK_FWRITE(mphf->o, (size_t)mphf->size, sizeof(cmph_uint32), fd);
#ifdef DEBUG
    fprintf(stderr, "O: ");
    for (cmph_uint32 i = 0; i < mphf->size; ++i) fprintf(stderr, "%u ", mphf->o[i]);
    fprintf(stderr, "\n");
#endif
  }

  return 1;
}

void chd_destroy(cmph_t *mphf) {
  chd_data_t *data = (chd_data_t *)mphf->data;
  free(data->packed_chd_phf);
  free(data->packed_cr);
  free(data);
  if(mphf->o)
    free(mphf->o);
  free(mphf);
}

static inline cmph_uint32 _chd_search(void *packed_chd_phf, void *packed_cr,
                                      const char *key, cmph_uint32 keylen) {
  cmph_uint32 bin_idx = cmph_search_packed(packed_chd_phf, key, keylen);
  cmph_uint32 rank = compressed_rank_query_packed(packed_cr, bin_idx);
  return bin_idx - rank;
}

cmph_uint32 chd_search(cmph_t *mphf, const char *key, cmph_uint32 keylen) {
  chd_data_t *chd = (chd_data_t *)mphf->data;
  return _chd_search(chd->packed_chd_phf, chd->packed_cr, key, keylen);
}

void chd_pack(cmph_t *mphf, void *packed_mphf) {
  chd_data_t *data = (chd_data_t *)mphf->data;
  cmph_uint32 *ptr = (cmph_uint32 *)packed_mphf;
  cmph_uint8 *ptr8;

  // packing packed_cr_size and packed_cr
  *ptr = data->packed_cr_size;
  ptr8 = (cmph_uint8 *)(ptr + 1);

  memcpy(ptr8, data->packed_cr, data->packed_cr_size);
  ptr8 += data->packed_cr_size;

  ptr = (cmph_uint32 *)ptr8;
  *ptr = data->packed_chd_phf_size;

  ptr8 = (cmph_uint8 *)(ptr + 1);
  memcpy(ptr8, data->packed_chd_phf, data->packed_chd_phf_size);
}

cmph_uint32 chd_packed_size(cmph_t *mphf) {
  chd_data_t *data = (chd_data_t *)mphf->data;
  return (cmph_uint32)(sizeof(CMPH_ALGO) + 2 * sizeof(cmph_uint32) +
                       data->packed_cr_size + data->packed_chd_phf_size);
}

cmph_uint32 chd_search_packed(void *packed_mphf, const char *key,
                              cmph_uint32 keylen) {

  cmph_uint32 *ptr = (cmph_uint32 *)packed_mphf;
  cmph_uint32 packed_cr_size = *ptr++;
  cmph_uint8 *packed_chd_phf =
      ((cmph_uint8 *)ptr) + packed_cr_size + sizeof(cmph_uint32);
  return _chd_search(packed_chd_phf, ptr, key, keylen);
}
