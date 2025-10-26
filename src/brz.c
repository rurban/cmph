#include "graph.h"
#include "fch.h"
#include "fch_structs.h"
#include "bmz8.h"
#include "bmz8_structs.h"
#include "brz.h"
#include "cmph_structs.h"
#include "brz_structs.h"
#include "buffer_manager.h"
#include "cmph.h"
#include "hash.h"
#include "bitbool.h"
#include "compile.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#define MAX_BUCKET_SIZE 255
//#define DEBUG
#include "debug.h"

static int brz_gen_mphf(cmph_config_t *mph);
static cmph_uint32 brz_min_index(cmph_uint32 * vector, cmph_uint32 n);
static void brz_destroy_keys_vd(cmph_uint8 ** keys_vd, cmph_uint32 nkeys);
static char * brz_copy_partial_fch_mphf(fch_data_t * fchf, cmph_uint32 *buflen);
static char * brz_copy_partial_bmz8_mphf(brz_config_data_t *brz, bmz8_data_t * bmzf, cmph_uint32 index,  cmph_uint32 *buflen);
brz_config_data_t *brz_config_new(void)
{
	brz_config_data_t *brz = NULL;
        brz = (brz_config_data_t *)xmalloc(sizeof(brz_config_data_t));
        if (!brz) return NULL;
        brz->algo = CMPH_FCH;
	brz->b = 128;
	brz->size   = NULL;
	brz->offset = NULL;
	brz->g      = NULL;
	brz->h1 = NULL;
	brz->h2 = NULL;
	//brz->h0 = NULL;
	brz->memory_availability = 1024*1024;
	brz->tmp_dir = (cmph_uint8 *)xcalloc((size_t)10, sizeof(cmph_uint8));
	brz->mphf_fd = NULL;
	strcpy((char *)(brz->tmp_dir), "/var/tmp/");
	assert(brz);
	return brz;
}

void brz_config_destroy(cmph_config_t *mph)
{
	brz_config_data_t *data = (brz_config_data_t *)mph->data;
	free(data->tmp_dir);
	DEBUGP("Destroying algorithm dependent data\n");
	free(data);
}

// support 3 independent hash functions, but mostly just one for all
// but we need all 3 set
void brz_config_set_hashfuncs(cmph_config_t *mph, CMPH_HASH *hashfuncs)
{
	CMPH_HASH *hashptr = hashfuncs;
	CMPH_HASH def = *hashptr;
	char done = 0;
	if (def >= CMPH_HASH_DJB2) // brz fails with weak hashes easily
	    def = CMPH_HASH_JENKINS;
	int i = 0;
	for (; i<3; i++) // set three hash functions
	{
		if (done || *hashptr == CMPH_HASH_COUNT) {
		    done = 1;
		    mph->hashfuncs[i] = def;
		} else {
		    mph->hashfuncs[i] = *hashptr++;
		    mph->nhashfuncs = i + 1;
		}
	}
}

void brz_config_set_memory_availability(cmph_config_t *mph, cmph_uint32 memory_availability)
{
	brz_config_data_t *brz = (brz_config_data_t *)mph->data;
	if(memory_availability > 0) brz->memory_availability = memory_availability*1024*1024;
}

void brz_config_set_tmp_dir(cmph_config_t *mph, cmph_uint8 *tmp_dir)
{
	brz_config_data_t *brz = (brz_config_data_t *)mph->data;
	if(tmp_dir)
	{
		size_t len = strlen((char *)tmp_dir);
		free(brz->tmp_dir);
		if(tmp_dir[len-1] != '/')
		{
			brz->tmp_dir = (cmph_uint8 *)xcalloc((size_t)len+2, sizeof(cmph_uint8));
			sprintf((char *)(brz->tmp_dir), "%s/", (char *)tmp_dir);
		}
		else
		{
			brz->tmp_dir = (cmph_uint8 *)xcalloc((size_t)len+1, sizeof(cmph_uint8));
			sprintf((char *)(brz->tmp_dir), "%s", (char *)tmp_dir);
		}

	}
}

void brz_config_set_mphf_fd(cmph_config_t *mph, FILE *mphf_fd)
{
	brz_config_data_t *brz = (brz_config_data_t *)mph->data;
	brz->mphf_fd = mphf_fd;
	assert(brz->mphf_fd);
}

void brz_config_set_b(cmph_config_t *mph, cmph_uint32 b)
{
	brz_config_data_t *brz = (brz_config_data_t *)mph->data;
	if(b <= 64 || b >= 175)
		b = 128;
	brz->b = (cmph_uint8)b;
}

void brz_config_set_algo(cmph_config_t *mph, CMPH_ALGO algo)
{
	if (algo == CMPH_BMZ8 || algo == CMPH_FCH) // supported algorithms
	{
		brz_config_data_t *brz = (brz_config_data_t *)mph->data;
		brz->algo = algo;
	}
	else {
	    // should not happen
	    fprintf(stderr, "Unsupported brz algo %s, only bmz8 or fch.\n",
		    cmph_names[algo]);
	    exit(1);
	}
}

