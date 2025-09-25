/* Create ordering table */
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
#include <libgen.h>

#include "cmph.h"
#ifdef WIN32
#define VERSION "2.1"
#else
#include "config.h"
#endif

#define MIN(a,b) a < b ? a : b

void usage(char *prg)
{
	fprintf(stderr, "usage: %s [-v] [-h] [-V] [-k nkeys] [-m file.mph] keysfile\n", basename(prg));
}
void usage_long(char *prg)
{
	usage(prg);
	fprintf(stderr, "Create ordering table for MPHF\n\n");
	fprintf(stderr, "  -v\t increase verbosity (may be used multiple times)\n");
	fprintf(stderr, "  -k\t number of keys\n");
	fprintf(stderr, "  -m\t minimum perfect hash function file\n");
	fprintf(stderr, "  keysfile\t line separated file with keys\n");

	fprintf(stderr, "  -h\t print this help message\n");
	fprintf(stderr, "  -V\t print version number and exit\n");
}

int main(int argc, char **argv)
{
	char verbosity = 0;
	char *mphf_file = NULL;
	const char *keys_file = NULL;
	FILE *mphf_fd = stdout;
	FILE *keys_fd;
	cmph_uint32 nkeys = UINT_MAX;
	cmph_t *mphf = NULL;
	cmph_config_t *config;
	cmph_io_adapter_t *source;

	while (1)
	{
		char ch = (char)getopt(argc, argv, "hVvk:m:");
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
				mphf_file = strdup(optarg);
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
		mphf_file = (char *)malloc(strlen(keys_file) + 5);
		memcpy(mphf_file, keys_file, strlen(keys_file));
		memcpy(mphf_file + strlen(keys_file), ".mph\0", (size_t)5);
	}

	keys_fd = fopen(keys_file, "r");
	if (keys_fd == NULL)
	{
		fprintf(stderr, "Unable to open file %s: %s\n", keys_file, strerror(errno));
		return -1;
	}

	if (nkeys == UINT_MAX)
		source = cmph_io_nlfile_adapter(keys_fd);
	else
		source = cmph_io_nlnkfile_adapter(keys_fd, nkeys);

	config = cmph_config_new(source);
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
		free(mphf_file);
		free(source);
		return -1;
	}
	cmph_uint32 sz = cmph_size(mphf);
	cmph_uint32 *o = (cmph_uint32*)malloc(sz * sizeof(cmph_uint32));
	cmph_uint32 *n = (cmph_uint32*)calloc(sz, sizeof(cmph_uint32));
	memset(o, 0xFF, sz * sizeof(cmph_uint32));
	// check all keys
	for (cmph_uint32 i = 0; i < source->nkeys; ++i)
	{
		cmph_uint32 h;
		char *buf;
		cmph_uint32 buflen = 0;
		source->read(source->data, &buf, &buflen);
		h = cmph_search(mphf, buf, buflen);
		n[i] = h;
		o[h] = i;
		if (verbosity) {
			if (i % 16 == 15)
				printf("%u,\n", h);
			else
				printf("%u, ", h);
		}
		source->dispose(buf);
	}
	printf("\nOrdering:\n        ");
	cmph_uint32 min16 = MIN(16, sz);
	for (cmph_uint32 i = 0; i < min16; ++i) {
		printf("%3u ", i);
	}
	if (min16 < sz) printf("...");
	printf("\n        ");
	for (cmph_uint32 i = 0; i < min16; ++i) {
		printf("----");
	}
	printf("\n        ");
	cmph_uint32 min_n = MIN(16, source->nkeys);
	for (cmph_uint32 i = 0; i < min_n; ++i) {
		printf("%3u ", n[i]);
	}
	if (min_n < source->nkeys) printf("...");
	printf("\n");
	for (cmph_uint32 i = 0; i < sz; ++i) {
		if (i % 16 == 0)
			printf("/*%2u*/\t", i);
		if (i % 16 == 15)
			printf("%3u\n", o[i]);
		else
			printf("%3d ", (int32_t)o[i]);
	}
	printf("\n");

	cmph_destroy(mphf);
	free(n);
	free(o);

	fclose(keys_fd);
	free(mphf_file);
	free(source);
	return ret;
}
