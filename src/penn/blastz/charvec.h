#ifndef CHARVEC_H
#define CHARVEC_H
#include <unistd.h>

typedef struct charvec {
    char *a;
    unsigned int len;
    unsigned int max;
    void *((*alloc)(void*, size_t));
    void (*free)(void*);
} charvec_t;

int charvec_fini(charvec_t *t);
int charvec_need(charvec_t *t, unsigned int n);
int charvec_more(charvec_t *t, unsigned int n);
int charvec_append(charvec_t *t, char e);
int charvec_fit(charvec_t *t);

#define charvec_INIT(a,f)  {0, 0, 0, a, f}

#endif /* CHARVEC_H */
