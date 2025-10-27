#include "fch.h"
#include "cmph_structs.h"
#include "fch_structs.h"
#include "hash.h"
#include "bitbool.h"
#include "fch_buckets.h"
#include "compile.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#define INDEX 0 /* alignment index within a bucket */
//#define DEBUG
#include "debug.h"

static fch_buckets_t * mapping(cmph_config_t *mph);
static cmph_uint32 * ordering(fch_buckets_t * buckets);
static cmph_uint8 check_for_collisions_h2(fch_config_data_t *fch, fch_buckets_t * buckets, cmph_uint32 *sorted_indexes);
static void permut(cmph_uint32 * vector, cmph_uint32 n);
static cmph_uint8 searching(cmph_config_t *mph, fch_buckets_t *buckets, cmph_uint32 *sorted_indexes, cmph_uint32 *ordering_table);

fch_config_data_t *fch_config_new()
{
	fch_config_data_t *fch;
	fch = (fch_config_data_t *)xmalloc(sizeof(fch_config_data_t));
        if (!fch) return NULL;
	memset(fch, 0, sizeof(fch_config_data_t));
	//fch->m = fch->b = 0;
	//fch->c = fch->p1 = fch->p2 = 0.0;
	//fch->g = NULL;
	//fch->h1 = NULL;
	//fch->h2 = NULL;
	return fch;
}

void fch_config_destroy(cmph_config_t *mph)
{
	fch_config_data_t *data = (fch_config_data_t *)mph->data;
	free(mph->ordering_table);
	//DEBUGP("Destroying algorithm dependent data\n");
	free(data);
}

// support 2 independent hash functions, but mostly just one for both
void fch_config_set_hashfuncs(cmph_config_t *mph, CMPH_HASH *hashfuncs)
{
        //fch_config_data_t *fch = (fch_config_data_t *)mph->data;
	CMPH_HASH *hashptr = hashfuncs;
	cmph_uint32 i = 0;
	while (*hashptr != CMPH_HASH_COUNT)
	{
		if (i >= 2) break; // fch only uses two hash functions
		mph->hashfuncs[i] = *hashptr;
		++i, ++hashptr;
	}
	if (i >= 2) {
		if (mph->hashfuncs[0] == mph->hashfuncs[1])
			mph->nhashfuncs = 1;
		else
			mph->nhashfuncs = 2;
	} else {
		mph->hashfuncs[1] = mph->hashfuncs[0];
		mph->nhashfuncs = 1;
	}
#ifdef DEBUG
	if (mph->nhashfuncs == 1) {
	    DEBUGP("Same hash functions\n");
	}
	else {
	    DEBUGP("Different hash functions\n");
	}
#endif
}

cmph_uint32 mixh10h11h12(cmph_uint32 b, double p1, double p2, cmph_uint32 initial_index)
{
	cmph_uint32 int_p2 = (cmph_uint32)p2;
	if (initial_index < p1)  initial_index %= int_p2;  /* h11 o h10 */
	else { /* h12 o h10 */
		initial_index %= b;
		if(initial_index < p2) initial_index += int_p2;
	}
	return initial_index;
}


cmph_uint32 fch_calc_b(double c, cmph_uint32 m)
{
	return (cmph_uint32)ceil((c*m)/(log((double)m)/log(2.0) + 1));
}

double fch_calc_p1(cmph_uint32 m)
{
	return ceil(0.55*m);
}

double fch_calc_p2(cmph_uint32 b)
{
	return ceil(0.3*b);
}

static fch_buckets_t * mapping(cmph_config_t *mph)
{
	cmph_uint32 i = 0;
	fch_buckets_t *buckets = NULL;
	fch_config_data_t *fch = (fch_config_data_t *)mph->data;
	if (fch->h1) hash_state_destroy(fch->h1);
	fch->h1 = hash_state_new(mph->hashfuncs[0], fch->m);
	fch->b = fch_calc_b(fch->c, fch->m);
	fch->p1 = fch_calc_p1(fch->m);
	fch->p2 = fch_calc_p2(fch->b);
	//DEBUGP("b:%u   p1:%f   p2:%f\n", fch->b, fch->p1, fch->p2);
	buckets = fch_buckets_new(fch->b);

	mph->key_source->rewind(mph->key_source->data);
	for(i = 0; i < fch->m; i++)
	{
		cmph_uint32 h1, keylen;
		char *key = NULL;
		mph->key_source->read(mph->key_source->data, &key, &keylen);
		h1 = hash(fch->h1, key, keylen) % fch->m;
		h1 = mixh10h11h12 (fch->b, fch->p1, fch->p2, h1);
		fch_buckets_insert(buckets, h1, key, keylen);
	}
	return buckets;
}


