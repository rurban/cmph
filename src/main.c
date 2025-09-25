#ifdef WIN32
#include "wingetopt.h"
#else
#include <getopt.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#ifdef __linux__
#include <unistd.h>
#include <fcntl.h>
#endif
#include <errno.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <assert.h>

#include "cmph.h"
#include "hash.h"
#include "debug.h"

#ifdef WIN32
#define VERSION "2.1"
#else
#include "config.h"
#endif

void usage(const char *prg)
{
	fprintf(stderr, "usage: %s [-vVhrR] [-k nkeys] [-f hash_function] [-g [-c algorithm_dependent_value][-s seed] ] [-a algorithm] [-M memory_in_MB] [-b algorithm_dependent_value] [-t keys_per_bin] [-d tmp_dir] [-m file.mph] [-o c_file] [-p c_func_prefix] keysfile\n", prg);
}
void usage_long(const char *prg)
{
	cmph_uint32 i;
	usage(prg);
	fprintf(stderr, "Minimum perfect hashing tool\n\n");
	fprintf(stderr, "  -h\t print this help message\n");
	fprintf(stderr, "  -c\t c value determines:\n");
	fprintf(stderr, "    \t  * the number of vertices in the graph for the algorithms BMZ and CHM\n");
	fprintf(stderr, "    \t  * the number of bits per key required in the FCH algorithm\n");
	fprintf(stderr, "    \t  * the load factor in the CHD_PH algorithm\n");
	fprintf(stderr, "  -a\t algorithm - valid values are\n");
	for (i = 0; i < CMPH_COUNT; ++i) fprintf(stderr, "    \t  * %s\n", cmph_names[i]);
	fprintf(stderr, "  -f\t hash function (may be used multiple times) - valid values are\n");
	for (i = 0; i < CMPH_HASH_COUNT; ++i) fprintf(stderr, "    \t  * %s\n", cmph_hash_names[i]);
	fprintf(stderr, "  -V\t print version number and exit\n");
	fprintf(stderr, "  -v\t increase verbosity (may be used multiple times)\n");
	fprintf(stderr, "  -k\t number of keys\n");
	fprintf(stderr, "  -g\t generate a .mph file\n");
	fprintf(stderr, "  -C\t generate a C file\n");
	fprintf(stderr, "  -o\t name of C file (default: stdout)\n");
	fprintf(stderr, "  -p\t name of C functions prefix (default: cmph_c_)\n");
	fprintf(stderr, "  -r\t create an ordering table for the key indices\n");
	fprintf(stderr, "  -R\t create an ordered keyfile\n");
	fprintf(stderr, "  -s\t random seed\n");
	fprintf(stderr, "  -m\t minimum perfect hash function file \n");
	fprintf(stderr, "  -M\t main memory availability (in MB) used in BRZ algorithm \n");
	fprintf(stderr, "  -d\t temporary directory used in BRZ algorithm \n");
	fprintf(stderr, "  -b\t the meaning of this parameter depends on the algorithm selected in the -a option:\n");
	fprintf(stderr, "    \t  * For BRZ it is used to make the maximal number of keys in a bucket lower than 256.\n");
	fprintf(stderr, "    \t    In this case its value should be an integer in the range [64,175]. Default is 128.\n\n");
	fprintf(stderr, "    \t  * For BDZ it is used to determine the size of some precomputed rank\n");
	fprintf(stderr, "    \t    information and its value should be an integer in the range [3,10]. Default\n");
	fprintf(stderr, "    \t    is 7. The larger is this value, the more compact are the resulting functions\n");
	fprintf(stderr, "    \t    and the slower are them at evaluation time.\n\n");
	fprintf(stderr, "    \t  * For CHD and CHD_PH it is used to set the average number of keys per bucket\n");
	fprintf(stderr, "    \t    and its value should be an integer in the range [1,32]. Default is 4. The\n");
	fprintf(stderr, "    \t    larger is this value, the slower is the construction of the functions.\n");
	fprintf(stderr, "    \t    This parameter has no effect for other algorithms.\n\n");
	fprintf(stderr, "  -t\t set the number of keys per bin for a t-perfect hashing function. A t-perfect\n");
	fprintf(stderr, "    \t hash function allows at most t collisions in a given bin. This parameter applies\n");
	fprintf(stderr, "    \t only to the CHD and CHD_PH algorithms. Its value should be an integer in the\n");
	fprintf(stderr, "    \t range [1,128]. Default is 1\n");
	fprintf(stderr, "  keysfile\t line separated file with keys\n");
}

