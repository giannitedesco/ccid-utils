/*
 * This file is part of ccid-utils
 * Copyright (c) 2009 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/

#ifndef _GANG_HEADER_INCLUDED
#define _GANG_HEADER_INCLDUED

#define GANG_DEFAULT_ALLOC	0x1000U
#define GANG_POISON 		1
#define GANG_POISON_PATTERN 	0xa5

typedef struct _gang *gang_t;

_private gang_t gang_new(size_t alloc, size_t align);
_private void *gang_alloc(gang_t g, size_t sz) _malloc;
_private void *gang_alloc_a(gang_t g, size_t sz, size_t align) _malloc;
_private void *gang_alloc0(gang_t g, size_t sz) _malloc;
_private void *gang_alloc0_a(gang_t g, size_t sz, size_t align) _malloc;
_private void gang_free(gang_t g);

#endif /* _GANG_HEADER_INCLUDED */
