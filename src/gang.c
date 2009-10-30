/*
 * This file is part of ccid-utils
 * Copyright (c) 2009 Gianni Tedesco <gianni@scaramanga.co.uk>
 * Released under the terms of the GNU GPL version 3
*/
#include <compiler.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <gang.h>

#if GANG_POISON
#define POISON(ptr, len) memset(ptr, GANG_POISON_PATTERN, len)
#else
#define POISON(ptr, len) do { } while(0);
#endif

struct _slab {
	struct _slab *s_next;
	uint8_t s_data[0];
};

struct _gang {
	size_t g_align;
	size_t g_alloc;
	struct _slab *g_slab;
	uint8_t *g_ptr;
};

gang_t gang_new(size_t alloc, size_t align)
{
	struct _gang *g;

	g = malloc(sizeof(*g));
	if ( NULL == g )
		return NULL;
	
	g->g_align = align;
	g->g_alloc = (0 == alloc) ? GANG_DEFAULT_ALLOC : alloc;
	g->g_slab = NULL;
	g->g_ptr = NULL;

	return g;
}

static uint8_t *ptr_align(uint8_t *ptr, size_t sz)
{
	/* later for this bullshit */
	return ptr;
}

static void *do_alloc_slow(struct _gang *g, size_t sz, size_t align)
{
	struct _slab *s;
	uint8_t *ret;

	s = malloc(g->g_alloc);
	if ( NULL == s )
		return NULL;

	POISON(s, g->g_alloc);

	ret = ptr_align(s->s_data, align);
	if ( ret + sz > (uint8_t *)s + g->g_alloc ) {
		free(s);
		return NULL;
	}

	s->s_next = g->g_slab;
	g->g_slab = s;
	g->g_ptr = ret + sz;

	return ret;
}

static void *do_alloc(struct _gang *g, size_t sz, size_t align)
{
	uint8_t *nxt;

	if ( NULL == g->g_ptr )
		return do_alloc_slow(g, sz, align);

	nxt = ptr_align(g->g_ptr, align);
	if ( nxt + sz >= (uint8_t *)g->g_slab + g->g_alloc )
		return do_alloc_slow(g, sz, align);

	g->g_ptr += sz;
	return nxt;
}

void *gang_alloc(gang_t g, size_t sz)
{
	return do_alloc(g, sz, g->g_align);
}

void *gang_alloc_a(gang_t g, size_t sz, size_t align)
{
	return do_alloc(g, sz, align);
}

void *gang_alloc0(gang_t g, size_t sz)
{
	void *ret;
	ret = do_alloc(g, sz, g->g_align);
	if ( ret )
		memset(ret, 0, sz);
	return ret;
}

void *gang_alloc0_a(gang_t g, size_t sz, size_t align)
{
	void *ret;
	ret = do_alloc(g, sz, align);
	if ( ret )
		memset(ret, 0, sz);
	return ret;
}

void gang_free(gang_t g)
{
	struct _slab *s, *tmp;

	if ( NULL == g )
		return;

	for(s = g->g_slab; (tmp = s); free(tmp)) {
		s = s->s_next;
		POISON(tmp, g->g_alloc);
	}

	POISON(g, sizeof(*g));
	free(g);
}