cmph_uint32 reorder_keyfile(cmph_io_adapter_t *source, cmph_uint32 *o, FILE *reordered_fd)
{
	cmph_uint32 i, err = 0;
	if (o == NULL) {
		fprintf(stderr, "Missing ordering table\n");
		return 1;
	}
	DEBUGP("reorder_keyfile for the %u keys\n", source->nkeys);
	for (cmph_uint32 j = 0; j < source->nkeys; ++j) { // index
		for (i = 0; i < source->nkeys; ++i) {  // line
			cmph_uint32 buflen = 0;
			if (j == o[i]) {
				char *buf;
				int len = source->read(source->data, &buf, &buflen);
				if (len >= 0) {
					DEBUGP("found \"%.*s\" [%u] at line %u\n", (int)buflen, buf, j, i);
				} else {
					DEBUGP("error with \"%.*s\" [%u] at line %u\n", (int)buflen, buf, j, i);
				}
				fprintf(reordered_fd, "%.*s\n", (int)buflen, buf);
				free(buf);
				break;
			} else { // optim: skip allocating buf. just fgets and continue
				source->read(source->data, NULL, &buflen);
			}
		}
		// check progress or abort
		if (i == source->nkeys - 1) {
			fprintf(stderr, "Reordering: Did not find key %u at line %u in keys file\n",
				j, o[j]);
			fprintf(reordered_fd, "\n");
			err++;
			//break;
		}
		source->rewind(source->data);
	}
	source->rewind(source->data);
	return err;
}

