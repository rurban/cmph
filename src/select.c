#include<stdlib.h>
#include<stdio.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include "select_lookup_tables.h"
#include "select.h"
#include "compile.h"

//#define DEBUG
#include "debug.h"

#ifndef STEP_SELECT_TABLE
#define STEP_SELECT_TABLE 128
#endif

#ifndef NBITS_STEP_SELECT_TABLE
#define NBITS_STEP_SELECT_TABLE 7
#endif

#ifndef MASK_STEP_SELECT_TABLE
#define MASK_STEP_SELECT_TABLE 0x7f // 0x7f = 127
#endif

static inline void select_insert_0(cmph_uint32 * buffer)
{
	(*buffer) >>= 1;
};

static inline void select_insert_1(cmph_uint32 * buffer)
{
	(*buffer) >>= 1;
	(*buffer) |= 0x80000000;
};

void select_init(select_t * sel)
{
	sel->n = 0;
	sel->m = 0;
	sel->bits_vec = 0;
	sel->select_table = 0;
};

cmph_uint32 select_get_space_usage(select_t * sel)
{
	register cmph_uint32 nbits;
	register cmph_uint32 vec_size;
	register cmph_uint32 sel_table_size;
	register cmph_uint32 space_usage;
	
	nbits = sel->n + sel->m;
	vec_size = (nbits + 31) >> 5;
	sel_table_size = (sel->n >> NBITS_STEP_SELECT_TABLE) + 1; // (sel->n >> NBITS_STEP_SELECT_TABLE) = (sel->n/STEP_SELECT_TABLE)

	space_usage = 2 * sizeof(cmph_uint32) * 8; // n and m
	space_usage += vec_size * (cmph_uint32) sizeof(cmph_uint32) * 8;
	space_usage += sel_table_size * (cmph_uint32)sizeof(cmph_uint32) * 8;
	return space_usage;
}

void select_destroy(select_t * sel)
{
	free(sel->bits_vec);
	free(sel->select_table);
	sel->bits_vec = 0;
	sel->select_table = 0;
};

static inline void select_generate_sel_table(select_t * sel)
{
	register cmph_uint8 * bits_table = (cmph_uint8 *)sel->bits_vec;
	register cmph_uint32 part_sum, old_part_sum;
	register cmph_uint32 vec_idx, one_idx, sel_table_idx;
	
	part_sum = vec_idx = one_idx = sel_table_idx = 0;
	
	for(;;)
	{
	        // FABIANO: Should'n it be one_idx >= sel->n
		if(one_idx >= sel->n)
			break;
		do 
		{
			old_part_sum = part_sum; 
			part_sum += rank_lookup_table[bits_table[vec_idx]];
			vec_idx++;
		} while (part_sum <= one_idx);
		
		sel->select_table[sel_table_idx] = select_lookup_table[bits_table[vec_idx - 1]][one_idx - old_part_sum] + ((vec_idx - 1) << 3); // ((vec_idx - 1) << 3) = ((vec_idx - 1) * 8)
		one_idx += STEP_SELECT_TABLE ;
		sel_table_idx++;
	};
};