cmph_t *brz_new(cmph_config_t *mph, double c)
{
	cmph_t *mphf = NULL;
	brz_data_t *brzf = NULL;
	cmph_uint32 i;
	cmph_uint32 iterations = 20;
	cmph_uint32 *ordering_table = NULL;
	brz_config_data_t *brz = (brz_config_data_t *)mph->data;
	DEBUGP("c: %f\n", c);

        // Since we keep dumping partial pieces of the MPHF as it gets created
        // the caller must set the file to store the resulting MPHF before calling
        // this function.
        if (brz->mphf_fd == NULL)
            return NULL;

	switch(brz->algo) // validating restrictions over parameter c.
	{
		case CMPH_BMZ8:
			if (c == 0 || c >= 2.0)
			    c = 1;
			break;
		case CMPH_FCH:
			if (c <= 2.0)
			    c = 2.6;
			break;
		default:
			assert(0);
	}
	brz->c = c;
	brz->m = mph->key_source->nkeys;
        if (brz->m < 5)
        	brz->c = 5;

	DEBUGP("m: %u\n", brz->m);
        brz->k = (cmph_uint32)ceil(brz->m/((double)brz->b));
	DEBUGP("k: %u\n", brz->k);
	brz->size = (cmph_uint8 *)xcalloc((size_t)brz->k, sizeof(cmph_uint8));

	// Clustering the keys by graph id.
	if (mph->verbosity)
	    fprintf(stderr, "Partitioning %u keys into %u sets with algo %s.\n",
		    brz->m, brz->k, cmph_names[brz->algo]);

	while(1)
	{
		int ok;
		DEBUGP("hash function 3\n");
		brz->h0 = hash_state_new(mph->hashfuncs[2], brz->k);
		DEBUGP("Generating graphs\n");
		ok = brz_gen_mphf(mph);
		if (!ok)
		{
			--iterations;
			hash_state_destroy(brz->h0);
			brz->h0 = NULL;
			DEBUGP("%u iterations remaining to create the graphs in a external file\n", iterations);
			if (mph->verbosity)
				fprintf(stderr, "Failure: A graph with more than 255 keys was created - %u iterations remaining\n", iterations);
			if (iterations == 0)
				break;
		}
		else
			break;
	}
	if (iterations == 0)
	{
		DEBUGP("Graphs with more than 255 keys were created in all 20 iterations\n");
		free(brz->size);
		return NULL;
	}
	DEBUGP("Graphs generated\n");

	brz->offset = (cmph_uint32 *)xcalloc((size_t)brz->k, sizeof(cmph_uint32));
	for (i = 1; i < brz->k; ++i)
		brz->offset[i] = brz->size[i-1] + brz->offset[i-1];

	// Generating a mphf
	mphf = (cmph_t *)xmalloc(sizeof(cmph_t));
	mphf->algo = mph->algo;
	mphf->o = ordering_table;
	brzf = (brz_data_t *)xmalloc(sizeof(brz_data_t));
	brzf->g = brz->g;
	brz->g = NULL; //transfer memory ownership
	// FIXME h1 and h2 are empty, just dumped to disc. need to fill them
	brzf->h1 = brz->h1;
	brz->h1 = NULL; //transfer memory ownership
	brzf->h2 = brz->h2;
	brz->h2 = NULL; //transfer memory ownership
	brzf->h0 = brz->h0;
	brz->h0 = NULL; //transfer memory ownership
	brzf->size = brz->size;
	brz->size = NULL; //transfer memory ownership
	brzf->offset = brz->offset;
	brz->offset = NULL; //transfer memory ownership
	brzf->k = brz->k;
	brzf->c = brz->c;
	brzf->m = brz->m;
	brzf->algo = brz->algo;
	mphf->data = brzf;
	mphf->size = brz->m;

	if (mph->do_ordering_table) {
	    ordering_table = (cmph_uint32 *)xcalloc((size_t)mphf->size, sizeof(cmph_uint32));
	    assert(ordering_table);
	    if (mph->verbosity)
		fprintf(stderr, "Create ordering table\n");
	    mph->key_source->rewind(mph->key_source->data);
	    for (cmph_uint32 i = 0; i < mphf->size; i++) {
		cmph_uint32 h, keylen;
		char *key = NULL;
		mph->key_source->read(mph->key_source->data, &key, &keylen);
		h = brz_search(mphf, key, keylen);
		ordering_table[h] = i;
		mph->key_source->dispose(key);
	    }
	    mphf->o = ordering_table;
	}

	DEBUGP("Successfully generated minimal perfect hash\n");
	if (mph->verbosity)
		fprintf(stderr, "Successfully generated minimal perfect hash function\n");
	return mphf;
}

