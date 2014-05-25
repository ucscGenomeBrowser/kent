/* annoFormatter -- aggregates, formats and writes output from multiple sources */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoFormatter.h"

struct annoOption *annoFormatterGetOptions(struct annoFormatter *self)
/* Return supported options and current settings.  Callers can modify and free when done. */
{
return annoOptionCloneList(self->options);
}

void annoFormatterSetOptions(struct annoFormatter *self, struct annoOption *newOptions)
/* Free old options and use clone of newOptions. */
{
annoOptionFreeList(&(self->options));
self->options = annoOptionCloneList(newOptions);
}

void annoFormatterFree(struct annoFormatter **pSelf)
/* Free self. This should be called at the end of subclass close methods, after
 * subclass-specific connections are closed and resources are freed. */
{
if (pSelf == NULL)
    return;
annoOptionFreeList(&((*pSelf)->options));
freez(pSelf);
}
