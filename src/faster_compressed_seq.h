#ifndef __CMPH_FASTER_COMPRESSED_SEQ_H__
#define __CMPH_FASTER_COMPRESSED_SEQ_H__

#include "select.h"

struct _faster_compressed_seq_t
{
	cmph_uint32 n; // number of values stored in store_table
	// The length in bits of each value is decomposed into two compnents: the lg(n) MSBs are stored in rank_select data structure
	// the remaining LSBs are stored in a table of n cells, each one of rem_r bits.
	cmph_uint32 rem_r;
	select_t sel;
	cmph_uint32 * val_rems;
};

typedef struct _faster_compressed_seq_t faster_compressed_seq_t;

/** \fn void faster_compressed_seq_init(faster_compressed_seq_t * cs);
 *  \brief Initialize a compressed sequence structure.
 *  \param cs points to the compressed sequence structure to be initialized
 */
void faster_compressed_seq_init(faster_compressed_seq_t * cs);

/** \fn void faster_compressed_seq_destroy(faster_compressed_seq_t * cs);
 *  \brief Destroy a compressed sequence given as input.
 *  \param cs points to the compressed sequence structure to be destroyed
 */
void faster_compressed_seq_destroy(faster_compressed_seq_t * cs);

/** \fn void faster_compressed_seq_generate(faster_compressed_seq_t * cs, cmph_uint32 * vals_table, cmph_uint32 n);
 *  \brief Generate a compressed sequence from an input array with n values.
 *  \param cs points to the compressed sequence structure
 *  \param vals_table poiter to the array given as input
 *  \param n number of values in @see vals_table
 */
void faster_compressed_seq_generate(faster_compressed_seq_t * cs, cmph_uint32 * vals_table, cmph_uint32 n);


/** \fn cmph_uint32 faster_compressed_seq_query(faster_compressed_seq_t * cs, cmph_uint32 idx);
 *  \brief Returns the value stored at index @see idx of the compressed sequence structure.
 *  \param cs points to the compressed sequence structure
 *  \param idx index to retrieve the value from
 *  \return the value stored at index @see idx of the compressed sequence structure
 */
cmph_uint32 faster_compressed_seq_query(faster_compressed_seq_t * cs, cmph_uint32 idx);


/** \fn cmph_uint32 faster_compressed_seq_get_space_usage(faster_compressed_seq_t * cs);
 *  \brief Returns amount of space (in bits) to store the compressed sequence.
 *  \param cs points to the compressed sequence structure
 *  \return the amount of space (in bits) to store @see cs
 */
cmph_uint32 faster_compressed_seq_get_space_usage(faster_compressed_seq_t * cs);

void faster_compressed_seq_dump(faster_compressed_seq_t * cs, char ** buf, cmph_uint32 * buflen);

void faster_compressed_seq_load(faster_compressed_seq_t * cs, const char * buf, cmph_uint32 buflen);


/** \fn void faster_compressed_seq_pack(faster_compressed_seq_t *cs, void *cs_packed);
 *  \brief Support the ability to pack a compressed sequence structure into a preallocated contiguous memory space pointed by cs_packed.
 *  \param cs points to the compressed sequence structure
 *  \param cs_packed pointer to the contiguous memory area used to store the compressed sequence structure. The size of cs_packed must be at least @see faster_compressed_seq_packed_size 
 */
void faster_compressed_seq_pack(faster_compressed_seq_t *cs, void *cs_packed);

/** \fn cmph_uint32 faster_compressed_seq_packed_size(faster_compressed_seq_t *cs);
 *  \brief Return the amount of space needed to pack a compressed sequence structure.
 *  \return the size of the packed compressed sequence structure or zero for failures
 */ 
cmph_uint32 faster_compressed_seq_packed_size(faster_compressed_seq_t *cs);


/** \fn cmph_uint32 faster_compressed_seq_query_packed(void * cs_packed, cmph_uint32 idx);
 *  \brief Returns the value stored at index @see idx of the packed compressed sequence structure.
 *  \param cs_packed is a pointer to a contiguous memory area
 *  \param idx is the index to retrieve the value from
 *  \return the value stored at index @see idx of the packed compressed sequence structure
 */
cmph_uint32 faster_compressed_seq_query_packed(void * cs_packed, cmph_uint32 idx);

#endif