static int brz_gen_mphf(cmph_config_t *mph)
{
	cmph_uint32 i, e, error;
	brz_config_data_t *brz = (brz_config_data_t *)mph->data;
	cmph_uint32 memory_usage = 0;
	cmph_uint32 nkeys_in_buffer = 0;
	cmph_uint8 *buffer = (cmph_uint8 *)xmalloc((size_t)brz->memory_availability);
	cmph_uint32 *buckets_size = (cmph_uint32 *)xcalloc((size_t)brz->k, sizeof(cmph_uint32));
	cmph_uint32 *keys_index = NULL;
	cmph_uint8 **buffer_merge = NULL;
	cmph_uint32 *buffer_h0 = NULL;
	cmph_uint32 nflushes = 0;
	cmph_uint32 h0 = 0;
	FILE *tmp_fd = NULL;
	buffer_manager_t * buff_manager = NULL;
	char *filename = NULL;
	char *key = NULL;
	cmph_uint32 keylen;
	cmph_uint32 cur_bucket = 0;
	cmph_uint8 nkeys_vd = 0;
	cmph_uint8 ** keys_vd = NULL;

	mph->key_source->rewind(mph->key_source->data);
	DEBUGP("Generating graphs from %u keys with algo %s\n", brz->m,
	       cmph_names[brz->algo]);
	// Partitioning
	for (e = 0; e < brz->m; ++e)
	{
		mph->key_source->read(mph->key_source->data, &key, &keylen);

		/* Buffers management */
		if (memory_usage + keylen + sizeof(keylen) > brz->memory_availability) // flush buffers
		{
			if(mph->verbosity)
				fprintf(stderr, "Flushing %u\n", nkeys_in_buffer);
			cmph_uint32 value = buckets_size[0];
			cmph_uint32 sum = 0;
			cmph_uint32 keylen1 = 0;
			buckets_size[0]   = 0;
			for(i = 1; i < brz->k; i++)
			{
				if(buckets_size[i] == 0) continue;
				sum += value;
				value = buckets_size[i];
				buckets_size[i] = sum;
			}
			memory_usage = 0;
			keys_index = (cmph_uint32 *)xcalloc((size_t)nkeys_in_buffer, sizeof(cmph_uint32));
			for(i = 0; i < nkeys_in_buffer; i++)
			{
				memcpy(&keylen1, buffer + memory_usage, sizeof(keylen1));
				h0 = hash(brz->h0, (char *)(buffer + memory_usage + sizeof(keylen1)), keylen1) % brz->k;
				keys_index[buckets_size[h0]] = memory_usage;
				buckets_size[h0]++;
				memory_usage +=  keylen1 + (cmph_uint32)sizeof(keylen1);
			}
			filename = (char *)xcalloc(strlen((char *)(brz->tmp_dir)) + 11, sizeof(char));
			sprintf(filename, "%s%u.cmph", brz->tmp_dir, nflushes);
			tmp_fd = fopen(filename, "wb");
			free(filename);
			filename = NULL;
			for(i = 0; i < nkeys_in_buffer; i++)
			{
				memcpy(&keylen1, buffer + keys_index[i], sizeof(keylen1));
				CHK_FWRITE(buffer + keys_index[i], (size_t)1, keylen1 + sizeof(keylen1), tmp_fd);
			}
			nkeys_in_buffer = 0;
			memory_usage = 0;
			memset((void *)buckets_size, 0, brz->k*sizeof(cmph_uint32));
			nflushes++;
			free(keys_index);
			fclose(tmp_fd);
		}
		memcpy(buffer + memory_usage, &keylen, sizeof(keylen));
		memcpy(buffer + memory_usage + sizeof(keylen), key, (size_t)keylen);
		memory_usage += keylen + (cmph_uint32)sizeof(keylen);
		h0 = hash(brz->h0, key, keylen) % brz->k;

		if ((brz->size[h0] == MAX_BUCKET_SIZE)
		    || (brz->algo == CMPH_BMZ8 &&
			((brz->c >= 1.0)
			 && (cmph_uint8)(brz->c * brz->size[h0]) < brz->size[h0])))
		{
			if (buff_manager)
				buffer_manager_destroy(buff_manager);
			free(buffer);
			free(buckets_size);
			mph->key_source->dispose(key);
			return 0;
		}
		brz->size[h0] = (cmph_uint8)(brz->size[h0] + 1U);
		buckets_size[h0] ++;
		nkeys_in_buffer++;
		mph->key_source->dispose(key);
	}
	if (memory_usage != 0) // flush buffers
	{
		if(mph->verbosity)
			fprintf(stderr, "Flushing %u\n", nkeys_in_buffer);
		cmph_uint32 value = buckets_size[0];
		cmph_uint32 sum = 0;
		cmph_uint32 keylen1 = 0;
		buckets_size[0]   = 0;
		for(i = 1; i < brz->k; i++)
		{
			if(buckets_size[i] == 0) continue;
			sum += value;
			value = buckets_size[i];
			buckets_size[i] = sum;
		}
		memory_usage = 0;
		keys_index = (cmph_uint32 *)xcalloc((size_t)nkeys_in_buffer, sizeof(cmph_uint32));
		for(i = 0; i < nkeys_in_buffer; i++)
		{
			memcpy(&keylen1, buffer + memory_usage, sizeof(keylen1));
			h0 = hash(brz->h0, (char *)(buffer + memory_usage + sizeof(keylen1)), keylen1) % brz->k;
			keys_index[buckets_size[h0]] = memory_usage;
			buckets_size[h0]++;
			memory_usage += keylen1 + (cmph_uint32)sizeof(keylen1);
		}
		filename = (char *)xcalloc(strlen((char *)(brz->tmp_dir)) + 11, sizeof(char));
		sprintf(filename, "%s%u.cmph", brz->tmp_dir, nflushes);
		tmp_fd = fopen(filename, "wb");
		free(filename);
		filename = NULL;
		for(i = 0; i < nkeys_in_buffer; i++)
		{
			memcpy(&keylen1, buffer + keys_index[i], sizeof(keylen1));
			CHK_FWRITE(buffer + keys_index[i], (size_t)1, keylen1 + sizeof(keylen1), tmp_fd);
		}
		nkeys_in_buffer = 0;
		memory_usage = 0;
		memset((void *)buckets_size, 0, brz->k*sizeof(cmph_uint32));
		nflushes++;
		free(keys_index);
		fclose(tmp_fd);
	}

	free(buffer);
	free(buckets_size);
	if(nflushes > 1024) return 0; // Too many files generated.
	// mphf generation
	if(mph->verbosity)
		fprintf(stderr, "\nMPHF generation \n");
	/* Starting to dump to disk the resulting MPHF: __cmph_dump function */
	CHK_FWRITE(cmph_names[CMPH_BRZ], (size_t)(strlen(cmph_names[CMPH_BRZ]) + 1), (size_t)1, brz->mphf_fd);
	CHK_FWRITE(&(brz->m), sizeof(brz->m), (size_t)1, brz->mphf_fd);
	CHK_FWRITE(&(brz->c), sizeof(double), (size_t)1, brz->mphf_fd);
	CHK_FWRITE(&(brz->algo), sizeof(brz->algo), (size_t)1, brz->mphf_fd);
	CHK_FWRITE(&(brz->k), sizeof(cmph_uint32), (size_t)1, brz->mphf_fd); // number of MPHFs
	CHK_FWRITE(brz->size, sizeof(cmph_uint8)*(brz->k), (size_t)1, brz->mphf_fd);

	//tmp_fds = (FILE **)xcalloc(nflushes, sizeof(FILE *));
	buff_manager = buffer_manager_new(brz->memory_availability, nflushes);
	buffer_merge = (cmph_uint8 **)xcalloc((size_t)nflushes, sizeof(cmph_uint8 *));
	buffer_h0    = (cmph_uint32 *)xcalloc((size_t)nflushes, sizeof(cmph_uint32));
	if (mph->do_ordering_table) {
	    brz->g  = (cmph_uint8 **)xcalloc((size_t)brz->k, sizeof(cmph_uint8 *));
	    brz->h1 = (hash_state_t **)xcalloc(brz->k, sizeof(hash_state_t*)*2);
	    brz->h2 = (hash_state_t **)xcalloc(brz->k, sizeof(hash_state_t*)*2);
	}

	memory_usage = 0;
	for(i = 0; i < nflushes; i++)
	{
		filename = (char *)xcalloc(strlen((char *)(brz->tmp_dir)) + 11, sizeof(char));
		sprintf(filename, "%s%u.cmph", brz->tmp_dir, i);
		buffer_manager_open(buff_manager, i, filename);
		free(filename);
		filename = NULL;
		key = (char *)buffer_manager_read_key(buff_manager, i, &keylen);
		h0 = hash(brz->h0, key+sizeof(keylen), keylen) % brz->k;
		buffer_h0[i] = h0;
                buffer_merge[i] = (cmph_uint8 *)key;
                key = NULL; //transfer memory ownership
	}
	e = 0;
	keys_vd = (cmph_uint8 **)xcalloc((size_t)MAX_BUCKET_SIZE, sizeof(cmph_uint8 *));
	nkeys_vd = 0;
	error = 0;
	while(e < brz->m)
	{
		i = brz_min_index(buffer_h0, nflushes);
		cur_bucket = buffer_h0[i];
		key = (char *)buffer_manager_read_key(buff_manager, i, &keylen);
		if(key)
		{
			while(key)
			{
				//keylen = strlen(key);
				h0 = hash(brz->h0, key+sizeof(keylen), keylen) % brz->k;
				if (h0 != buffer_h0[i]) break;
				keys_vd[nkeys_vd++] = (cmph_uint8 *)key;
				key = NULL; //transfer memory ownership
				e++;
				key = (char *)buffer_manager_read_key(buff_manager, i, &keylen);
			}
			if (key)
			{
				assert(nkeys_vd < brz->size[cur_bucket]);
				keys_vd[nkeys_vd++] = buffer_merge[i];
				buffer_merge[i] = NULL; //transfer memory ownership
				e++;
				buffer_h0[i] = h0;
				buffer_merge[i] = (cmph_uint8 *)key;
			}
		}
		if(!key)
		{
			assert(nkeys_vd < brz->size[cur_bucket]);
			keys_vd[nkeys_vd++] = buffer_merge[i];
			buffer_merge[i] = NULL; //transfer memory ownership
			e++;
			buffer_h0[i] = UINT_MAX;
		}

		if(nkeys_vd == brz->size[cur_bucket]) // Generating mphf for each bucket.
		{
			cmph_io_adapter_t *source = NULL;
			cmph_config_t *config = NULL;
			cmph_t *mphf_tmp = NULL;
			char *bufmphf = NULL;
			cmph_uint32 buflenmphf = 0;
			// Source of keys
			source = cmph_io_byte_vector_adapter(keys_vd, (cmph_uint32)nkeys_vd);
			config = cmph_config_new(source);
			cmph_config_set_algo(config, brz->algo);
			cmph_config_set_hashfuncs(config, mph->hashfuncs);
			cmph_config_set_graphsize(config, brz->c);
			mphf_tmp = cmph_new(config);
			if (mphf_tmp == NULL)
			{
				if(mph->verbosity)
					fprintf(stderr, "ERROR: Can't generate MPHF for bucket %u out of %u\n",
						cur_bucket + 1, brz->k);
				error = 1;
				cmph_config_destroy(config);
 				brz_destroy_keys_vd(keys_vd, nkeys_vd);
				cmph_io_byte_vector_adapter_destroy(source);
				if (key)
					free(key);
				break;
			}
			if(mph->verbosity)
			{
				if (cur_bucket % 1000 == 0)
					fprintf(stderr, "MPHF for bucket %u out of %u was generated.\n",
						cur_bucket + 1, brz->k);
			}
			switch(brz->algo)
			{
				case CMPH_FCH:
				{
					fch_data_t * fchf = NULL;
					fchf = (fch_data_t *)mphf_tmp->data;
					if (mph->do_ordering_table) {
					    brz->g[cur_bucket] = xmalloc(fchf->b);
					    brz->h1[cur_bucket] = xmalloc(sizeof(hash_state_t) * 2);
					    brz->h2[cur_bucket] = xmalloc(sizeof(hash_state_t) * 2);
					    memcpy(brz->g[cur_bucket], fchf->g, fchf->b);
					    memcpy(brz->h1[cur_bucket], fchf->h1, sizeof(hash_state_t) * 2);
					    memcpy(brz->h2[cur_bucket], fchf->h2, sizeof(hash_state_t) * 2);
					}
					bufmphf = brz_copy_partial_fch_mphf(fchf, &buflenmphf);
				}
					break;
				case CMPH_BMZ8:
				{
					bmz8_data_t * bmzf = NULL;
					bmzf = (bmz8_data_t *)mphf_tmp->data;
					if (mph->do_ordering_table) {
					    brz->g[cur_bucket] = xmalloc(bmzf->n);
					    brz->h1[cur_bucket] = xmalloc(sizeof(hash_state_t));
					    brz->h2[cur_bucket] = xmalloc(sizeof(hash_state_t));
					    memcpy(brz->g[cur_bucket], bmzf->g, bmzf->n);
					    memcpy(brz->h1[cur_bucket], bmzf->hashes[0], sizeof(hash_state_t));
					    memcpy(brz->h2[cur_bucket], bmzf->hashes[1], sizeof(hash_state_t));
					}
					bufmphf = brz_copy_partial_bmz8_mphf(brz, bmzf, cur_bucket,  &buflenmphf);
				}
					break;
				default: assert(0);
			}
		        CHK_FWRITE(bufmphf, (size_t)buflenmphf, (size_t)1, brz->mphf_fd);
			free(bufmphf);
			bufmphf = NULL;
			cmph_config_destroy(config);
 			brz_destroy_keys_vd(keys_vd, nkeys_vd);
			cmph_destroy(mphf_tmp);
			cmph_io_byte_vector_adapter_destroy(source);
			nkeys_vd = 0;
		}
	}
	buffer_manager_destroy(buff_manager);
	free(keys_vd);
	free(buffer_merge);
	free(buffer_h0);
	if (error)
		return 0;
	return 1;
}