// returns the buckets indexes sorted by their sizes.
static cmph_uint32 * ordering(fch_buckets_t * buckets)
{
    return fch_buckets_get_indexes_sorted_by_size(buckets);
}

/* Check whether function h2 causes collisions among the keys of each bucket */
static cmph_uint8 check_for_collisions_h2(fch_config_data_t *fch, fch_buckets_t * buckets, cmph_uint32 *sorted_indexes)
{
	//cmph_uint32 max_size = fch_buckets_get_max_size(buckets);
	cmph_uint8 * hashtable = (cmph_uint8 *)xcalloc((size_t)fch->m, sizeof(cmph_uint8));
	cmph_uint32 nbuckets = fch_buckets_get_nbuckets(buckets);
	cmph_uint32 i = 0, index = 0, j =0;
	for (i = 0; i < nbuckets; i++)
	{
		cmph_uint32 nkeys = fch_buckets_get_size(buckets, sorted_indexes[i]);
		memset(hashtable, 0, (size_t)fch->m);
		DEBUGP("bucket %u -- nkeys: %u\n", i, nkeys);
		for (j = 0; j < nkeys; j++)
		{
			char * key = fch_buckets_get_key(buckets, sorted_indexes[i], j);
			cmph_uint32 keylen = fch_buckets_get_keylength(buckets, sorted_indexes[i], j);
			index = hash(fch->h2, key, keylen) % fch->m;
			if(hashtable[index]) { // collision detected
				free(hashtable);
				return 1;
			}
			DEBUGP("%u: key \"%.*s\" index: %u\n", sorted_indexes[i], (int)keylen, key, index);
			hashtable[index] = 1;
		}
	}
	free(hashtable);
	return 0;
}

static void permut(cmph_uint32 * vector, cmph_uint32 n)
{
    cmph_uint32 i, j, b;
    for (i = 0; i < n; i++) {
	j = (cmph_uint32) rand() % n;
	b = vector[i];
	vector[i] = vector[j];
	vector[j] = b;
    }
}

