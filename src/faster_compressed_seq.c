#include "faster_compressed_seq.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <string.h>

#include "bitbool.h"

// #define DEBUG
#include "debug.h"

static inline cmph_uint32 faster_compressed_seq_i_log2(cmph_uint32 x)
{
	register cmph_uint32 res = 0;
	
	while(x > 1)
	{
		x >>= 1;
		res++;
	}
	return res;
};

void faster_compressed_seq_init(faster_compressed_seq_t * cs)
{
	select_init(&cs->sel);
	cs->n = 0;
	cs->rem_r = 0;
	cs->val_rems = 0;
}

void faster_compressed_seq_destroy(faster_compressed_seq_t * cs)
{
	free(cs->val_rems);
	cs->val_rems = 0;
	select_destroy(&cs->sel);
};

void faster_compressed_seq_generate(faster_compressed_seq_t * cs, cmph_uint32 * vals_table, cmph_uint32 n)
{

    register cmph_uint32 i;
    register cmph_uint32 * rem_vals = (cmph_uint32 *)calloc(n, sizeof(cmph_uint32));
    register cmph_uint32 rems_mask;
    register cmph_uint32 sum_of_values = 0;

    cs->n = n;
    for(i = 0; i < cs->n; i++)
    {
        sum_of_values += vals_table[i];
    }

    cs->rem_r = faster_compressed_seq_i_log2(sum_of_values/cs->n);

    cs->val_rems = (cmph_uint32 *) calloc(BITS_TABLE_SIZE(cs->n, cs->rem_r), sizeof(cmph_uint32));
    
    rems_mask = (1U << cs->rem_r) - 1U;
    sum_of_values = 0;

    for(i = 0; i < cs->n; i++)
    {
        sum_of_values += vals_table[i];
        set_bits_value(cs->val_rems, i, sum_of_values & rems_mask, cs->rem_r, rems_mask);
        rem_vals[i] = sum_of_values >> cs->rem_r;
    };

    select_destroy(&cs->sel);
    select_init(&cs->sel);

    // FABIANO: before it was (cs->total_length >> cs->rem_r) + 1. But I wiped out the + 1 because
    // I changed the select structure to work up to m, instead of up to m - 1.
    select_generate(&cs->sel, rem_vals, cs->n, (sum_of_values >> cs->rem_r));

    free(rem_vals);
};

cmph_uint32 faster_compressed_seq_get_space_usage(faster_compressed_seq_t * cs)
{
	register cmph_uint32 space_usage = select_get_space_usage(&cs->sel);
	space_usage += BITS_TABLE_SIZE(cs->n, cs->rem_r) * (cmph_uint32)sizeof(cmph_uint32) * 8;
	return  3U * (cmph_uint32)sizeof(cmph_uint32) * 8U + space_usage;
}

cmph_uint32 faster_compressed_seq_query(faster_compressed_seq_t * cs, cmph_uint32 idx)
{
	register cmph_uint32 rems_mask;
	register cmph_uint32 stored_value;
	register cmph_uint32 sel_res;

	assert(idx < cs->n); // FABIANO ADDED

	rems_mask = (1U << cs->rem_r) - 1U;
	
	if(idx == 0)
	{
		stored_value = 0;
                sel_res = select_query(&cs->sel, idx);
	}
	else
	{
		sel_res = select_query(&cs->sel, idx - 1);
		
		stored_value = (sel_res - (idx - 1)) << cs->rem_r;
		stored_value += get_bits_value(cs->val_rems, idx - 1, cs->rem_r, rems_mask);
		
		sel_res = select_next_query(&cs->sel, sel_res);
	};

	stored_value = ((sel_res - idx) << cs->rem_r) - stored_value;
	stored_value += get_bits_value(cs->val_rems, idx, cs->rem_r, rems_mask);

	return stored_value;
};