static cmph_uint32 brz_min_index(cmph_uint32 * vector, cmph_uint32 n)
{
	cmph_uint32 i, min_index = 0;
	for(i = 1; i < n; i++)
	{
		if(vector[i] < vector[min_index])
			min_index = i;
	}
	return min_index;
}

static void brz_destroy_keys_vd(cmph_uint8 ** keys_vd, cmph_uint32 nkeys)
{
	cmph_uint8 i;
	for(i = 0; i < nkeys; i++) { free(keys_vd[i]); keys_vd[i] = NULL;}
}

static char * brz_copy_partial_fch_mphf(fch_data_t * fchf, cmph_uint32 *buflen)
{
	cmph_uint32 i = 0;
	cmph_uint32 buflenh1 = 0;
	cmph_uint32 buflenh2 = 0;
	char * bufh1 = NULL;
	char * bufh2 = NULL;
	char * buf   = NULL;
	cmph_uint32 n = fchf->b;//brz->size[index];

	hash_state_dump(fchf->h1, "brz: fch->h1", &bufh1, &buflenh1);
	hash_state_dump(fchf->h2, "brz: fch->h2", &bufh2, &buflenh2);
	*buflen = buflenh1 + buflenh2 + n + 2U * (cmph_uint32)sizeof(cmph_uint32);
	buf = (char *)xmalloc((size_t)(*buflen));
	memcpy(buf, &buflenh1, sizeof(cmph_uint32));
	memcpy(buf+sizeof(cmph_uint32), bufh1, (size_t)buflenh1);
	memcpy(buf+sizeof(cmph_uint32)+buflenh1, &buflenh2, sizeof(cmph_uint32));
	memcpy(buf+2*sizeof(cmph_uint32)+buflenh1, bufh2, (size_t)buflenh2);
	for (i = 0; i < n; i++)
        	memcpy(buf+2*sizeof(cmph_uint32)+buflenh1+buflenh2+i,(fchf->g + i), (size_t)1);
	free(bufh1);
	free(bufh2);
	return buf;
}
static char * brz_copy_partial_bmz8_mphf(brz_config_data_t *brz, bmz8_data_t * bmzf, cmph_uint32 index,  cmph_uint32 *buflen)
{
	cmph_uint32 buflenh1 = 0;
	cmph_uint32 buflenh2 = 0;
	char * bufh1 = NULL;
	char * bufh2 = NULL;
	char * buf   = NULL;
	cmph_uint32 n = (cmph_uint32)ceil(brz->c * brz->size[index]);
	hash_state_dump(bmzf->hashes[0], "brz: bmz8->hashes[0]", &bufh1, &buflenh1);
	hash_state_dump(bmzf->hashes[1], "brz: bmz8->hashes[1]", &bufh2, &buflenh2);
	*buflen = buflenh1 + buflenh2 + n + 2U * (cmph_uint32)sizeof(cmph_uint32);
	buf = (char *)xmalloc((size_t)(*buflen));
	memcpy(buf, &buflenh1, sizeof(cmph_uint32));
	memcpy(buf+sizeof(cmph_uint32), bufh1, (size_t)buflenh1);
	memcpy(buf+sizeof(cmph_uint32)+buflenh1, &buflenh2, sizeof(cmph_uint32));
	memcpy(buf+2*sizeof(cmph_uint32)+buflenh1, bufh2, (size_t)buflenh2);
	memcpy(buf+2*sizeof(cmph_uint32)+buflenh1+buflenh2,bmzf->g, (size_t)n);
	free(bufh1);
	free(bufh2);
	return buf;
}