static cmph_uint8 searching(cmph_config_t *mph, fch_buckets_t *buckets, cmph_uint32 *sorted_indexes, cmph_uint32 *ordering_table)
{
	fch_config_data_t *fch = (fch_config_data_t *)mph->data;
	cmph_uint32 *random_table = (cmph_uint32 *)xcalloc((size_t)fch->m, sizeof(cmph_uint32));
	cmph_uint32 *map_table    = (cmph_uint32 *)xcalloc((size_t)fch->m, sizeof(cmph_uint32));
	cmph_uint32 iteration_to_generate_h2 = 0;
	cmph_uint32 searching_iterations     = 0;
	cmph_uint8 restart                   = 0;
	cmph_uint32 nbuckets                 = fch_buckets_get_nbuckets(buckets);
	cmph_uint32 i, j, z, counter = 0, filled_count = 0;
	if (fch->g) free (fch->g);
	fch->g = (cmph_uint32 *)xcalloc((size_t)fch->b, sizeof(cmph_uint32));

	DEBUGP("max bucket size: %u, nbuckets: %u\n", fch_buckets_get_max_size(buckets), nbuckets);

	for(i = 0; i < fch->m; i++)
		random_table[i] = i;
	permut(random_table, fch->m);
	for(i = 0; i < fch->m; i++)
		map_table[random_table[i]] = i;
	do {
		if (fch->h2) hash_state_destroy(fch->h2);
		fch->h2 = hash_state_new(mph->hashfuncs[1], fch->m);
		restart = check_for_collisions_h2(fch, buckets, sorted_indexes);
		filled_count = 0;
		if (!restart)
		{
			searching_iterations++;
			iteration_to_generate_h2 = 0;
			//DEBUGP("searching_iterations: %u\n", searching_iterations);
		}
		else {
			iteration_to_generate_h2++;
			//DEBUGP("iteration_to_generate_h2: %u\n", iteration_to_generate_h2);
		}
		for(i = 0; (i < nbuckets) && !restart; i++) {
			cmph_uint32 bucketsize = fch_buckets_get_size(buckets, sorted_indexes[i]);
			if (bucketsize == 0)
			{
				restart = 0; // false
				break;
			}
			else restart = 1; // true
			for(z = 0; (z < (fch->m - filled_count)) && restart; z++) {
				char * key = fch_buckets_get_key(buckets, sorted_indexes[i], INDEX);
				cmph_uint32 keylen = fch_buckets_get_keylength(buckets, sorted_indexes[i], INDEX);
				cmph_uint32 h2 = hash(fch->h2, key, keylen) % fch->m;
				counter = 0;
				restart = 0; // false
				fch->g[sorted_indexes[i]] = (fch->m + random_table[filled_count + z] - h2) % fch->m;
				//DEBUGP("g[%u]: %u\n", sorted_indexes[i], fch->g[sorted_indexes[i]]);
				j = INDEX;
				do {
					cmph_uint32 index = 0;
					key = fch_buckets_get_key(buckets, sorted_indexes[i], j);
					keylen = fch_buckets_get_keylength(buckets, sorted_indexes[i], j);
					h2 = hash(fch->h2, key, keylen) % fch->m;
					index = (h2 + fch->g[sorted_indexes[i]]) % fch->m;
					DEBUGP("key:\"%.*s\"  \tkeylen:%u index: %u  h2:%u  bucketsize:%u\n",
                                               (int)keylen, key, keylen, index, h2, bucketsize);
					if (map_table[index] >= filled_count) {
						cmph_uint32 y  = map_table[index];
						cmph_uint32 ry = random_table[y];
						random_table[y] = random_table[filled_count];
						random_table[filled_count] = ry;
						map_table[random_table[y]] = y;
						map_table[random_table[filled_count]] = filled_count;
						filled_count++;
						counter ++;
					}
					else {
						restart = 1; // true
						filled_count = filled_count - counter;
						counter = 0;
						break;
					}
					j = (j + 1) % bucketsize;
				} while (j % bucketsize != INDEX);
			}
		}
	} while (restart && (searching_iterations < 10) && (iteration_to_generate_h2 < 1000));

	if (!restart && ordering_table) {
	    mph->key_source->rewind(mph->key_source->data);
	    for (i = 0; i < fch->m; i++) {
		cmph_uint32 h, h1, h2, keylen;
		char *key = NULL;
		mph->key_source->read(mph->key_source->data, &key, &keylen);
		h1 = hash(fch->h1, key, keylen) % fch->m;
		h2 = hash(fch->h2, key, keylen) % fch->m;
		h1 = mixh10h11h12 (fch->b, fch->p1, fch->p2, h1);
		h = (fch->g[h1] + h2) % fch->m;
		ordering_table[h] = i;
		mph->key_source->dispose(key);
	    }
	}
	free(map_table);
	free(random_table);
	return restart;
}



