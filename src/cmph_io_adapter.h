#ifndef __CMPH_IO_ADAPTER_H__
#define __CMPH_IO_ADAPTER_H__

#include <stdio.h>
#include "cmph_types.h"

typedef struct 
{
	void *data;
	/** Total number of keys in the input */
	cmph_uint32 nkeys;
	/** Read current key in the input and advance to next position
	 * Optionally you can set this to NULL and implement the hash and next
	 * methods below.
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
	/** Find hval of current key. Do _not_ advance stream position
	 * This should be used only when you cannot implement the read
	 * method in a efficient fashion. If you define read, set this to NULL.
	 * \return 0 on failure, 1 otherwise
	 */
	int (*hash)(void *, cmph_hashfunc_t hashfunc, cmph_uint32 seed, cmph_uint32 *hval);
	/** Advance stream position
	 * This should be used only when you cannot implement the read
	 * method in a efficient fashion. If you define read, set this to NULL.
	 * return 0 on failure, 1 otherwise
	 */
	int (*next)(void *);
} cmph_io_adapter_t;

/** Adapter pattern API **/
cmph_io_adapter_t *cmph_io_nlfile_adapter(FILE * keys_fd, cmph_uint32 maxkeys);
void cmph_io_nlfile_adapter_destroy(cmph_io_adapter_t *adapter);
cmph_io_adapter_t *cmph_io_vector_adapter(char const * const *vec, cmph_uint32 nkeys);
void cmph_io_vector_adapter_destroy(cmph_io_adapter_t *adapter);

#endif