int main(int argc, char **argv)
{
	cmph_uint32 verbosity = 0;
	char *mphf_file = NULL;
	FILE *mphf_fd = stdout;
	const char *keys_file = NULL;
	char *reordered_keyfile = NULL;
	FILE *keys_fd;
	FILE *reordered_fd = NULL;
	unsigned int nkeys = UINT_MAX;
	unsigned int seed = UINT_MAX;
	CMPH_HASH *hashes = NULL;
	cmph_uint32 nhashes = 0;
	cmph_uint32 i;
	CMPH_ALGO mph_algo = CMPH_CHM;
	double c = 0;
	cmph_config_t *config = NULL;
	cmph_t *mphf = NULL;
	char *tmp_dir = NULL;
	char *c_file = NULL;
	char *c_prefix = NULL;
	cmph_io_adapter_t *source;
	cmph_uint32 memory_availability = 0;
	cmph_uint32 b = 0;
	cmph_uint32 keys_per_bin = 1;
	cmph_uint32 *ordering_table = NULL;
	unsigned errs = 0;
	char generate = 0;
	char compile = 0;
	char do_ordering_table = 0;
	char do_reorder_keyfile = 0;
	char is_minimal = 1;

#define FREE_STRDUP_ARGS			\
	if (hashes)				\
		free(hashes);			\
	if (c_file)				\
		free(c_file);			\
	if (mphf_file)				\
		free(mphf_file);		\
	if (c_prefix)				\
		free(c_prefix);			\
	if (tmp_dir)				\
		free(tmp_dir)

	while (1)
	{
		char ch = (char)getopt(argc, argv, "hVvgCrRc:k:a:M:b:t:f:m:d:s:o:p:");
		if (ch == -1) break;
		switch (ch)
		{
			case 's':
				{
					char *cptr;
					seed = (unsigned)strtoul(optarg, &cptr, 10);
					if(*cptr != 0) {
						fprintf(stderr, "Invalid seed %s\n", optarg);
						FREE_STRDUP_ARGS;
						exit(1);
					}
				}
				break;
			case 'c':
				{
					char *endptr;
					c = strtod(optarg, &endptr);
					if(*endptr != 0) {
						fprintf(stderr, "Invalid c value %s\n", optarg);
						FREE_STRDUP_ARGS;
						exit(1);
					}
				}
				break;
			case 'C':
				compile = 1;
				break;
			case 'g':
				generate = 1;
				break;
			case 'r':
				do_ordering_table = 1;
				break;
			case 'R':
				do_reorder_keyfile = 1;
				break;
			case 'k':
			        {
					char *endptr;
					nkeys = (cmph_uint32)strtoul(optarg, &endptr, 10);
					if(*endptr != 0) {
						fprintf(stderr, "Invalid number of keys %s\n", optarg);
						FREE_STRDUP_ARGS;
						exit(1);
					}
				}
				break;
			case 'm':
				mphf_file = strdup(optarg);
				break;
			case 'd':
				tmp_dir = strdup(optarg);
				break;
			case 'o':
				c_file = strdup(optarg);
				break;
			case 'p':
				c_prefix = strdup(optarg);
				break;
			case 'M':
				{
					char *cptr;
					memory_availability = (cmph_uint32)strtoul(optarg, &cptr, 10);
					if(*cptr != 0) {
						fprintf(stderr, "Invalid memory availability %s\n", optarg);
						FREE_STRDUP_ARGS;
						exit(1);
					}
				}
				break;
			case 'b':
				{
					char *cptr;
					b =  (cmph_uint32)strtoul(optarg, &cptr, 10);
					if(*cptr != 0) {
						fprintf(stderr, "Parameter b was not found: %s\n", optarg);
						FREE_STRDUP_ARGS;
						exit(1);
					}
				}
				break;
			case 't':
				{
					char *cptr;
					keys_per_bin = (cmph_uint32)strtoul(optarg, &cptr, 10);
					if(*cptr != 0) {
						fprintf(stderr, "Parameter t was not found: %s\n", optarg);
						FREE_STRDUP_ARGS;
						exit(1);
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
					FREE_STRDUP_ARGS;
					return -1;
				}
				if (mph_algo == CMPH_BDZ_PH || mph_algo == CMPH_CHD_PH)
				    is_minimal = 0;
				}
				break;
			case 'f':
				{
				char valid = 0;
				for (i = 0; i < CMPH_HASH_COUNT; ++i)
				{
					if (strcmp(cmph_hash_names[i], optarg) == 0)
					{
						if (hashes) {
							CMPH_HASH *new_hashes = (CMPH_HASH *)realloc(hashes, sizeof(CMPH_HASH) * ( nhashes + 2 ));
							if (!new_hashes)
							{
								perror("realloc");
								return -1;
							}
							else
								hashes = new_hashes;
						}
						else
							hashes = (CMPH_HASH *)calloc(2, sizeof(CMPH_HASH));
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
					FREE_STRDUP_ARGS;
                                        for (i = 0; i < CMPH_HASH_COUNT; ++i) {
                                                fprintf(stderr, " %s", cmph_hash_names[i]);
                                        }
                                        fprintf(stderr, "\n");
					return -1;
				}
				}
				break;
			default:
				usage(argv[0]);
				FREE_STRDUP_ARGS;
				return 1;
		}
	}

	if (optind != argc - 1)
	{
		usage(argv[0]);
		FREE_STRDUP_ARGS;
		return 1;
	}
	keys_file = argv[optind];

	if (mphf_file == NULL)
	{
		mphf_file = (char *)malloc(strlen(keys_file) + 5);
		memcpy(mphf_file, keys_file, strlen(keys_file));
		memcpy(mphf_file + strlen(keys_file), ".mph\0", (size_t)5);
	}
	if (do_reorder_keyfile)
	{
		size_t sz = strlen(keys_file) + strlen(cmph_names[mph_algo]) + 6;
		reordered_keyfile = (char *)malloc(sz+1);
		if (snprintf(reordered_keyfile, sz, "%s_%s.ord", keys_file, cmph_names[mph_algo]) <= 0) {
			perror("snprintf");
			FREE_STRDUP_ARGS;
			exit(1);
		}
	}

	keys_fd = fopen(keys_file, "r");

	if (keys_fd == NULL)
	{
		fprintf(stderr, "Unable to open file %s: %s\n", keys_file, strerror(errno));
		FREE_STRDUP_ARGS;
		if (do_reorder_keyfile)
			free(reordered_keyfile);
		return -1;
	}

	if (seed == UINT_MAX) {
#ifdef __linux__
		int fd = open("/dev/urandom", O_RDONLY);
		int nread = read(fd, &seed, sizeof(seed));
		assert(nread == (int)sizeof(seed));
	        srand(seed);
#else
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
	        srand((unsigned)ts.tv_sec ^ ts.tv_nsec ^ (unsigned int)(uintptr_t)&ts);
#endif
	}
	if (verbosity)
		printf("Using seed %u\n", seed);
	if (nkeys == UINT_MAX)
		source = cmph_io_nlfile_adapter(keys_fd);
	else
		source = cmph_io_nlnkfile_adapter(keys_fd, nkeys);
	if (generate || compile)
	{
		//Create mphf
		if (generate)
			mphf_fd = fopen(mphf_file, "wb");
		config = cmph_config_new(source);
		cmph_config_set_seed(config, seed);
		cmph_config_set_algo(config, mph_algo);
		if (nhashes)
			cmph_config_set_hashfuncs(config, hashes);
		if (c_prefix)
			cmph_config_set_c_prefix(config, c_prefix);
		cmph_config_set_verbosity(config, verbosity);
		if (do_ordering_table || do_reorder_keyfile)
			cmph_config_set_ordering_table(config);
		cmph_config_set_verbosity(config, verbosity);
		cmph_config_set_tmp_dir(config, (cmph_uint8 *) tmp_dir);
		cmph_config_set_mphf_fd(config, mphf_fd);
		cmph_config_set_memory_availability(config, memory_availability);
		cmph_config_set_b(config, b);
		cmph_config_set_keys_per_bin(config, keys_per_bin);

		//if((mph_algo == CMPH_BMZ || mph_algo == CMPH_BRZ) && c >= 2.0) c=1.15;
		if(mph_algo == CMPH_BMZ && c >= 2.0)
			c = 1.15;
		if (c != 0)
			cmph_config_set_graphsize(config, c);
		mphf = cmph_new(config);

		if (mphf == NULL)
		{
			fprintf(stderr, "Unable to create %sperfect hashing function with seed %u\n", is_minimal ? "minimal " : "", seed);
			cmph_config_destroy(config);
			if (generate) {
				fclose(mphf_fd);
				unlink(mphf_file);
			}
			FREE_STRDUP_ARGS;
			if (do_reorder_keyfile)
				free(reordered_keyfile);
			free(source);
			return -1;
		}
		if (do_ordering_table || do_reorder_keyfile)
			ordering_table = cmph_ordering_table(mphf);

		if (generate && mphf_fd == NULL)
		{
			fprintf(stderr, "Unable to open output file %s: %s\n", mphf_file, strerror(errno));
			cmph_config_destroy(config);
			FREE_STRDUP_ARGS;
			if (do_reorder_keyfile)
				free(reordered_keyfile);
			free(source);
			return -1;
		}
		if (compile)
			cmph_compile(mphf, config, c_file, keys_file);
		if (generate)
			cmph_dump(mphf, mphf_fd);
		if (do_reorder_keyfile) {
			reordered_fd = fopen(reordered_keyfile, "wb");
			if (reordered_fd == NULL || ordering_table == NULL)
			{
				if (!reordered_fd)
					fprintf(stderr, "Unable to open output file %s: %s\n", reordered_keyfile,
						strerror(errno));
				if (!ordering_table || ordering_table[0] == ordering_table[1])
					fprintf(stderr, "Unable to create ordering table\n");
				FREE_STRDUP_ARGS;
				if (do_reorder_keyfile)
					free(reordered_keyfile);
				cmph_io_nlfile_adapter_destroy(source);
				return -1;
			}
			int errs = reorder_keyfile(source, ordering_table, reordered_fd);
			if (verbosity) {
				if (!errs)
					printf("Wrote %s\n", reordered_keyfile);
				else
					printf("Wrote %s with %u errors\n", reordered_keyfile, errs);
			}
			fclose(reordered_fd);
			free(reordered_keyfile);
		}
		cmph_config_destroy(config);
		cmph_destroy(mphf);
		if (generate)
			fclose(mphf_fd);
	}
	else
	{
		cmph_uint8 *hashtable = NULL;
		if (c_file)
			free(c_file);
		mphf_fd = fopen(mphf_file, "rb");
		if (mphf_fd == NULL)
		{
			fprintf(stderr, "Unable to open input file %s: %s\n", mphf_file, strerror(errno));
			cmph_config_destroy(config);
			FREE_STRDUP_ARGS;
			if (do_reorder_keyfile)
				free(reordered_keyfile);
			cmph_io_nlfile_adapter_destroy(source);
			return -1;
		}
		mphf = cmph_load(mphf_fd);
		if (do_reorder_keyfile) {
			reordered_fd = fopen(reordered_keyfile, "wb");
			if (reordered_fd == NULL)
			{
				fprintf(stderr, "Unable to open output file %s: %s\n", reordered_keyfile,
					strerror(errno));
				FREE_STRDUP_ARGS;
				free(reordered_keyfile);
				cmph_io_nlfile_adapter_destroy(source);
				return -1;
			}
			ordering_table = cmph_ordering_table(mphf);
			int err = reorder_keyfile(source, ordering_table, reordered_fd);
			if (verbosity && !err)
				printf("Wrote %s\n", reordered_keyfile);
			fclose(reordered_fd);
			free(reordered_keyfile);
		}
		fclose(mphf_fd);
		if (!mphf)
		{
			fprintf(stderr, "Unable to parse input file %s\n", mphf_file);
			cmph_config_destroy(config);
			FREE_STRDUP_ARGS;
			cmph_io_nlfile_adapter_destroy(source);
			//free(source);
			return -1;
		}
		cmph_uint32 siz = cmph_size(mphf);
		hashtable = (cmph_uint8*)calloc(siz, sizeof(cmph_uint8));
		// check all keys
		for (i = 0; i < source->nkeys; ++i)
		{
			cmph_uint32 h;
			char *buf;
			cmph_uint32 buflen = 0;
			source->read(source->data, &buf, &buflen);
			// TODO what? we need to write them ordered.
			h = cmph_search(mphf, buf, buflen);
			if (!(h < siz))
			{
				if (errs < 10)
					fprintf(stderr, "Unknown key %*s in the input.\n", buflen, buf);
				errs++;
			} else if(hashtable[h] >= keys_per_bin)
			{
				if (errs < 10) {
					fprintf(stderr, "More than %u keys were mapped to bin %u\n", keys_per_bin, h);
					fprintf(stderr, "Duplicated or unknown key %*s in the input\n", buflen, buf);
				}
				errs++;
			} else
				hashtable[h]++;
			if (verbosity) {
#ifdef DEBUG
				printf("%s -> %u\n", buf, h);
#else
				if (i < 10)
					printf("%s -> %u\n", buf, h);
				else if (i == 10)
					printf("...\n");
#endif
			}
			source->dispose(buf);
		}
		if (errs >= 10)
			fprintf(stderr, "%u key errors\n", errs);

		cmph_destroy(mphf);
		free(hashtable);
	}
	FREE_STRDUP_ARGS;
	fclose(keys_fd);
	free(source);
	return errs ? 1 : 0;
}