cmph_t *fch_new(cmph_config_t *mph, double c)
{
	cmph_t *mphf = NULL;
	fch_data_t *fchf = NULL;
	cmph_uint32 iterations = 100;
	cmph_uint8 restart_mapping = 0;
	fch_buckets_t *buckets = NULL;
	cmph_uint32 *sorted_indexes = NULL;
	compressed_seq_t co;
	fch_config_data_t *fch = (fch_config_data_t *)mph->data;

	fch->m = mph->key_source->nkeys;
	DEBUGP("m: %u\n", fch->m);
	if (c <= 2)
	    c = 2.6; // validating restrictions over parameter c.
	fch->c = c;
	DEBUGP("c: %f\n", fch->c);
	fch->h1 = NULL;
	fch->h2 = NULL;
	fch->g = NULL;
	if (mph->do_ordering_table)
	    mph->ordering_table = (cmph_uint32 *)xcalloc(fch->m, sizeof(cmph_uint32));

	do
	{
		if (mph->verbosity)
			fprintf(stderr, "Entering mapping step for mph creation of %u keys\n", fch->m);
		if (buckets)
			fch_buckets_destroy(buckets, mph);
		//if (ordering_table)
		//    memset(ordering_table, 0, sizeof(cmph_uint32)*fch->m);
		buckets = mapping(mph);
		if (mph->verbosity)
			fprintf(stderr, "Starting ordering step\n");
		if (sorted_indexes) free (sorted_indexes);
		sorted_indexes = ordering(buckets);
		if (mph->verbosity)
			fprintf(stderr, "Starting searching step.\n");
		restart_mapping = searching(mph, buckets, sorted_indexes, mph->ordering_table);
		iterations--;

        } while(restart_mapping && iterations > 0);

	if (buckets) fch_buckets_destroy(buckets, mph);
	if (sorted_indexes) free (sorted_indexes);
	if (iterations == 0) {
	    free (fch->g);
	    if (mph->ordering_table)
		free(mph->ordering_table);
	    free(mphf);
	    hash_state_destroy(fch->h1);
	    hash_state_destroy(fch->h2);
	    return NULL;
	}
	mphf = (cmph_t *)xcalloc(1, sizeof(cmph_t));
	if (mph->ordering_table) {
	    compressed_seq_init(&co);
	    compressed_seq_generate(&co, mph->ordering_table, fch->m);
	    mphf->packed_co_size = compressed_seq_packed_size(&co);
	    mphf->packed_co = (cmph_uint8 *)xcalloc(mphf->packed_co_size, sizeof(cmph_uint8));
	    compressed_seq_pack(&co, mphf->packed_co);
	    compressed_seq_destroy(&co);
	}

	mphf->algo = mph->algo;
	mphf->size = fch->m;
	fchf = (fch_data_t *)xmalloc(sizeof(fch_data_t));
	fchf->g = fch->g;
	fch->g = NULL; //transfer memory ownership
	fchf->h1 = fch->h1;
	fch->h1 = NULL; //transfer memory ownership
	fchf->h2 = fch->h2;
	fch->h2 = NULL; //transfer memory ownership
	fchf->p2 = fch->p2;
	fchf->p1 = fch->p1;
	fchf->b = fch->b;
	fchf->c = fch->c;
	fchf->m = fch->m;
	mphf->data = fchf;
	mphf->size = fch->m;
	DEBUGP("Successfully generated minimal perfect fch hash\n");
	if (mph->verbosity)
		fprintf(stderr, "Successfully generated minimal perfect hash function\n");
	return mphf;
}

