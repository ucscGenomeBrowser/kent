#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "jksql.h"
#include "chainView.h"

struct chainView *chainViewLoad(char **row)
{
struct chainView *ret;
int sizeOne,i;
char *s;

AllocVar(ret);
ret->score = atof(row[0]);
ret->tName = cloneString(row[1]);
ret->tSize = sqlUnsigned(row[2]);
ret->tStart = sqlUnsigned(row[3]);
ret->tEnd = sqlUnsigned(row[4]);
ret->qName = cloneString(row[5]);
ret->qSize = sqlUnsigned(row[6]);
ret->qStrand = row[7][0];
ret->qStart = sqlUnsigned(row[8]);
ret->qEnd = sqlUnsigned(row[9]);
ret->id = sqlUnsigned(row[10]);
ret->gtStart = sqlUnsigned(row[12]);
ret->gtEnd = sqlUnsigned(row[13]);
ret->gqStart = sqlUnsigned(row[14]);
ret->chainId = sqlUnsigned(row[15]);
return ret;
}
void chainViewFree(struct chainView **pEl)
/* Free a single dynamically allocated chainView such as created
 * with chainViewLoad(). */
{
struct chainView *el;

struct chainView *cv = *pEl;
if ((el = *pEl) == NULL) return;
freeMem(el->tName);
freeMem(el->qName);
slFreeList(&cv->blockList);
freez(pEl);
}

void chainViewFreeList(struct chainView **pList)
/* Free a list of dynamically allocated chainView's */
{
struct chainView *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    chainViewFree(&el);
    }
*pList = NULL;
}

