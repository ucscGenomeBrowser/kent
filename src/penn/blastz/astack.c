#define NDEBUG

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#ifndef INLINE
#define INLINE static inline
#endif

#ifdef TESTING
static void *ckallocz(unsigned int n)
{
	return calloc(n,1);
}
static void *ckalloc(unsigned int n)
{
	return malloc(n);
}
static void *ckrealloc(void *p, unsigned int n)
{
	return realloc(p,n);
}
#endif

typedef struct comb {
	int alen;
	int* a;  // array values 0..alen-1
	int nv;
	int* v;  // bag of valid indexes 0..alen-1
	int origin;
} comb_t;

INLINE comb_t *comb_clear(comb_t *a)
{
	int i;
	for (i=0; i<a->nv; ++i)
		a->a[a->v[i]] = 0;
	a->nv = 0;
	return a;
}

INLINE comb_t *comb_new(int nints, int origin)
{
	comb_t *c = ckalloc(sizeof(*c));
	c->alen = nints;
	c->a = ckallocz(nints * sizeof(c->a[0]));
	c->nv = 0;
	c->v = ckalloc(nints * sizeof(c->v[0])); // XXX - could be growable
	c->origin = origin;
	return c;
}

INLINE comb_t *comb_resize(comb_t *c, int nints, int origin)
{
	if (c == 0) {
		return comb_new(nints, origin);
	}

	if (c->alen < nints) {
		c->a = ckrealloc(c->a, nints*sizeof(c->a[0]));
		c->v = ckrealloc(c->v, nints*sizeof(c->v[0]));
	}
	c->alen = nints;
	c->origin = origin;
	c->nv = 0;
	return c;
}

INLINE int comb_get(comb_t *c, int n)
{
	n += c->origin;
	assert(n < c->alen);
	return c->a[n];
}

INLINE int comb_set(comb_t *c, int n, int x)
{
	n += c->origin;
	assert(n < c->alen);
	if (c->a[n] == 0)
		c->v[c->nv++] = n;
	c->a[n] = x;
	return 0;
}

INLINE int comb_free(comb_t *c)
{
	free(c->a);
	free(c->v);
	return 0;
}
// ---- comb

#ifdef TESTING
int main()
{
	int i;
	unsigned int n=60*1000*1000;
	comb_t *c;

	fprintf(stderr, "new...");
	c = comb_new(n,0);
	fprintf(stderr, "set...");
	for (i = 0; i < n; ++i) { comb_set(c, i, i); }
	fprintf(stderr, "get...");
	for (i = 0; i < n; ++i) { assert(comb_get(c, i) == i); }
	fprintf(stderr, "clear...");
	comb_clear(c);
	fprintf(stderr, "check...");
	for (i = 0; i < n; ++i) { assert(comb_get(c, i) == 0); }
	exit(0);
}
#endif