int brz_compile(cmph_t *mphf, cmph_config_t *mph, FILE *out)
{
	brz_data_t *data = (brz_data_t *)mphf->data;
	//brz_config_data_t *config = (brz_config_data_t *)mph->data;
	char do_vector = mph->nhashfuncs == 1 ||
		mph->hashfuncs[0] == mph->hashfuncs[1];
	hash_state_t *hashes[3];
	(void)mph; // TODO
	DEBUGP("Compiling brz\n");
	hashes[0] = data->h1 ? data->h1[0] : data->h0;
	hashes[1] = data->h2 ? data->h2[0] : data->h0;
	hashes[2] = data->h0;
	hash_state_compile(3, hashes, do_vector, out);
	fprintf(out, "// NYI\n");
	if (mphf->o) {
		uint32_compile(out, "ordering_table", mphf->o, data->m);
		fprintf(out, "uint32_t %s_order(uint32_t id) {\n", mph->c_prefix);
		fprintf(out, "    assert(id < %u);\n", data->m);
		fprintf(out, "    return ordering_table[id];\n");
		fprintf(out, "}\n");
	}
	fprintf(out, "uint32_t %s_size(void) {\n", mph->c_prefix);
	fprintf(out, "    return %u;\n}\n", data->m);
	return 0;
}

int brz_dump(cmph_t *mphf, FILE *fd)
{
	brz_data_t *data = (brz_data_t *)mphf->data;
	char *buf = NULL;
	cmph_uint32 buflen;
	DEBUGP("Dumping brz\n");
	DEBUGP("c = %f   k = %u   algo = %u (%s)\n", data->c, data->k, data->algo,
	       cmph_names[data->algo]);
	// The initial part of the MPHF has already been dumped to disk during construction
	// Dumping h0
        hash_state_dump(data->h0, "brz->h0", &buf, &buflen);
        DEBUGP("Dumping hash state with %u bytes to disk\n", buflen);
        CHK_FWRITE(&buflen, sizeof(cmph_uint32), (size_t)1, fd);
        CHK_FWRITE(buf, (size_t)buflen, (size_t)1, fd);
        free(buf);
	// Dumping m and the vector offset.
	CHK_FWRITE(&(data->m), sizeof(cmph_uint32), (size_t)1, fd);
	CHK_FWRITE(data->offset, sizeof(cmph_uint32)*(data->k), (size_t)1, fd);
	if (mphf->o) {
	    DEBUGP("Dumping ordering table with %u entries\n", data->m);
	    CHK_FWRITE(mphf->o, sizeof(cmph_uint32) * (data->m), (size_t)1, fd);
#ifdef DEBUG
	    fprintf(stderr, "O: ");
	    for (cmph_uint32 i = 0; i < data->m; ++i) fprintf(stderr, "%u ", mphf->o[i]);
	    fprintf(stderr, "\n");
#endif
	}
	return 1;
}

