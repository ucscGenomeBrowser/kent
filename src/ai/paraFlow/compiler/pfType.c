/* pfType - ParaFlow type heirarchy */

#include "common.h"
#include "hash.h"
#include "pfType.h"

void pfCollectedTypeDump(struct pfCollectedType *ct, FILE *f)
/* Write out info on ct to file. */
{
while (ct != NULL)
    {
    fprintf(f, "%s", ct->base->name);
    if (ct->next != NULL)
        fprintf(f, " of ");
    ct = ct->next;
    }
}