int fch_compile(cmph_t *mphf, cmph_config_t *mph, FILE *out)
{
	fch_data_t *data = (fch_data_t *)mphf->data;
	char g_name[24];
	/*char do_vector = mph->nhashfuncs == 1 ||
	  mph->hashfuncs[0] == mph->hashfuncs[1];*/
	hash_state_t *hl[2] = { data->h1, data->h2 };
	(void)mph;
	DEBUGP("Compiling fch\n");
	hash_state_compile(2, (hash_state_t**)hl, false, out);
	fprintf(out, "#include <assert.h>\n");
	fprintf(out, "#ifdef DEBUG\n");
	fprintf(out, "#include <stdio.h>\n");
	fprintf(out, "#endif\n");

	fprintf(out, "static inline uint32_t mixh10h11h12(uint32_t initial_index) {\n");
	fprintf(out, "    if (initial_index < %f)\n", data->p1);
	fprintf(out, "        initial_index %%= %u;  /* h11 o h10 */\n", (cmph_uint32)data->p2);
	fprintf(out, "    else { /* h12 o h10 */\n");
	fprintf(out, "        initial_index %%= %u;\n", data->b);
	fprintf(out, "        if(initial_index < %f) initial_index += %u;\n",
	       data->p2, (cmph_uint32)data->p2);
	fprintf(out, "    }\n");
	fprintf(out, "    return initial_index;\n");
	fprintf(out, "}\n");
	snprintf(g_name, sizeof(g_name)-1, "_%s_g", mph->c_prefix);
	uint32_compile(out, g_name, data->g, data->b);
	fprintf(out, "\nuint32_t %s_search(const char* key, uint32_t keylen) {\n", mph->c_prefix);
	fprintf(out, "    /* m: %u */\n", data->m);
	fprintf(out, "    /* c: %f */\n", data->c);
	fprintf(out, "    uint32_t h1, h2;\n");
	fprintf(out, "    h1 = %s_hash_0((const unsigned char*)key, keylen) %% %u;\n",
		cmph_hash_names[hl[0]->hashfunc], data->m);
	fprintf(out, "    h2 = %s_hash_1((const unsigned char*)key, keylen) %% %u;\n",
		cmph_hash_names[hl[1]->hashfunc], data->m);
	fprintf(out, "    h1 = mixh10h11h12 (h1);\n");
	fprintf(out, "    assert(h1 < %u);\n", data->b);
	//fprintf(out, "    DEBUGP(\"key: %%s h1: %%u h2: %%u  _%s_g[h1]: %%u\\n\", key, h1, h2, g[h1]);\n", mph->c_prefix);
	fprintf(out, "    return (h2 + %s[h1]) %% %u;\n", g_name, data->m);
	fprintf(out, "}\n");
	if (mph->ordering_table) {
		compressed_seq_t co = {0};
		compressed_seq_unpack((uint32_t *)mphf->packed_co, &co);
		compressed_seq_data_compile(out, "ordering_seq", &co, 0);
		compressed_seq_query_compile(out, &co, 0); // the function
	        //uint32_compile(out, "ordering_table", mph->ordering_table, data->m);
		fprintf(out, "uint32_t %s_order(uint32_t id) {\n", mph->c_prefix);
		fprintf(out, "    assert(id < %u);\n", data->m);
		fprintf(out, "    return compressed_seq_query(&ordering_seq, id);\n");
		fprintf(out, "}\n");
	}
	fprintf(out, "uint32_t %s_size(void) {\n", mph->c_prefix);
	fprintf(out, "    return %u;\n}\n", data->m);
	fclose(out);
	return 1;
}
int fch_dump(cmph_t *mphf, FILE *fd)
{
	char *buf = NULL;
	cmph_uint32 buflen;

	fch_data_t *data = (fch_data_t *)mphf->data;
	__cmph_dump(mphf, fd);

	hash_state_dump(data->h1, "fch->h1", &buf, &buflen);
	DEBUGP("Dumping hash state with %u bytes to disk:\n", buflen);
#ifdef DEBUG
	for (unsigned i=0; i<buflen; i++)
	    fprintf(stderr, "0x%02x ", (unsigned char)buf[i]);
	fprintf(stderr, "\n");
#endif
	CHK_FWRITE(&buflen, sizeof(cmph_uint32), (size_t)1, fd);
	CHK_FWRITE(buf, (size_t)buflen, (size_t)1, fd);
	free(buf);

	hash_state_dump(data->h2, "fch->h2", &buf, &buflen);
	DEBUGP("Dumping hash state with %u bytes to disk:\n", buflen);
#ifdef DEBUG
	for (unsigned i=0; i<buflen; i++)
	    fprintf(stderr, "0x%02x ", (unsigned char)buf[i]);
	fprintf(stderr, "\n");
#endif
	CHK_FWRITE(&buflen, sizeof(cmph_uint32), (size_t)1, fd);
	CHK_FWRITE(buf, (size_t)buflen, (size_t)1, fd);
	free(buf);

	CHK_FWRITE(&(data->m), sizeof(cmph_uint32), (size_t)1, fd);
	CHK_FWRITE(&(data->c), sizeof(double), (size_t)1, fd);
	CHK_FWRITE(&(data->b), sizeof(cmph_uint32), (size_t)1, fd);
	CHK_FWRITE(&(data->p1), sizeof(double), (size_t)1, fd);
	CHK_FWRITE(&(data->p2), sizeof(double), (size_t)1, fd);
	CHK_FWRITE(data->g, sizeof(cmph_uint32)*(data->b), (size_t)1, fd);
#ifdef DEBUG
	cmph_uint32 i;
	fprintf(stderr, "G: ");
	for (i = 0; i < data->b; ++i) fprintf(stderr, "%u ", data->g[i]);
	fprintf(stderr, "\n");
#endif
	// version 2.1 extension
	if (mphf->packed_co_size) {
	    DEBUGP("Dumping packed ordering table\n");
	    CHK_FWRITE(&(mphf->packed_co_size), sizeof(cmph_uint32), (size_t)1, fd);
	    CHK_FWRITE(mphf->packed_co, mphf->packed_co_size,(size_t)1, fd);
	}
	return 1;
}

