/* annoOption -- optionSpec-style param plus its value */

#ifndef ANNOOPTION_H
#define ANNOOPTION_H

#include "options.h"

struct annoOption
/* A named and typed option and its value. */
    {
    struct annoOption *next;
    struct optionSpec spec;
    void *value;
    };

struct annoOption *annoOptionCloneList(struct annoOption *list);
/* Return a newly allocated copy of list. */

void annoOptionFreeList(struct annoOption **pList);
/* Free the same things that we clone above. */

#endif//ndef ANNOOPTION_H
