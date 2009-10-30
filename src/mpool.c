/*
* This file is part of Firestorm NIDS
* Copyright (c) 2003 Gianni Tedesco
* Released under the terms of the GNU GPL version 2
*/

#include <compiler.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <mpool.h>

#if MPOOL_POISON
#define POISON(ptr, len) memset(ptr, MPOOL_POISON_PATTERN, len)
#else
#define POISON(ptr, len) do { } while(0);
#endif

/** mpool descriptor.
 * \ingroup g_mpool
*/
struct _mpool {
	/** Object size. */
	size_t obj_size;
	/** Size of each block including mpool_hdr overhead. */
	size_t slab_size;
	/** List of blocks. */
	struct _mpool_hdr *slabs;
	/** List of free'd objects. */
	void *free;
};

/** mpool memory area descriptor.
 * \ingroup g_mpool
*/
struct _mpool_hdr {
	/** Pointer to next item in the list */
	struct _mpool_hdr *next;
	/** Pointer to next available object */
	void *next_obj;
	/** Data up to the slab_size */
	uint8_t data[0];
};

/** Initialise an mpool.
 * \ingroup g_mpool
 *
 * @param m an mpool structure to use
 * @param obj_size size of objects to allocate
 * @param slab_size size of slabs in number of objects (set to zero for auto)
 *
 * Creates a new empty memory pool descriptor with the passed values
 * set. The resultant mpool has no alignment requirement set.
 *
 * @return zero on error, non-zero for success
 * (may only return 0 if the obj_size is 0).
 */
mpool_t mpool_new(size_t obj_size, unsigned slab_size)
{
	struct _mpool *m;

	/* quick sanity checks */
	if ( obj_size == 0 )
		return NULL;

	m = malloc(sizeof(*m));
	if ( NULL == m )
		return NULL;

	if ( obj_size < sizeof(void *) )
		obj_size = sizeof(void *);

	m->obj_size = obj_size;

	if ( slab_size ) {
		m->slab_size = sizeof(struct _mpool_hdr) + 
				(slab_size * obj_size);
	}else{
		/* XXX: totally arbitrary... */
		if ( m->obj_size < 8192 ) {
			m->slab_size = sizeof(struct _mpool_hdr) +
				(8192 - (8192 % m->obj_size));
		}else{
			m->slab_size = sizeof(struct _mpool_hdr) +
				(4 * m->obj_size);
		}
	}

	m->slabs = NULL;
	m->free = NULL;

	return m;
}

/** Slow path for mpool allocations.
 * \ingroup g_mpool
 * @param m a valid mpool structure returned from mpool_init()
 *
 * Allocate a new object, returns NULL if out of memory. Note that this
 * is the slow path, the fast path for common case (no new allocation
 * needed) is handled in the inline fucntion mpool_alloc() in mpool.h.
 *
 * @return a new object
 */
static void *mpool_alloc_slow(struct _mpool *m)
{
	struct _mpool_hdr *h;
	void *ptr, *ret;
	
	h = ptr = malloc(m->slab_size);
	if ( h == NULL )
		return NULL;

	POISON(ptr, m->slab_size);

	/* Set first object */
	ret = ptr + sizeof(*h);
	h->next_obj = ret + m->obj_size;

	/* prepend to slab list */
	h->next = m->slabs;
	m->slabs = h;

	return ret;
}

/** Allocate an object from an mpool.
 * \ingroup g_mpool
 * @param m a valid mpool structure returned from mpool_init()
 *
 * Allocate a new object, returns NULL if out of memory. This is the
 * fast path. It never calls malloc directly.
 *
 * @return a new object
 */
void *mpool_alloc(mpool_t m)
{
	/* Try a free'd object first */
	if ( unlikely(m->free) ) {
		void *ret = m->free;
		m->free = *(void **)m->free;
		return ret;
	}

	/* If there is space in the slab, allocate */
	if ( m->slabs != NULL ) {
		void *ret = m->slabs->next_obj;
		m->slabs->next_obj += m->obj_size;

		if ( m->slabs->next_obj <= ((void *)m->slabs) + m->slab_size )
			return ret;
	}

	/* Otherwise go to slow path (calls malloc) */
	return mpool_alloc_slow(m);
}

/** Destroy an mpool object.
 * \ingroup g_mpool
 * @param m a valid mpool structure returned from mpool_init()
 *
 * Frees up all allocated memory from the #mpool and resets all members
 * to invalid values.
 */
void mpool_free(mpool_t m)
{
	struct _mpool_hdr *h, *f;

	if ( NULL == m )
		return;

	for(h = m->slabs; (f = h); free(f)) {
		h = h->next;
		POISON(f, m->slab_size);
	}

	POISON(m, sizeof(*m));
	free(m);
}

/** Free an individual object.
 * \ingroup g_mpool
 * @param m mpool object that obj was allocated from.
 * @param obj pointer to object to free.
 *
 * When an mpool object is free'd it's added to a linked list of
 * free objects which mpool_alloc() scans before trying to commit
 * further memory resources.
*/
void mpool_return(mpool_t m, void *obj)
{
	if ( unlikely(obj == NULL) )
		return;
	if ( unlikely(m->obj_size < sizeof(void *)) )
		return;
	POISON(obj, m->obj_size);
	*(void **)obj = m->free;
	m->free = obj;
}

/** Allocate an object initialized to zero.
 * \ingroup g_mpool
 *
 * @param m mpool object to allocate from.
 *
 * This is the same as mpool_alloc() but initializes the returned
 * memory to all zeros.
 *
 * @return pointer to new object or NULL for error
*/
void *mpool_alloc0(mpool_t m)
{
	void *ret;

	ret = mpool_alloc(m);
	if ( ret )
		memset(ret, 0, m->obj_size);

	return ret;
}