void fch_load(FILE *f, cmph_t *mphf)
{
	char *buf = NULL;
	cmph_uint32 buflen;
	fch_data_t *fch = (fch_data_t *)xmalloc(sizeof(fch_data_t));

	DEBUGP("Loading fch mphf\n");
	mphf->data = fch;
	DEBUGP("Reading h1\n");
	fch->h1 = NULL;
	CHK_FREAD(&buflen, sizeof(cmph_uint32), (size_t)1, f);
	DEBUGP("Hash state of h1 has %u bytes\n", buflen);
	buf = (char *)xmalloc((size_t)buflen);
	CHK_FREAD(buf, (size_t)buflen, (size_t)1, f);
	fch->h1 = hash_state_load(buf, "fch->h1");
	free(buf);

	mphf->data = fch;
	DEBUGP("Reading h2\n");
	fch->h2 = NULL;
	CHK_FREAD(&buflen, sizeof(cmph_uint32), (size_t)1, f);
	DEBUGP("Hash state of h2 has %u bytes\n", buflen);
	buf = (char *)xmalloc((size_t)buflen);
	CHK_FREAD(buf, (size_t)buflen, (size_t)1, f);
	fch->h2 = hash_state_load(buf, "fch->h2");
	free(buf);

	DEBUGP("Reading m and n\n");
	CHK_FREAD(&(fch->m), sizeof(cmph_uint32), (size_t)1, f);
	CHK_FREAD(&(fch->c), sizeof(double), (size_t)1, f);
	CHK_FREAD(&(fch->b), sizeof(cmph_uint32), (size_t)1, f);
	CHK_FREAD(&(fch->p1), sizeof(double), (size_t)1, f);
	CHK_FREAD(&(fch->p2), sizeof(double), (size_t)1, f);

	fch->g = (cmph_uint32 *)xmalloc(sizeof(cmph_uint32)*fch->b);
	CHK_FREAD(fch->g, fch->b*sizeof(cmph_uint32), (size_t)1, f);
#ifdef DEBUG
	cmph_uint32 i;
	fprintf(stderr, "G: ");
	for (i = 0; i < fch->b; ++i) fprintf(stderr, "%u ", fch->g[i]);
	fprintf(stderr, "\n");
#endif
	//loading the optional ordering table. since version 2.1
	cmph_uint32 nread =
	    fread(&(mphf->packed_co_size), sizeof(cmph_uint32), (size_t)1, f);
	if (nread == 1) {
	    mphf->packed_co = (cmph_uint8 *)xmalloc(mphf->packed_co_size);
	    CHK_FREAD(mphf->packed_co, mphf->packed_co_size, (size_t)1, f);
	} else {
	    mphf->packed_co_size = 0;
	}
	return;
}

cmph_uint32 fch_search(cmph_t *mphf, const char *key, cmph_uint32 keylen)
{
	fch_data_t *fch = (fch_data_t *)mphf->data;
	cmph_uint32 h1 = hash(fch->h1, key, keylen) % fch->m;
	cmph_uint32 h2 = hash(fch->h2, key, keylen) % fch->m;
	h1 = mixh10h11h12 (fch->b, fch->p1, fch->p2, h1);
	cmph_uint32 index = (fch->g[h1] + h2) % fch->m;
	DEBUGP("key:%.*s   g[h1]:%u h2:%u\t=> %u\n", (int)keylen, key, fch->g[h1], h2, index);
	return index;
}
void fch_destroy(cmph_t *mphf)
{
	fch_data_t *data = (fch_data_t *)mphf->data;
	free(data->g);
	hash_state_destroy(data->h1);
	hash_state_destroy(data->h2);
	free(data);
	free(mphf);
}

