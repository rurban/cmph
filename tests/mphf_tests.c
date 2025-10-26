#ifdef WIN32
#include "../wingetopt.h"
#else
#include <getopt.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <assert.h>

#include "cmph.h"
#ifdef WIN32
#define VERSION "0.8"
#else
#include "config.h"
#endif


void usage(const char *prg)
{
	fprintf(stderr, "usage: %s [-v] [-h] [-V] [-k nkeys] [-m file.mph] [-a algorithm] [-f hash_function] keysfile\n", prg);
}
void usage_long(const char *prg)
{
	cmph_uint32 i;
	fprintf(stderr, "usage: %s [-v] [-h] [-V] [-k nkeys] [-m file.mph] [-a algorithm] [-f hash_function] keysfile\n", prg);
	fprintf(stderr, "MPHFs testing tool\n\n");
	fprintf(stderr, "  -h\t print this help message\n");
	fprintf(stderr, "  -V\t print version number and exit\n");
	fprintf(stderr, "  -v\t increase verbosity (may be used multiple times)\n");
	fprintf(stderr, "  -k\t number of keys\n");
	fprintf(stderr, "  -m\t minimum perfect hash function file\n");
	fprintf(stderr, "  -a\t algorithm - valid values are\n");
	for (i = 0; i < CMPH_COUNT; ++i) fprintf(stderr, "    \t  * %s\n", cmph_names[i]);
	fprintf(stderr, "  -f\t hash function (may be used multiple times) - valid values are\n");
	for (i = 0; i < CMPH_HASH_COUNT; ++i) fprintf(stderr, "    \t  * %s\n", cmph_hash_names[i]);
	fprintf(stderr, "  keysfile\t line separated file with keys\n");
}

