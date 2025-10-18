#ifndef __CMPH_XHELPERS_H__
#define __CMPH_XHELPERS_H__

#include <stdlib.h>

/* stdlib helpers which abort on ENOMEM */
#if defined(__GLIBC__) && defined(_SYS_CDEFS_H)

/* Allocate `n` bytes of memory.  */
void *xmalloc(size_t n)
    __THROW __attribute_malloc__ __attribute_alloc_size__ ((1)) __wur;
/* Allocate `nmemb` elements of `size` bytes each, all initialized to 0.  */
void *xcalloc(size_t nmemb, size_t size)
    __THROW __attribute_malloc__ __attribute_alloc_size__ ((1, 2)) __wur;
/* Reallocate to `size` bytes.  */
void *xrealloc (void *ptr, size_t size)
     __THROW __attribute_warn_unused_result__ __attribute_alloc_size__ ((2));
/* Duplicate `s`, returning an identical malloc'd string.  */
void *xstrdup(char *s)
     __THROW __attribute_malloc__ __nonnull ((1));

#else
void *xmalloc(size_t n);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);
void *xstrdup(char *s);
#endif

#endif