/** \fn void fch_pack(cmph_t *mphf, void *packed_mphf);
 *  \brief Support the ability to pack a perfect hash function into a preallocated contiguous memory space pointed by packed_mphf.
 *  \param mphf pointer to the resulting mphf
 *  \param packed_mphf pointer to the contiguous memory area used to store the resulting mphf. The size of packed_mphf must be at least cmph_packed_size()
 */
void fch_pack(cmph_t *mphf, void *packed_mphf)
{
	fch_data_t *data = (fch_data_t *)mphf->data;
	cmph_uint8 * ptr = (cmph_uint8 *)packed_mphf;

	// packing h1 type
	CMPH_HASH h1_type = hash_get_type(data->h1);
	*((cmph_uint32 *) ptr) = h1_type;
	ptr += sizeof(cmph_uint32);

	// packing h1
	hash_state_pack(data->h1, ptr);
	ptr += hash_state_packed_size(h1_type);

	// packing h2 type
	CMPH_HASH h2_type = hash_get_type(data->h2);
	*((cmph_uint32 *) ptr) = h2_type;
	ptr += sizeof(cmph_uint32);

	// packing h2
	hash_state_pack(data->h2, ptr);
	ptr += hash_state_packed_size(h2_type);

	// packing m
	*((cmph_uint32 *) ptr) = data->m;
	ptr += sizeof(data->m);

	// packing b
	*((cmph_uint32 *) ptr) = data->b;
	ptr += sizeof(data->b);

	// packing p1
	*((cmph_uint64 *)ptr) = (cmph_uint64)data->p1;
	ptr += sizeof(data->p1);

	// packing p2
	*((cmph_uint64 *)ptr) = (cmph_uint64)data->p2;
	ptr += sizeof(data->p2);

	// packing g
	memcpy(ptr, data->g, sizeof(cmph_uint32)*(data->b));
}

/** \fn cmph_uint32 fch_packed_size(cmph_t *mphf);
 *  \brief Return the amount of space needed to pack mphf.
 *  \param mphf pointer to a mphf
 *  \return the size of the packed function or zero for failures
 */
cmph_uint32 fch_packed_size(cmph_t *mphf)
{
	fch_data_t *data = (fch_data_t *)mphf->data;
	CMPH_HASH h1_type = hash_get_type(data->h1);
	CMPH_HASH h2_type = hash_get_type(data->h2);

	return (cmph_uint32)(sizeof(CMPH_ALGO) + hash_state_packed_size(h1_type) + hash_state_packed_size(h2_type) +
			4*sizeof(cmph_uint32) + 2*sizeof(double) + sizeof(cmph_uint32)*(data->b));
}


/** cmph_uint32 fch_search(void *packed_mphf, const char *key, cmph_uint32 keylen);
 *  \brief Use the packed mphf to do a search.
 *  \param  packed_mphf pointer to the packed mphf
 *  \param key key to be hashed
 *  \param keylen key legth in bytes
 *  \return The mphf value
 */
cmph_uint32 fch_search_packed(void *packed_mphf, const char *key, cmph_uint32 keylen)
{
	cmph_uint8 *h1_ptr = (cmph_uint8 *)packed_mphf;
	CMPH_HASH h1_type  = (CMPH_HASH)*((cmph_uint32 *)h1_ptr);
	h1_ptr += 4;

	cmph_uint8 *h2_ptr = h1_ptr + hash_state_packed_size(h1_type);
	CMPH_HASH h2_type  = (CMPH_HASH)*((cmph_uint32 *)h2_ptr);
	h2_ptr += 4;

	cmph_uint32 *g_ptr = (cmph_uint32 *)(h2_ptr + hash_state_packed_size(h2_type));

	cmph_uint32 m = *g_ptr++;

	cmph_uint32 b = *g_ptr++;

	double p1 = (double)(*((cmph_uint64 *)g_ptr));
	g_ptr += 2;

	double p2 = (double)(*((cmph_uint64 *)g_ptr));
	g_ptr += 2;

	cmph_uint32 h1 = hash_packed(h1_ptr, h1_type, key, keylen) % m;
	cmph_uint32 h2 = hash_packed(h2_ptr, h2_type, key, keylen) % m;

	h1 = mixh10h11h12 (b, p1, p2, h1);
	return (h2 + g_ptr[h1]) % m;
}