int main(int argc, char **argv)
{
	char verbosity = 0;
	char *mphf_file = NULL;
	const char *keys_file = NULL;
	FILE *mphf_fd = stdout;
	FILE *keys_fd;
	CMPH_ALGO mph_algo = CMPH_CHM;
	CMPH_HASH *hashes = NULL;
	cmph_uint32 nhashes = 0;
	cmph_uint32 nkeys = UINT_MAX;
	cmph_uint32 seed = UINT_MAX;
	cmph_uint32 i = 0;
	cmph_t *mphf = NULL;
	cmph_config_t *config;
	cmph_io_adapter_t *source;
	while (1)
	{
		char ch = (char)getopt(argc, argv, "hVvs:k:m:f:a:");
		if (ch == -1) break;
		switch (ch)
		{
			case 'k':
			        {
					char *endptr;
					nkeys = (cmph_uint32) strtoul(optarg, &endptr, 10);
					if(*endptr != 0) {
						fprintf(stderr, "Invalid number of keys %s\n", optarg);
						exit(1);
					}
				}
				break;
			case 'm':
				mphf_file = xstrdup(optarg);
				break;
			case 's':
				{
					char *cptr;
					seed = (cmph_uint32)strtoul(optarg, &cptr, 10);
					if(*cptr != 0) {
						fprintf(stderr, "Invalid seed %s\n", optarg);
						exit(1);
					}
				}
				break;
			case 'a':
				{
				char valid = 0;
				for (i = 0; i < CMPH_COUNT; ++i)
				{
					if (strcmp(cmph_names[i], optarg) == 0)
					{
						mph_algo = (CMPH_ALGO)i;
						valid = 1;
						break;
					}
				}
				if (!valid)
				{
					fprintf(stderr, "Invalid mph algorithm: %s.\nValid algorithms are:", optarg);
                                        for (i = 0; i < CMPH_COUNT; ++i)
                                                fprintf(stderr, " %s", cmph_names[i]);
                                        fprintf(stderr, "\n");
					return -1;
				}
				}
				break;
			case 'f': {
				char valid = 0;
				for (i = 0; i < CMPH_HASH_COUNT; ++i)
				{
					if (strcmp(cmph_hash_names[i], optarg) == 0)
					{
						hashes = (CMPH_HASH *)xrealloc(hashes, sizeof(CMPH_HASH) * ( nhashes + 2 ));
						hashes[nhashes] = (CMPH_HASH)i;
						hashes[nhashes + 1] = CMPH_HASH_COUNT;
						++nhashes;
						valid = 1;
						break;
					}
				}
				if (!valid)
				{
					fprintf(stderr, "Invalid hash function: %s\nValid names are:", optarg);
                                        for (i = 0; i < CMPH_HASH_COUNT; ++i) {
                                                fprintf(stderr, " %s",cmph_hash_names[i]);
                                        }
                                        fprintf(stderr, "\n");
					return -1;
				}
			        }
				break;
			case 'v':
				++verbosity;
				break;
			case 'V':
				printf("%s\n", VERSION);
				return 0;
			case 'h':
				usage_long(argv[0]);
				return 0;
			default:
				usage(argv[0]);
				return 1;
		}
	}

	if (optind != argc - 1)
	{
		usage(argv[0]);
		return 1;
	}
	keys_file = argv[optind];

	int ret = 0;
	if (mphf_file == NULL)
	{
		mphf_file = (char *)xmalloc(strlen(keys_file) + 5);
		memcpy(mphf_file, keys_file, strlen(keys_file));
		memcpy(mphf_file + strlen(keys_file), ".mph\0", (size_t)5);
	}

	keys_fd = fopen(keys_file, "r");

	if (keys_fd == NULL)
	{
		fprintf(stderr, "Unable to open file %s: %s\n", keys_file, strerror(errno));
		free(hashes);
		return -1;
	}

	if (nkeys == UINT_MAX) source = cmph_io_nlfile_adapter(keys_fd);
	else source = cmph_io_nlnkfile_adapter(keys_fd, nkeys);

	config = cmph_config_new(source);
	cmph_config_set_algo(config, mph_algo);
	if (nhashes) cmph_config_set_hashfuncs(config, hashes);
	if (seed == UINT_MAX) seed = (cmph_uint32)time(NULL);
	srand(seed);

	mphf_fd = fopen(mphf_file, "rb");
	if (mphf_fd == NULL)
	{
		fprintf(stderr, "Unable to open input file %s: %s\n", mphf_file, strerror(errno));
		free(mphf_file);
		free(source);
		return -1;
	}
	cmph_config_set_mphf_fd(config, mphf_fd);
	mphf = cmph_load(mphf_fd);
	cmph_config_destroy(config);
	fclose(mphf_fd);
	if (!mphf)
	{
		fprintf(stderr, "Unable to parser input file %s\n", mphf_file);
		free(hashes);
		free(mphf_file);
		free(source);
		return -1;
	}
	cmph_uint32 siz = cmph_size(mphf);
	cmph_uint32 *hasharray = (cmph_uint32*)xcalloc(siz, sizeof(cmph_uint32));
	cmph_uint32 *o = cmph_ordering_table(mphf);
	if (o && !o[0] && !o[1]) {
		fprintf(stderr, "TODO empty ordering table with %s\n", cmph_names[mph_algo]);
		o = NULL;
	}
	//check all keys
	for (i = 0; i < source->nkeys; ++i)
	{
		cmph_uint32 h;
		char *buf;
		cmph_uint32 buflen = 0;
		source->read(source->data, &buf, &buflen);
		h = cmph_search(mphf, buf, buflen);
		if (!(h < siz))
		{
			fprintf(stderr, "Unknown key %.*s in the input.\n", buflen, buf);
			ret = 1;
		} else if(hasharray[h])
		{
			fprintf(stderr, "Duplicated or unknown key %.*s in the input\n", buflen, buf);
			ret = 1;
		} else
			hasharray[h] = 1;
		// with the order-preserving algo CHM or the optional ordering_table,
		// check h against the key index also.
		if (mph_algo == CMPH_CHM) {
			if (i != h) {
				fprintf(stderr, "Keys are not in the right order: %u, expected %u\n", h, i);
				ret = 1;
			}
		} else {
			if (o && o[h] != i) {
				fprintf(stderr, "Keys are not in the right order: %d for %u, "
					"expected %u\n", (int32_t)o[h], h, i);
				ret = 1;
			}
		}

		if (verbosity)
			printf("%s -> %u\n", buf, h);
		source->dispose(buf);
	}

	cmph_destroy(mphf);
	free(hasharray);
	free(hashes);

	fclose(keys_fd);
	free(mphf_file);
	free(source);
	return ret;
}