void brz_load(FILE *f, cmph_t *mphf)
{
	char *buf = NULL;
	cmph_uint32 buflen;
	cmph_uint32 i, n;
	brz_data_t *brz = (brz_data_t *)xmalloc(sizeof(brz_data_t));

	DEBUGP("Loading brz mphf\n");
	mphf->data = brz;
	CHK_FREAD(&(brz->c), sizeof(double), (size_t)1, f);
	CHK_FREAD(&(brz->algo), sizeof(brz->algo), (size_t)1, f); // Reading algo.
	assert(brz->algo < CMPH_COUNT);
	CHK_FREAD(&(brz->k), sizeof(cmph_uint32), (size_t)1, f);
	brz->size = (cmph_uint8 *)xmalloc(sizeof(cmph_uint8)*brz->k);
	CHK_FREAD(brz->size, sizeof(cmph_uint8)*(brz->k), (size_t)1, f);
	brz->h1 = (hash_state_t **)xmalloc(sizeof(hash_state_t *)*brz->k);
	brz->h2 = (hash_state_t **)xmalloc(sizeof(hash_state_t *)*brz->k);
	brz->g  = (cmph_uint8 **)xcalloc((size_t)brz->k, sizeof(cmph_uint8 *));
	DEBUGP("Reading c = %f   k = %u   algo = %u (%s)\n", brz->c, brz->k, brz->algo,
	       cmph_names[brz->algo]);
	//loading h_i1, h_i2 and g_i.
	for(i = 0; i < brz->k; i++)
	{
		char name[16];
		snprintf(name, sizeof(name)-1, "brz->h1[%u]", i & 0xff);
		// h1
		CHK_FREAD(&buflen, sizeof(cmph_uint32), (size_t)1, f);
		DEBUGP("Hash state 1 has %u bytes\n", buflen);
		buf = (char *)xmalloc((size_t)buflen);
		CHK_FREAD(buf, (size_t)buflen, (size_t)1, f);
		brz->h1[i] = hash_state_load(buf, name);
		free(buf);
		//h2
		CHK_FREAD(&buflen, sizeof(cmph_uint32), (size_t)1, f);
		DEBUGP("Hash state 2 has %u bytes\n", buflen);
		buf = (char *)xmalloc((size_t)buflen);
		CHK_FREAD(buf, (size_t)buflen, (size_t)1, f);
		snprintf(name, sizeof(name)-1, "brz->h2[%u]", i & 0xff);
		brz->h2[i] = hash_state_load(buf, name);
		free(buf);
		switch(brz->algo)
		{
			case CMPH_FCH:
				n = fch_calc_b(brz->c, brz->size[i]);
				break;
			case CMPH_BMZ8:
				n = (cmph_uint32)ceil(brz->c * brz->size[i]);
				break;
			default: assert(0);
		}
		DEBUGP("g[] has %u bytes\n", n);
		brz->g[i] = (cmph_uint8 *)xcalloc((size_t)n, sizeof(cmph_uint8));
		CHK_FREAD(brz->g[i], sizeof(cmph_uint8)*n, (size_t)1, f);
	}
	//loading h0
	CHK_FREAD(&buflen, sizeof(cmph_uint32), (size_t)1, f);
	DEBUGP("Hash state has %u bytes\n", buflen);
	buf = (char *)xmalloc((size_t)buflen);
	CHK_FREAD(buf, (size_t)buflen, (size_t)1, f);
	brz->h0 = hash_state_load(buf, "brz->h0");
	free(buf);

	//loading c, m, and the vector offset.
	CHK_FREAD(&(brz->m), sizeof(cmph_uint32), (size_t)1, f);
	brz->offset = (cmph_uint32 *)xmalloc(sizeof(cmph_uint32)*brz->k);
	CHK_FREAD(brz->offset, sizeof(cmph_uint32)*(brz->k), (size_t)1, f);
	//loading the optional ordering table.
	mphf->o = (cmph_uint32 *)xmalloc(sizeof(cmph_uint32) * brz->m);
	cmph_uint32 nread =
	    fread(mphf->o, sizeof(cmph_uint32), (size_t)brz->m, f);
	if (nread != brz->m) {
	    free(mphf->o);
	    mphf->o = NULL;
	}
#ifdef DEBUG
	if (mphf->o) {
	    fprintf(stderr, "O: ");
	    for (cmph_uint32 i = 0; i < brz->m; ++i)
		fprintf(stderr, "%u ", mphf->o[i]);
	    fprintf(stderr, "\n");
	}
#endif
	return;
}

static cmph_uint32 brz_bmz8_search(brz_data_t *brz, const char *key, cmph_uint32 keylen, cmph_uint32 * fingerprint)
{
	cmph_uint32 h0;

	hash_vector(brz->h0, key, keylen, fingerprint);
	h0 = fingerprint[2] % brz->k;

	cmph_uint32 m = brz->size[h0];
	cmph_uint32 n = (cmph_uint32)ceil(brz->c * m);
	cmph_uint32 h1 = hash(brz->h1[h0], key, keylen) % n;
	cmph_uint32 h2 = hash(brz->h2[h0], key, keylen) % n;
	cmph_uint8 mphf_bucket;

	if (h1 == h2 && ++h2 >= n) h2 = 0;
	mphf_bucket = (cmph_uint8)(brz->g[h0][h1] + brz->g[h0][h2]);
	DEBUGP("key: %.*s h1: %u h2: %u h0: %u\n", (int)keylen, key, h1, h2, h0);
	DEBUGP("key: %.*s g[h1]: %u g[h2]: %u offset[h0]: %u edges: %u\n", (int)keylen, key,
               brz->g[h0][h1], brz->g[h0][h2], brz->offset[h0], brz->m);
	DEBUGP("Address: %u\n", mphf_bucket + brz->offset[h0]);
	return (mphf_bucket + brz->offset[h0]);
}

static cmph_uint32 brz_fch_search(brz_data_t *brz, const char *key, cmph_uint32 keylen, cmph_uint32 * fingerprint)
{
	cmph_uint32 h0;

	hash_vector(brz->h0, key, keylen, fingerprint);
	h0 = fingerprint[2] % brz->k;

	cmph_uint32 m = brz->size[h0];
	cmph_uint32 b = fch_calc_b(brz->c, m);
	double p1 = fch_calc_p1(m);
	double p2 = fch_calc_p2(b);
	cmph_uint32 h1 = hash(brz->h1[h0], key, keylen) % m;
	cmph_uint32 h2 = hash(brz->h2[h0], key, keylen) % m;
	cmph_uint8 mphf_bucket = 0;
	h1 = mixh10h11h12(b, p1, p2, h1);
	mphf_bucket = (cmph_uint8)((h2 + brz->g[h0][h1]) % m);
	return (mphf_bucket + brz->offset[h0]);
}

