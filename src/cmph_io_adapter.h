#ifndef __CMPH_IO_ADAPTER_H__
#define __CMPH_IO_ADAPTER_H__

#include <stdio.h>
#include "cmph_types.h"
#include "hash.h"

typedef struct 
{
	void *data;
	/** Total number of keys in the input */
	cmph_uint32 nkeys;
	/** Read current key in the input and advance to next position
	 * \param data user defined parameter
	 * \param key pointer to store the retrieved key
	 * \param keylen pointer to store the length of the retrieved key
	 * \return length of key (in bytes) on success, -1 otherwise
	 */
	int (*read)(void *, char **, cmph_uint32 *);
	/** Release resources associated with the last key read
	 * \param data user defined parameter
	 * \param key last retrieved key
	 * \param keylen length of the last retrieved key
	 */
	void (*dispose)(void *, char *, cmph_uint32);
	/** Reset key stream to initial position
	 * \param data user defined parameter
	 */
	void (*rewind)(void *);
	/** Find hval of current key and advance stream position
	 * \return 0 on failure, 1 otherwise
	 */
	int (*hash)(void *, cmph_hashfunc_t *hashfunc, cmph_uint32 seed, cmph_uint32 *hval);
} cmph_io_adapter_t;

/** Adapter pattern API **/
/* please call free() in the created adapters */
cmph_io_adapter_t *cmph_io_nlfile_adapter(FILE * keys_fd);
cmph_io_adapter_t *cmph_io_nlnkfile_adapter(FILE * keys_fd, cmph_uint32 nkeys);
//cmph_io_adapter_t *cmph_io_vector_adapter(char ** vector, cmph_uint32 nkeys);

#endif
