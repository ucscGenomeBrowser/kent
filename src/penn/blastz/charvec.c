#include "charvec.h"

static const char rcsid[]=
"$Id: charvec.c,v 1.1 2002/09/25 05:44:07 kent Exp $";

/* This implementation uses a "growable array" abstraction */

int charvec_fini(charvec_t *t)
{
    if (t->a && t->free) { t->free(t->a); t->a = 0; t->max = 0; }
    t->len = 0;
    return 1;
}

#ifndef BASE_ALLOC
#define BASE_ALLOC 30
#endif
enum { BASE = BASE_ALLOC };

int charvec_need(charvec_t *t, unsigned int n)
{
    if (t->a == 0) {
        t->len = 0; 
        t->max = n;
        t->a = t->alloc(0, n * sizeof(char));
        return t->a != 0;
    }

    if (n > t->max) { 
        unsigned int i = BASE + n + (n >> 3); 
        void *p = t->alloc(t->a, i * sizeof(char));
        if (!p)
            return 0;
        t->max = i;
        t->a = p;
    }
    return 1;
}

int charvec_more(charvec_t *t, unsigned int n)
{
    return charvec_need(t, n + t->len);
}

int charvec_append(charvec_t *t, char e)
{
    if (!charvec_more(t, 1))
        return 0;
    t->a[t->len++] = e;
    return 1;
}

int charvec_fit(charvec_t *t)
{
    unsigned int i = t->len;
    void *p = t->alloc(t->a, i * sizeof(char));
    if (!p)
        return 0;
    t->max = i;
    t->a = p;
    return 1;
}