cmph_uint32 brz_search(cmph_t *mphf, const char *key, cmph_uint32 keylen)
{
	brz_data_t *brz = (brz_data_t *)mphf->data;
	cmph_uint32 fingerprint[3];
	switch(brz->algo)
	{
		case CMPH_FCH:
			return brz_fch_search(brz, key, keylen, fingerprint);
		case CMPH_BMZ8:
			return brz_bmz8_search(brz, key, keylen, fingerprint);
		default: assert(0);
	}
	return 0;
}
void brz_destroy(cmph_t *mphf)
{
	cmph_uint32 i;
	brz_data_t *data = (brz_data_t *)mphf->data;
	if(data->g)
	{
		for(i = 0; i < data->k; i++)
		{
			free(data->g[i]);
			hash_state_destroy(data->h1[i]);
			hash_state_destroy(data->h2[i]);
		}
		free(data->g);
		free(data->h1);
		free(data->h2);
	}
	hash_state_destroy(data->h0);
	free(data->size);
	free(data->offset);
	free(data);
	free(mphf->o);
	free(mphf);
}

/** \fn void brz_pack(cmph_t *mphf, void *packed_mphf);
 *  \brief Support the ability to pack a perfect hash function into a preallocated contiguous memory space pointed by packed_mphf.
 *  \param mphf pointer to the resulting mphf
 *  \param packed_mphf pointer to the contiguous memory area used to store the resulting mphf. The size of packed_mphf must be at least cmph_packed_size()
 */
void brz_pack(cmph_t *mphf, void *packed_mphf)
{
	brz_data_t *data = (brz_data_t *)mphf->data;
	cmph_uint8 * ptr = (cmph_uint8 *)packed_mphf;
	cmph_uint32 i,n;

        // This assumes that if one function pointer is NULL,
        // all the others will be as well.
        if (data->h1 == NULL)
		return;

	// packing internal algo type
	memcpy(ptr, &(data->algo), sizeof(data->algo));
	ptr += sizeof(data->algo);

	// packing h0 type
	CMPH_HASH h0_type = hash_get_type(data->h0);
	memcpy(ptr, &h0_type, sizeof(h0_type));
	ptr += sizeof(h0_type);

	// packing h0
	hash_state_pack(data->h0, ptr);
	ptr += hash_state_packed_size(h0_type);

	// packing k
	memcpy(ptr, &(data->k), sizeof(data->k));
	ptr += sizeof(data->k);

	// packing c
	*((cmph_uint64 *)ptr) = (cmph_uint64)data->c;
	ptr += sizeof(data->c);

	// packing h1 type
	CMPH_HASH h1_type = hash_get_type(data->h1[0]);
	memcpy(ptr, &h1_type, sizeof(h1_type));
	ptr += sizeof(h1_type);

	// packing h2 type
	CMPH_HASH h2_type = hash_get_type(data->h2[0]);
	memcpy(ptr, &h2_type, sizeof(h2_type));
	ptr += sizeof(h2_type);

	// packing size
	memcpy(ptr, data->size, sizeof(cmph_uint8)*data->k);
	ptr += data->k;

	// packing offset
	memcpy(ptr, data->offset, sizeof(cmph_uint32)*data->k);
	ptr += sizeof(cmph_uint32)*data->k;

#if defined (__ia64) || defined (__x86_64__)
        cmph_uint64 * g_is_ptr = (cmph_uint64 *)ptr;
#else
        cmph_uint32 * g_is_ptr = (cmph_uint32 *)ptr;
#endif

	cmph_uint8 * g_i = (cmph_uint8 *) (g_is_ptr + data->k);

	for(i = 0; i < data->k; i++)
	{
#if defined (__ia64) || defined (__x86_64__)
        	*g_is_ptr++ = (cmph_uint64)g_i;
#else
        	*g_is_ptr++ = (cmph_uint32)g_i;
#endif
		// packing h1[i]
		hash_state_pack(data->h1[i], g_i);
		g_i += hash_state_packed_size(h1_type);

		// packing h2[i]
		hash_state_pack(data->h2[i], g_i);
		g_i += hash_state_packed_size(h2_type);

		// packing g_i
		switch(data->algo)
		{
			case CMPH_FCH:
				n = fch_calc_b(data->c, data->size[i]);
				break;
			case CMPH_BMZ8:
				n = (cmph_uint32)ceil(data->c * data->size[i]);
				break;
			default: assert(0);
		}
		memcpy(g_i, data->g[i], sizeof(cmph_uint8)*n);
		g_i += n;

	}

}

/** \fn cmph_uint32 brz_packed_size(cmph_t *mphf);
 *  \brief Return the amount of space needed to pack mphf.
 *  \param mphf pointer to a mphf
 *  \return the size of the packed function or zero for failures
 */
cmph_uint32 brz_packed_size(cmph_t *mphf)
{
	cmph_uint32 i;
	cmph_uint32 size = 0;
	brz_data_t *data = (brz_data_t *)mphf->data;
	CMPH_HASH h0_type;
	CMPH_HASH h1_type;
	CMPH_HASH h2_type;

	// This assumes that if one function pointer is NULL,
	// all the others will be as well.
	if (data->h1 == NULL)
	    return 0U;

	h0_type = hash_get_type(data->h0);
	h1_type = hash_get_type(data->h1[0]);
	h2_type = hash_get_type(data->h2[0]);

	size = (cmph_uint32)(2*sizeof(CMPH_ALGO) + 3*sizeof(CMPH_HASH) + hash_state_packed_size(h0_type) + sizeof(cmph_uint32) +
			sizeof(double) + sizeof(cmph_uint8)*data->k + sizeof(cmph_uint32)*data->k);
	// pointers to g_is
#if defined (__ia64) || defined (__x86_64__)
        size +=  (cmph_uint32) sizeof(cmph_uint64)*data->k;
#else
        size +=  (cmph_uint32) sizeof(cmph_uint32)*data->k;
#endif

	size += hash_state_packed_size(h1_type) * data->k;
	size += hash_state_packed_size(h2_type) * data->k;

	cmph_uint32 n = 0;
	for(i = 0; i < data->k; i++)
	{
   		switch(data->algo)
   		{
   			case CMPH_FCH:
   				n = fch_calc_b(data->c, data->size[i]);
   				break;
   			case CMPH_BMZ8:
   				n = (cmph_uint32)ceil(data->c * data->size[i]);
   				break;
   			default: assert(0);
   		}
		size += n;
	}
	return size;
}