void faster_compressed_seq_dump(faster_compressed_seq_t * cs, char ** buf, cmph_uint32 * buflen)
{
	register cmph_uint32 sel_size = select_packed_size(&(cs->sel));
	register cmph_uint32 val_rems_size = BITS_TABLE_SIZE(cs->n, cs->rem_r) * (cmph_uint32)sizeof(cmph_uint32);
	register cmph_uint32 pos = 0;
	char * buf_sel = 0;
	cmph_uint32 buflen_sel = 0;
	
	*buflen = 3U*(cmph_uint32)sizeof(cmph_uint32) + sel_size + val_rems_size;
	
	DEBUGP("sel_size = %u\n", sel_size);
	DEBUGP("val_rems_size = %u\n", val_rems_size);
	*buf = (char *)calloc(*buflen, sizeof(char));
	
	if (!*buf) 
	{
		*buflen = UINT_MAX;
		return;
	}
	
	// dumping n, rem_r
	memcpy(*buf, &(cs->n), sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("n = %u\n", cs->n);
	
	memcpy(*buf + pos, &(cs->rem_r), sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("rem_r = %u\n", cs->rem_r);

	// dumping sel
	select_dump(&cs->sel, &buf_sel, &buflen_sel);
	memcpy(*buf + pos, &buflen_sel, sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("buflen_sel = %u\n", buflen_sel);

	memcpy(*buf + pos, buf_sel, buflen_sel);
	#ifdef DEBUG	
	cmph_uint32 i = 0; 
	for(i = 0; i < buflen_sel; i++)
	{
	    DEBUGP("pos = %u  -- buf_sel[%u] = %u\n", pos, i, *(*buf + pos + i));
	}
	#endif
	pos += buflen_sel;
	
	free(buf_sel);
	
	// dumping val_rems
	memcpy(*buf + pos, cs->val_rems, val_rems_size);
	#ifdef DEBUG	
	for(i = 0; i < val_rems_size; i++)
	{
	    DEBUGP("pos = %u -- val_rems_size = %u  -- val_rems[%u] = %u\n", pos, val_rems_size, i, *(*buf + pos + i));
	}
	#endif
	// pos += val_rems_size;

	DEBUGP("Dumped compressed sequence structure with size %u bytes\n", *buflen);
}

void faster_compressed_seq_load(faster_compressed_seq_t * cs, const char * buf, cmph_uint32 buflen)
{
	register cmph_uint32 pos = 0;
	cmph_uint32 buflen_sel = 0;
	register cmph_uint32 val_rems_size = 0;
	
	// loading n, rem_r 
	memcpy(&(cs->n), buf, sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("n = %u\n", cs->n);

	memcpy(&(cs->rem_r), buf + pos, sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("rem_r = %u\n", cs->rem_r);

	// loading sel
	memcpy(&buflen_sel, buf + pos, sizeof(cmph_uint32));
	pos += (cmph_uint32)sizeof(cmph_uint32);
	DEBUGP("buflen_sel = %u\n", buflen_sel);

	select_load(&cs->sel, buf + pos, buflen_sel);
	#ifdef DEBUG	
	cmph_uint32 i = 0;  
	for(i = 0; i < buflen_sel; i++)
	{
	    DEBUGP("pos = %u  -- buf_sel[%u] = %u\n", pos, i, *(buf + pos + i));
	}
	#endif
	pos += buflen_sel;
	
	// loading val_rems
	if(cs->val_rems)
	{
		free(cs->val_rems);
	}
	val_rems_size = BITS_TABLE_SIZE(cs->n, cs->rem_r);
	cs->val_rems = (cmph_uint32 *) calloc(val_rems_size, sizeof(cmph_uint32));
	val_rems_size *= (cmph_uint32)sizeof(cmph_uint32);
	memcpy(cs->val_rems, buf + pos, val_rems_size);
	
	#ifdef DEBUG	
	for(i = 0; i < val_rems_size; i++)
	{
	    DEBUGP("pos = %u -- val_rems_size = %u  -- val_rems[%u] = %u\n", pos, val_rems_size, i, *(buf + pos + i));
	}
	#endif
	// pos += val_rems_size;

	DEBUGP("Loaded compressed sequence structure with size %u bytes\n", buflen);
}

void faster_compressed_seq_pack(faster_compressed_seq_t *cs, void *cs_packed)
{
	if (cs && cs_packed)
	{
		char *buf = NULL;
		cmph_uint32 buflen = 0;
		faster_compressed_seq_dump(cs, &buf, &buflen);
		memcpy(cs_packed, buf, buflen);
		free(buf);
	}

}

cmph_uint32 faster_compressed_seq_packed_size(faster_compressed_seq_t *cs)
{
	register cmph_uint32 sel_size = select_packed_size(&cs->sel);
	register cmph_uint32 val_rems_size = BITS_TABLE_SIZE(cs->n, cs->rem_r) * (cmph_uint32)sizeof(cmph_uint32);
	return 3U * (cmph_uint32)sizeof(cmph_uint32) + sel_size + val_rems_size;
}



cmph_uint32 faster_compressed_seq_query_packed(void * cs_packed, cmph_uint32 idx)
{
	// unpacking cs_packed
	register cmph_uint32 *ptr = (cmph_uint32 *)cs_packed;
        // skipping n
	ptr++;
	register cmph_uint32 rem_r = *ptr++;
	register cmph_uint32 buflen_sel = *ptr++;
	register cmph_uint32 * sel_packed = ptr;
	register cmph_uint32 * val_rems = (ptr += (buflen_sel >> 2)); 
//	register cmph_uint32 val_rems_size = BITS_TABLE_SIZE(n, rem_r);

	// compressed sequence query computation
	register cmph_uint32 rems_mask;
	register cmph_uint32 stored_value;
	register cmph_uint32 sel_res;

	rems_mask = (1U << rem_r) - 1U;
	
	if(idx == 0)
	{
		stored_value = 0;
		sel_res = select_query_packed(sel_packed, idx);
	}
	else
	{
		sel_res = select_query_packed(sel_packed, idx - 1);
		
		stored_value = (sel_res - (idx - 1)) << rem_r;
		stored_value += get_bits_value(val_rems, idx - 1, rem_r, rems_mask);
		
		sel_res = select_next_query_packed(sel_packed, sel_res);
	};

	stored_value = ((sel_res - idx) << rem_r) - stored_value;
	stored_value += get_bits_value(val_rems, idx, rem_r, rems_mask);
	return stored_value; 
}

