#include "cmph_io_adapter.h"
//
//#define DEBUG
#include "debug.h"

typedef struct 
{
	FILE *fd;
	cmph_uint32 pos;
	cmph_uint16 buffer_fill;
	cmph_uint16 buffer_pos;
	char buffer[BUFSIZ];
} nlfile_fd_t; 

int nlfile_read(void *data, char **key, cmph_uint32 *keylen)
{
	int c = 0;
	nlfile_fd_t *nlfile_fd = (nlfile_fd_t *)data;
	*key = NULL;
	*keylen = 0;
	DEBUGP("Reading next key at pos %u\n", nlfile_fd->pos);
	while (1)
	{
		if (nlfile_fd->buffer_fill == 0)
		{
			DEBUGP("Reading 4k bytes\n");
			c = fread(nlfile_fd->buffer, 1, BUFSIZ, nlfile_fd->fd);
			if (c < 0) return -1;
			nlfile_fd->buffer_fill = c;
			nlfile_fd->buffer_pos = 0;
			if (c == 0 && feof(nlfile_fd->fd)) break;
			if (c == 0) return -1;
		}
		DEBUGP("Looking for newline at offset %hu with %hu bytes remaining\n", nlfile_fd->buffer_pos, nlfile_fd->buffer_fill);
		cmph_uint32 len = 0;
		char *ptr = strchr(nlfile_fd->buffer + nlfile_fd->buffer_pos, '\n');
		if (ptr == NULL) ptr = nlfile_fd->buffer + BUFSIZ;
		len = ptr - (nlfile_fd->buffer + nlfile_fd->buffer_pos);
		DEBUGP("Got %u more bytes\n", len);
		*key = (char *)realloc(*key, len + (*keylen));
		memcpy(*key + *keylen, nlfile_fd->buffer + nlfile_fd->buffer_pos, len);
		*keylen += len;
		//DEBUGP("Got %u bytes for key |%s|\n", *keylen, *key);
		nlfile_fd->buffer_fill -= len;
		nlfile_fd->buffer_pos += len;
		if (nlfile_fd->buffer_fill)
		{
			if (nlfile_fd->buffer[nlfile_fd->buffer_pos] == '\n')
			{
				++(nlfile_fd->buffer_pos);
				--(nlfile_fd->buffer_fill);
				break;
			}
		}
	}
	++(nlfile_fd->pos);
	return *keylen;
}

void nlfile_dispose(void *data, char *key, cmph_uint32 keylen)
{
	DEBUGP("Freeing key %s\n", key);
	free(key);
}
void nlfile_rewind(void *data)
{
	nlfile_fd_t *nlfile_fd = (nlfile_fd_t *)data;
	rewind(nlfile_fd->fd);
}

cmph_io_adapter_t *cmph_io_nlfile_adapter(FILE * keys_fd, cmph_uint32 maxkeys)
{
	nlfile_fd_t *nlfile_fd = (nlfile_fd_t *)malloc(sizeof(nlfile_fd_t));
	cmph_io_adapter_t *adapter = (cmph_io_adapter_t *)malloc(sizeof(cmph_io_adapter_t));
	if ((!adapter) || (!nlfile_fd)) return NULL;

	nlfile_fd->fd = keys_fd;
	nlfile_fd->pos = 0;
	adapter->data = nlfile_fd;
	rewind(nlfile_fd->fd);
	memset(nlfile_fd->buffer, 0, BUFSIZ);
	adapter->nkeys = 0;
	DEBUGP("Counting number of keys in the input\n");
	while (1)
	{
		int longbreak = 0;
		int c = fread(nlfile_fd->buffer, 1, BUFSIZ - 1, nlfile_fd->fd);
		if (c >= 0) nlfile_fd->buffer[c] = 0;
		if (c < 0) 
		{
			free(nlfile_fd);
			free(adapter);
			return NULL;
		}
		char *nl = nlfile_fd->buffer;
		while (1)
		{
			nl = strchr(nl, '\n');
			if (nl != NULL) 
			{
				++(adapter->nkeys);
				++nl;
			}
			else break;
		}
		if (feof(nlfile_fd->fd))
		{
			if (nlfile_fd->buffer[c - 1] != '\n') ++(adapter->nkeys);
			break;
		}
	}
	DEBUGP("Found %u keys\n", adapter->nkeys);
	rewind(nlfile_fd->fd);
	nlfile_fd->buffer_pos = 0;
	nlfile_fd->buffer_fill = 0;

	adapter->read = nlfile_read;
	adapter->dispose = nlfile_dispose;
	adapter->rewind = nlfile_rewind;
	adapter->hash = NULL;
	adapter->next = NULL;
	if (adapter->nkeys > maxkeys) adapter->nkeys = maxkeys;
	return adapter;
}
void cmph_io_nlfile_adapter_destroy(cmph_io_adapter_t *adapter)
{
	free(adapter->data);
	free(adapter);
}

typedef struct
{
	const char * const *keys;
	cmph_uint32 pos;
} vec_t;

int vector_read(void *data, char **key, cmph_uint32 *keylen)
{
	vec_t *vec = (vec_t *)data;
	*key = (char *)vec->keys[vec->pos]; //FIXME: change all signatures to be const-correct
	*keylen = strlen(*key);
	++(vec->pos);
	return *keylen;
}
void vector_dispose(void *data, char *key, cmph_uint32 keylen)
{
	return;
}
void vector_rewind(void *data)
{
	vec_t *vec = (vec_t *)data;
	vec->pos = 0;
}

cmph_io_adapter_t *cmph_io_vector_adapter(char const * const *vec, cmph_uint32 nkeys)
{
	vec_t *vector = (vec_t *)malloc(sizeof(vec_t));
	cmph_io_adapter_t *adapter = (cmph_io_adapter_t *)malloc(sizeof(cmph_io_adapter_t));
	if ((!adapter) || (!vector)) return NULL;
	vector->keys = vec;
	vector->pos = 0;
	adapter->data = vector;
	adapter->nkeys = nkeys;
	adapter->read = vector_read;
	adapter->dispose = vector_dispose;
	adapter->rewind = vector_rewind;
	adapter->hash = NULL;
	adapter->next = NULL;
	return adapter;
}
void cmph_io_vector_adapter_destroy(cmph_io_adapter_t *adapter)
{
	free(adapter->data);
	free(adapter);
}