static cmph_uint32 brz_bmz8_search_packed(cmph_uint32 *packed_mphf, const char *key, cmph_uint32 keylen, cmph_uint32 * fingerprint)
{
	CMPH_HASH h0_type = (CMPH_HASH)*packed_mphf++;
	cmph_uint32 *h0_ptr = packed_mphf;
	packed_mphf = (cmph_uint32 *)(((cmph_uint8 *)packed_mphf) + hash_state_packed_size(h0_type));

	cmph_uint32 k = *packed_mphf++;

	double c = (double)(*((cmph_uint64*)packed_mphf));
	packed_mphf += 2;

	CMPH_HASH h1_type = (CMPH_HASH)*packed_mphf++;

	CMPH_HASH h2_type = (CMPH_HASH)*packed_mphf++;

	cmph_uint8 * size = (cmph_uint8 *) packed_mphf;
	packed_mphf = (cmph_uint32 *)(size + k);

	cmph_uint32 * offset = packed_mphf;
	packed_mphf += k;

	cmph_uint32 h0;

	hash_vector_packed(h0_ptr, h0_type, key, keylen, fingerprint);
	h0 = fingerprint[2] % k;

	cmph_uint32 m = size[h0];
	cmph_uint32 n = (cmph_uint32)ceil(c * m);

#if defined (__ia64) || defined (__x86_64__)
        cmph_uint64 * g_is_ptr = (cmph_uint64 *)packed_mphf;
#else
        cmph_uint32 * g_is_ptr = packed_mphf;
#endif

	cmph_uint8 * h1_ptr = (cmph_uint8 *) g_is_ptr[h0];

	cmph_uint8 * h2_ptr = h1_ptr + hash_state_packed_size(h1_type);

	cmph_uint8 * g = h2_ptr + hash_state_packed_size(h2_type);

	cmph_uint32 h1 = hash_packed(h1_ptr, h1_type, key, keylen) % n;
	cmph_uint32 h2 = hash_packed(h2_ptr, h2_type, key, keylen) % n;

	cmph_uint8 mphf_bucket;

	if (h1 == h2 && ++h2 >= n) h2 = 0;
	mphf_bucket = (cmph_uint8)(g[h1] + g[h2]);
	DEBUGP("key: %.*s h1: %u h2: %u h0: %u\n", (int)keylen, key, h1, h2, h0);
	DEBUGP("Address: %u\n", mphf_bucket + offset[h0]);
	return (mphf_bucket + offset[h0]);
}

static cmph_uint32 brz_fch_search_packed(cmph_uint32 *packed_mphf, const char *key, cmph_uint32 keylen, cmph_uint32 * fingerprint)
{
	CMPH_HASH h0_type = (CMPH_HASH)*packed_mphf++;

	cmph_uint32 *h0_ptr = packed_mphf;
	packed_mphf = (cmph_uint32 *)(((cmph_uint8 *)packed_mphf) + hash_state_packed_size(h0_type));

	cmph_uint32 k = *packed_mphf++;

	double c = (double)(*((cmph_uint64*)packed_mphf));
	packed_mphf += 2;

	CMPH_HASH h1_type = (CMPH_HASH)*packed_mphf++;

	CMPH_HASH h2_type = (CMPH_HASH)*packed_mphf++;

	cmph_uint8 * size = (cmph_uint8 *) packed_mphf;
	packed_mphf = (cmph_uint32 *)(size + k);

	cmph_uint32 * offset = packed_mphf;
	packed_mphf += k;

	cmph_uint32 h0;

	hash_vector_packed(h0_ptr, h0_type, key, keylen, fingerprint);
	h0 = fingerprint[2] % k;

	cmph_uint32 m = size[h0];
	cmph_uint32 b = fch_calc_b(c, m);
	double p1 = fch_calc_p1(m);
	double p2 = fch_calc_p2(b);

#if defined (__ia64) || defined (__x86_64__)
        cmph_uint64 * g_is_ptr = (cmph_uint64 *)packed_mphf;
#else
        cmph_uint32 * g_is_ptr = packed_mphf;
#endif

	cmph_uint8 * h1_ptr = (cmph_uint8 *) g_is_ptr[h0];

	cmph_uint8 * h2_ptr = h1_ptr + hash_state_packed_size(h1_type);

	cmph_uint8 * g = h2_ptr + hash_state_packed_size(h2_type);

	cmph_uint32 h1 = hash_packed(h1_ptr, h1_type, key, keylen) % m;
	cmph_uint32 h2 = hash_packed(h2_ptr, h2_type, key, keylen) % m;

	cmph_uint8 mphf_bucket = 0;
	h1 = mixh10h11h12(b, p1, p2, h1);
	mphf_bucket = (cmph_uint8)((h2 + g[h1]) % m);
	return (mphf_bucket + offset[h0]);
}

/** cmph_uint32 brz_search(void *packed_mphf, const char *key, cmph_uint32 keylen);
 *  \brief Use the packed mphf to do a search.
 *  \param  packed_mphf pointer to the packed mphf
 *  \param key key to be hashed
 *  \param keylen key legth in bytes
 *  \return The mphf value
 */
cmph_uint32 brz_search_packed(void *packed_mphf, const char *key, cmph_uint32 keylen)
{
	cmph_uint32 *ptr = (cmph_uint32 *)packed_mphf;
	CMPH_ALGO algo = (CMPH_ALGO)*ptr++;
	cmph_uint32 fingerprint[3];
	switch(algo)
	{
		case CMPH_FCH:
			return brz_fch_search_packed(ptr, key, keylen, fingerprint);
		case CMPH_BMZ8:
			return brz_bmz8_search_packed(ptr, key, keylen, fingerprint);
		default: assert(0);
	}
	assert(0);
	return 0;
}
