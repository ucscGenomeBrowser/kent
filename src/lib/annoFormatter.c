/* annoFormatter -- aggregates, formats and writes output from multiple sources */

#include "annoFormatter.h"

static struct annoFormatterOption *annoFormatterOptionCloneList(struct annoFormatterOption *list)
/* Return a copy of list. */
{
struct annoFormatterOption *newList = NULL, *afo;
for (afo = list;  afo != NULL;  afo = afo->next)
    {
    struct annoFormatterOption *newAfo = CloneVar(afo);
    newAfo->spec.name = cloneString(afo->spec.name);
    newAfo->value = NULL;
    unsigned opFlags = opFlags;
    if (opFlags & OPTION_MULTI)
	{
//#*** What does it mean to have OPTION_MULTI set for an output field?
	switch (opFlags & OPTION_TYPE_MASK)
	    {
	    case OPTION_STRING:
		newAfo->value = slNameCloneList((struct slName *)(afo->value));
		break;
	    default:
		errAbort("annoFormatterOptionCloneList: OPTION_MULTI implemented only for "
			 "OPTION_STRING (not 0x%x)", opFlags);
	    }
	}
    else
	{
	switch (opFlags & OPTION_TYPE_MASK)
	    {
	    // For numeric singleton values, we are overloading value.
	    case OPTION_DOUBLE:
	    case OPTION_FLOAT:
	    case OPTION_LONG_LONG:
	    case OPTION_INT:
	    case OPTION_BOOLEAN:
		newAfo->value = afo->value;
		break;
	    case OPTION_STRING:
		newAfo->value = cloneString((char *)afo->value);
		break;
	    default:
		errAbort("annoFormatterOptionCloneList: unrecognized op type 0x%x", opFlags);
	    }
	}
    slAddHead(&newList, newAfo);
    }
slReverse(&newList);
return newList;
}

static void annoFormatterOptionFreeList(struct annoFormatterOption **pList)
/* Free the same things that we clone above. */
{
if (pList == NULL)
    return;
struct annoFormatterOption *afo, *nextAfo;
for (afo = *pList;  afo != NULL;  afo = nextAfo)
    {
    nextAfo = afo->next;
    freeMem(afo->spec.name);
    unsigned opFlags = opFlags;
    if (opFlags & OPTION_MULTI)
	{
	switch (opFlags & OPTION_TYPE_MASK)
	    {
	    case OPTION_STRING:
		slNameFreeList(&(afo->value));
		break;
	    default:
		errAbort("annoFormatterOptionFreeList: OPTION_MULTI implemented only for "
			 "OPTION_STRING (not 0x%x)", opFlags);
	    }
	}
    else
	{
	switch (opFlags & OPTION_TYPE_MASK)
	    {
	    // No need to free overloaded numeric values
	    case OPTION_DOUBLE:
	    case OPTION_FLOAT:
	    case OPTION_LONG_LONG:
	    case OPTION_INT:
	    case OPTION_BOOLEAN:
		break;
	    case OPTION_STRING:
		freeMem(afo->value);
		break;
	    default:
		errAbort("annoFormatterOptionFreeList: unrecognized op type 0x%x", opFlags);
	    }
	}
    freeMem(afo);
    }
*pList = NULL;
}

struct annoFormatterOption *annoFormatterGetOptions(struct annoFormatter *self)
/* Return supported options and current settings.  Callers can modify and free when done. */
{
return annoFormatterOptionCloneList(self->options);
}

void annoFormatterSetOptions(struct annoFormatter *self, struct annoFormatterOption *newOptions)
/* Free old options and use clone of newOptions. */
{
annoFormatterOptionFreeList(&(self->options));
self->options = annoFormatterOptionCloneList(newOptions);
}

void annoFormatterFree(struct annoFormatter **pSelf)
/* Free self. This should be called at the end of subclass close methods, after
 * subclass-specific connections are closed and resources are freed. */
{
if (pSelf == NULL)
    return;
annoFormatterOptionFreeList(&((*pSelf)->options));
freez(pSelf);
}