void select_generate(select_t * sel, cmph_uint32 * keys_vec, cmph_uint32 n, cmph_uint32 m)
{
	register cmph_uint32 i, j, idx;
	cmph_uint32 buffer = 0;
	
	register cmph_uint32 nbits;
	register cmph_uint32 vec_size;
	register cmph_uint32 sel_table_size;
	sel->n = n;
	sel->m = m; // n values in the range [0,m-1]
	
	nbits = sel->n + sel->m; 
	vec_size = (nbits + 31) >> 5; // (nbits + 31) >> 5 = (nbits + 31)/32
	
	sel_table_size = (sel->n >> NBITS_STEP_SELECT_TABLE) + 1; // (sel->n >> NBITS_STEP_SELECT_TABLE) = (sel->n/STEP_SELECT_TABLE)
	
	if(sel->bits_vec)
	{
		free(sel->bits_vec);
	}
	sel->bits_vec = (cmph_uint32 *)calloc(vec_size, sizeof(cmph_uint32));

	if(sel->select_table)
	{
		free(sel->select_table);
	}
	sel->select_table = (cmph_uint32 *)calloc(sel_table_size, sizeof(cmph_uint32));

	
	
	idx = i = j = 0;
	
	for(;;)
	{
		while(keys_vec[j]==i)
		{
			select_insert_1(&buffer);
			idx++;
			
			if((idx & 0x1f) == 0 ) // (idx & 0x1f) = idx % 32
				sel->bits_vec[(idx >> 5) - 1] = buffer; // (idx >> 5) = idx/32
			j++;
			
			if(j == sel->n)
				goto loop_end;
				
			//assert(keys_vec[j] < keys_vec[j-1]);
		}
		
		if(i == sel->m)
			break;
			
		while(keys_vec[j] > i)
		{
			select_insert_0(&buffer);
			idx++;
			
			if((idx & 0x1f) == 0 ) // (idx & 0x1f) = idx % 32
				sel->bits_vec[(idx >> 5) - 1] = buffer; // (idx >> 5) = idx/32
			i++;
		};
		
	};
	loop_end:
	if((idx & 0x1f) != 0 ) // (idx & 0x1f) = idx % 32
	{
		buffer >>= 32 - (idx & 0x1f);
		sel->bits_vec[ (idx - 1) >> 5 ] = buffer;
	};
	
	select_generate_sel_table(sel);
};

static inline cmph_uint32 _select_query(cmph_uint8 * bits_table, cmph_uint32 * select_table, cmph_uint32 one_idx)
{
	register cmph_uint32 vec_bit_idx ,vec_byte_idx;
	register cmph_uint32 part_sum, old_part_sum;
	
	vec_bit_idx = select_table[one_idx >> NBITS_STEP_SELECT_TABLE]; // one_idx >> NBITS_STEP_SELECT_TABLE = one_idx/STEP_SELECT_TABLE
	vec_byte_idx = vec_bit_idx >> 3; // vec_bit_idx / 8
	
	one_idx &= MASK_STEP_SELECT_TABLE; // one_idx %= STEP_SELECT_TABLE == one_idx &= MASK_STEP_SELECT_TABLE
	one_idx += rank_lookup_table[bits_table[vec_byte_idx] & ((1 << (vec_bit_idx & 0x7)) - 1)];
	part_sum = 0;
	
	do
	{
		old_part_sum = part_sum; 
		part_sum += rank_lookup_table[bits_table[vec_byte_idx]];
		vec_byte_idx++;
		
	}while (part_sum <= one_idx);
	
	return select_lookup_table[bits_table[vec_byte_idx - 1]][one_idx - old_part_sum] + ((vec_byte_idx-1) << 3);
}

cmph_uint32 select_query(select_t * sel, cmph_uint32 one_idx)
{
	return _select_query((cmph_uint8 *)sel->bits_vec, sel->select_table, one_idx);
};

static inline cmph_uint32 _select_next_query(cmph_uint8 * bits_table, cmph_uint32 vec_bit_idx)
{
	register cmph_uint32 vec_byte_idx, one_idx;
	register cmph_uint32 part_sum, old_part_sum;
	
	vec_byte_idx = vec_bit_idx >> 3;
	
	one_idx = rank_lookup_table[bits_table[vec_byte_idx] & ((1U << (vec_bit_idx & 0x7)) - 1U)] + 1U;
	part_sum = 0;
	
	do
	{
		old_part_sum = part_sum; 
		part_sum += rank_lookup_table[bits_table[vec_byte_idx]];
		vec_byte_idx++;
		
	}while (part_sum <= one_idx);
	
	return select_lookup_table[bits_table[(vec_byte_idx - 1)]][(one_idx - old_part_sum)] + ((vec_byte_idx - 1) << 3);
}

cmph_uint32 select_next_query(select_t * sel, cmph_uint32 vec_bit_idx)
{
	return _select_next_query((cmph_uint8 *)sel->bits_vec, vec_bit_idx);
};

