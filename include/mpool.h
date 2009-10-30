/*
 * This file is part of dotscara
 * Copyright (c) 2003 Gianni Tedesco
 * Released under the terms of the GNU GPL version 2
 */
#ifndef _MPOOL_HEADER_INCLUDED_
#define _MPOOL_HEADER_INCLUDED_

typedef struct _mpool *mpool_t;

#define MPOOL_POISON 		1
#define MPOOL_POISON_PATTERN 	0xa5

_private mpool_t mpool_new(size_t obj_size, unsigned slab_size);
_private void mpool_free(mpool_t m);
_private void * mpool_alloc(mpool_t m) _malloc;
_private void *mpool_alloc0(mpool_t m) _malloc;
_private void mpool_return(mpool_t m, void *obj);

#endif /* _MPOOL_HEADER_INCLUDED_ */
