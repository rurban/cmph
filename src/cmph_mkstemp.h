#ifndef __CMPH_MKSTEMP_H__
#define __CMPH_MKSTEMP_H__

/** Portable mkstemp implementation */
int cmph_mkstemp(char *tmpl);
/** Return OS-dependent tmp dir */
const char *cmph_get_tmp_dir();

#endif