void select_dump(select_t *sel, char **buf, cmph_uint32 *buflen)
{
        register cmph_uint32 nbits = sel->n + sel->m;
	register cmph_uint32 vec_size = ((nbits + 31) >> 5) * (cmph_uint32)sizeof(cmph_uint32); // (nbits + 31) >> 5 = (nbits + 31)/32
	register cmph_uint32 sel_table_size = ((sel->n >> NBITS_STEP_SELECT_TABLE) + 1) * (cmph_uint32)sizeof(cmph_uint32); // (sel->n >> NBITS_STEP_SELECT_TABLE) = (sel->n/STEP_SELECT_TABLE)
	register cmph_uint32 pos = 0;
	
	*buflen = 2*(cmph_uint32)sizeof(cmph_uint32) + vec_size + sel_table_size;
	
	*buf = (char *)calloc(*buflen, sizeof(char));
	
	if (!*buf) 
	{
		*buflen = UINT_MAX;
		return;
	}
	
	memcpy(*buf, &(sel->n), sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	memcpy(*buf + pos, &(sel->m), sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	memcpy(*buf + pos, sel->bits_vec, vec_size);
	pos += vec_size;
	memcpy(*buf + pos, sel->select_table, sel_table_size);

	DEBUGP("Dumped select structure with size %u bytes\n", *buflen);
}

void select_compile(FILE* out, const char *name, select_t *sel)
{
        cmph_uint32 nbits = sel->n + sel->m;
	cmph_uint32 vec_size = ((nbits + 31) >> 5);
	cmph_uint32 sel_table_size = ((sel->n >> NBITS_STEP_SELECT_TABLE) + 1);

	uint32_compile(out, "bits_vec", sel->bits_vec, vec_size);
	uint32_compile(out, "select_table", sel->select_table, sel_table_size);
	fprintf(out, "struct _select_t {\n"
		"    uint32_t n, m;\n"
		"    const uint32_t *bits_vec;\n"
		"    const uint32_t *select_table;\n"
		"};\n"
		"typedef struct _select_t select_t;\n");
	fprintf(out, "const select_t %s = {\n", name);
	fprintf(out, "    %u, /* n */\n", sel->n);
	fprintf(out, "    %u, /* m */\n", sel->m);
	fprintf(out, "    bits_vec,\n");
	fprintf(out, "    select_table};\n");
}

// no select_query_packed
void select_query_compile(FILE* out)
{
  bytes_compile(out, "rank_lookup_table", rank_lookup_table, 256);
  bytes_2_compile(out, "select_lookup_table", (uint8_t *)select_lookup_table, 256, 8);
  fprintf(out, "\n"
	  "#define BITS_TABLE_SIZE(n, bits_length) ((n * bits_length + 31) >> 5)\n"
	  "#define NBITS_STEP_SELECT_TABLE %u\n", NBITS_STEP_SELECT_TABLE);
  fprintf(out, "#define MASK_STEP_SELECT_TABLE 0x%x // 0x7f = 127\n\n", MASK_STEP_SELECT_TABLE);
  fprintf(out,
      "static inline uint32_t _select_query(const uint8_t *bits_table,\n"
      "                                     const uint32_t *select_table,\n"
      "                                     uint32_t one_idx)\n"
      "{\n"
      "    uint32_t vec_bit_idx ,vec_byte_idx;\n"
      "    uint32_t part_sum, old_part_sum;\n"
      "\n"
      "    vec_bit_idx = select_table[one_idx >> NBITS_STEP_SELECT_TABLE];\n"
      "    vec_byte_idx = vec_bit_idx >> 3; // vec_bit_idx / 8\n"
      "\n"
      "    one_idx &= MASK_STEP_SELECT_TABLE;\n"
      "    one_idx += rank_lookup_table[bits_table[vec_byte_idx] &\n"
      "                   ((1 << (vec_bit_idx & 0x7)) - 1)];\n"
      "    part_sum = 0;\n"
      "    do {\n"
      "        old_part_sum = part_sum; \n"
      "        part_sum += rank_lookup_table[bits_table[vec_byte_idx]];\n"
      "        vec_byte_idx++;\n"
      "    } while (part_sum <= one_idx);\n"
      "\n"
      "    return select_lookup_table[bits_table[vec_byte_idx - 1]]\n"
      "               [one_idx - old_part_sum] + ((vec_byte_idx-1) << 3);\n"
      "}\n"
      "static inline uint32_t get_bits_value(const uint32_t *bits_table, uint32_t index,\n"
      "                                      uint32_t string_length, uint32_t string_mask)\n"
      "{\n"
      "    uint32_t bit_idx = index * string_length;\n"
      "    uint32_t word_idx = bit_idx >> 5;\n"
      "    uint32_t shift1 = bit_idx & 0x0000001f;\n"
      "    uint32_t shift2 = 32 - shift1;\n"
      "    uint32_t bits_string;\n"
      "\n"
      "    bits_string = (bits_table[word_idx] >> shift1) & string_mask;\n"
      "\n"
      "    if(shift2 < string_length)\n"
      "        bits_string |= (bits_table[word_idx+1] << shift2) & string_mask;\n"
      "\n"
      "    return bits_string;\n"
      "};\n"
      "static inline uint32_t _select_next_query(const uint8_t *bits_table, uint32_t vec_bit_idx) {\n"
      "    uint32_t vec_byte_idx, one_idx;\n"
      "    uint32_t part_sum, old_part_sum;\n"
      "    vec_byte_idx = vec_bit_idx >> 3;\n"
      "    one_idx = rank_lookup_table[bits_table[vec_byte_idx] &\n"
      "                  ((1U << (vec_bit_idx & 0x7)) - 1U)] + 1U;\n"
      "    part_sum = 0;\n"
      "    do {\n"
      "    	old_part_sum = part_sum; \n"
      "    	part_sum += rank_lookup_table[bits_table[vec_byte_idx]];\n"
      "    	vec_byte_idx++;\n"
      "    } while (part_sum <= one_idx);	\n"
      "\n"
      "    return select_lookup_table[bits_table[(vec_byte_idx - 1)]]\n"
      "               [(one_idx - old_part_sum)] + ((vec_byte_idx - 1) << 3);\n"
      "}\n"
      "static inline uint32_t select_query(const select_t *sel, uint32_t one_idx) {\n"
      "    return _select_query((uint8_t *)sel->bits_vec, sel->select_table, one_idx);\n"
      "};\n"
      "static inline uint32_t select_next_query(const select_t *sel, uint32_t vec_bit_idx) {\n"
      "    return _select_next_query((const uint8_t *)sel->bits_vec, vec_bit_idx);\n"
      "};\n");
}

#if 0
void select_query_packed_compile(FILE* out)
{
  fprintf(out, "#define BITS_TABLE_SIZE(n, bits_length) ((n * bits_length + "
               "31) >> 5)\n");
  fprintf(out,
      "static inline uint32_t select_query(select_t *sel, uint32_t one_idx) {\n"
      "    return _select_query((uint8_t *)sel->bits_vec, sel->select_table, one_idx);\n"
      "};\n"
      "static inline uint32_t _select_query(const uint8_t *bits_table,\n"
      "                                     const uint32_t *select_table,\n"
      "                                     uint32_t one_idx)\n"
      "{\n"
      "    uint32_t vec_bit_idx ,vec_byte_idx;\n"
      "    uint32_t part_sum, old_part_sum;\n"
      "\n"
      "    vec_bit_idx = select_table[one_idx >> NBITS_STEP_SELECT_TABLE];\n"
      "    vec_byte_idx = vec_bit_idx >> 3; // vec_bit_idx / 8\n"
      "\n"
      "    one_idx &= MASK_STEP_SELECT_TABLE;\n"
      "    one_idx += rank_lookup_table[bits_table[vec_byte_idx] & ((1 << "
      "(vec_bit_idx & 0x7)) - 1)];\n"
      "    part_sum = 0;	\n"
      "    do {\n"
      "        old_part_sum = part_sum; \n"
      "        part_sum += rank_lookup_table[bits_table[vec_byte_idx]];\n"
      "        vec_byte_idx++;\n"
      "    } while (part_sum <= one_idx);\n"
      "    return select_lookup_table[bits_table[vec_byte_idx - 1]][one_idx - "
      "old_part_sum] + ((vec_byte_idx-1) << 3);\n"
      "}\n"
      "static uint32_t select_query_packed(void * sel_packed, uint32_t one_idx)\n"
      "{\n"
      "    uint32_t *ptr = (uint32_t *)sel_packed;\n"
      "    uint32_t n = *ptr++;\n"
      "    uint32_t m = *ptr++;\n"
      "    uint32_t nbits = n + m;\n"
      "    uint32_t vec_size = (nbits + 31) >> 5;\n"
      "    uint8_t * bits_vec = (uint8_t *)ptr;\n"
      "    uint32_t * select_table = ptr + vec_size;\n"
      "\n"
      "    return _select_query(bits_vec, select_table, one_idx);\n"
      "}\n"
      "static inline uint32_t get_bits_value(const uint32_t * bits_table, uint32_t index,\n"
      "                                      uint32_t string_length, uint32_t string_mask)\n"
      "{\n"
      "    uint32_t bit_idx = index * string_length;\n"
      "    uint32_t word_idx = bit_idx >> 5;\n"
      "    uint32_t shift1 = bit_idx & 0x0000001f;\n"
      "    uint32_t shift2 = 32 - shift1;\n"
      "    uint32_t bits_string;\n"
      "\n"
      "    bits_string = (bits_table[word_idx] >> shift1) & string_mask;\n"
      "\n"
      "    if(shift2 < string_length)\n"
      "        bits_string |= (bits_table[word_idx+1] << shift2) & string_mask;\n"
      "\n"
      "    return bits_string;\n"
      "};\n"
      "static inline uint32_t _select_next_query(uint8_t *bits_table, uint32_t vec_bit_idx) {\n"
      "    uint32_t vec_byte_idx, one_idx;\n"
      "    uint32_t part_sum, old_part_sum;\n"
      "    vec_byte_idx = vec_bit_idx >> 3;\n"
      "    one_idx = rank_lookup_table[bits_table[vec_byte_idx] &\n"
      "    	         ((1U << (vec_bit_idx & 0x7)) - 1U)] + 1U;\n"
      "    part_sum = 0;\n"
      "    do {\n"
      "        old_part_sum = part_sum; \n"
      "        part_sum += rank_lookup_table[bits_table[vec_byte_idx]];\n"
      "        vec_byte_idx++;\n"
      "    } while (part_sum <= one_idx);	\n"
      "    return select_lookup_table[bits_table[(vec_byte_idx - 1)]][(one_idx - old_part_sum)]\n"
      "    	+ ((vec_byte_idx - 1) << 3);\n"
      "}\n"
      "static uint32_t select_next_query_packed(void *sel_packed, uint32_t vec_bit_idx)\n"
      "{\n"
      "    uint8_t * bits_vec = (uint8_t *)sel_packed;\n"
      "    bits_vec += 8; // skipping n and m\n"
      "    return _select_next_query(bits_vec, vec_bit_idx);\n"
      "}\n"
      "static inline uint32_t get_bits_value(const uint32_t * bits_table, uint32_t index,\n"
      "                                      uint32_t string_length, uint32_t string_mask)\n"
      "{\n"
      "    uint32_t bit_idx = index * string_length;\n"
      "    uint32_t word_idx = bit_idx >> 5;\n"
      "    uint32_t shift1 = bit_idx & 0x0000001f;\n"
      "    uint32_t shift2 = 32-shift1;\n"
      "    uint32_t bits_string;\n"
      "    bits_string = (bits_table[word_idx] >> shift1) & string_mask;\n"
      "    if(shift2 < string_length)\n"
      "    	bits_string |= (bits_table[word_idx+1] << shift2) & string_mask;\n"
      "    return bits_string;\n"
      "};\n");
}
#endif

void select_load(select_t * sel, const char *buf)
{
	register cmph_uint32 pos = 0;
        register cmph_uint32 nbits = 0;
	register cmph_uint32 vec_size = 0;
	register cmph_uint32 sel_table_size = 0;
	
	memcpy(&(sel->n), buf, sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	memcpy(&(sel->m), buf + pos, sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	
	nbits = sel->n + sel->m;
	vec_size = ((nbits + 31) >> 5) * (cmph_uint32)sizeof(cmph_uint32); // (nbits + 31) >> 5 = (nbits + 31)/32
	sel_table_size = ((sel->n >> NBITS_STEP_SELECT_TABLE) + 1) * (cmph_uint32)sizeof(cmph_uint32); // (sel->n >> NBITS_STEP_SELECT_TABLE) = (sel->n/STEP_SELECT_TABLE)
	
	if(sel->bits_vec) 
	{
		free(sel->bits_vec);
	}
	sel->bits_vec = (cmph_uint32 *)calloc(vec_size/sizeof(cmph_uint32), sizeof(cmph_uint32));

	if(sel->select_table) 
	{
		free(sel->select_table);
	}
	sel->select_table = (cmph_uint32 *)calloc(sel_table_size/sizeof(cmph_uint32), sizeof(cmph_uint32));

	memcpy(sel->bits_vec, buf + pos, vec_size);
	pos += vec_size;
	memcpy(sel->select_table, buf + pos, sel_table_size);
	
	DEBUGP("Loaded select structure with size %u bytes\n", 2*(cmph_uint32)sizeof(cmph_uint32) + vec_size + sel_table_size);
}


/** \fn void select_pack(select_t *sel, void *sel_packed);
 *  \brief Support the ability to pack a select structure function into a preallocated contiguous memory space pointed by sel_packed.
 *  \param sel points to the select structure
 *  \param sel_packed pointer to the contiguous memory area used to store the select structure. The size of sel_packed must be at least @see select_packed_size 
 */
void select_pack(select_t *sel, void *sel_packed)
{
	if (sel && sel_packed)
	{
		char *buf = NULL;
		cmph_uint32 buflen = 0;
		select_dump(sel, &buf, &buflen);
		memcpy(sel_packed, buf, buflen);
		free(buf);
	}
}


/** \fn cmph_uint32 select_packed_size(select_t *sel);
 *  \brief Return the amount of space needed to pack a select structure.
 *  \return the size of the packed select structure or zero for failures
 */ 
cmph_uint32 select_packed_size(select_t *sel)
{
        register cmph_uint32 nbits = sel->n + sel->m;
	register cmph_uint32 vec_size = ((nbits + 31) >> 5) * (cmph_uint32)sizeof(cmph_uint32); // (nbits + 31) >> 5 = (nbits + 31)/32
	register cmph_uint32 sel_table_size = ((sel->n >> NBITS_STEP_SELECT_TABLE) + 1) * (cmph_uint32)sizeof(cmph_uint32); // (sel->n >> NBITS_STEP_SELECT_TABLE) = (sel->n/STEP_SELECT_TABLE)
	return 2*(cmph_uint32)sizeof(cmph_uint32) + vec_size + sel_table_size;
}



cmph_uint32 select_query_packed(void * sel_packed, cmph_uint32 one_idx)
{
	register cmph_uint32 *ptr = (cmph_uint32 *)sel_packed;
	register cmph_uint32 n = *ptr++;
	register cmph_uint32 m = *ptr++;
        register cmph_uint32 nbits = n + m;
	register cmph_uint32 vec_size = (nbits + 31) >> 5; // (nbits + 31) >> 5 = (nbits + 31)/32
	register cmph_uint8 * bits_vec = (cmph_uint8 *)ptr;
	register cmph_uint32 * select_table = ptr + vec_size;
	
	return _select_query(bits_vec, select_table, one_idx);
}


cmph_uint32 select_next_query_packed(void * sel_packed, cmph_uint32 vec_bit_idx)
{
	register cmph_uint8 * bits_vec = (cmph_uint8 *)sel_packed;
	bits_vec += 8; // skipping n and m
	return _select_next_query(bits_vec, vec_bit_idx);
}
