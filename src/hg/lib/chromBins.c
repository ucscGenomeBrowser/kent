/* chromBins - object for storing per-chrom binKeeper objects */
#include "common.h"
#include "chromBins.h"
#include "binRange.h"
#include "hash.h"

static char const rcsid[] = "$Id: chromBins.c,v 1.6 2008/09/17 18:10:13 kent Exp $";

/* Sized hold a very larger chromosome. */
static const int MAX_CHROM_SIZE = 1000000000;

struct chromBins *chromBinsNew(chromBinsFreeFunc *freeFunc)
/* create a new chromBins object */
{
struct chromBins *chromBins;
AllocVar(chromBins);
chromBins->freeFunc = freeFunc;
chromBins->chromTbl = hashNew(19); /* handle scaffolds too */
return chromBins;
}

static void freeChrom(struct chromBins *chromBins, struct binKeeper *chromBk)
/* free a chrom's binRange and optional  entries */
{
if (chromBins->freeFunc != NULL)
    {
    struct binKeeperCookie cookie = binKeeperFirst(chromBk);
    struct binElement* be;
    while ((be = binKeeperNext(&cookie)) != NULL)
        (*chromBins->freeFunc)(&be->val);
    }
binKeeperFree(&chromBk);
}

void chromBinsFree(struct chromBins **chromBinsPtr)
/* free chromBins object, calling freeFunc on each entry if it was specified */
{
struct chromBins *chromBins = *chromBinsPtr;
if (chromBins != NULL)
    {
    struct hashCookie cookie = hashFirst(chromBins->chromTbl);
    struct hashEl* hel;
    while ((hel = hashNext(&cookie)) != NULL)
        freeChrom(chromBins, hel->val);
    hashFree(&chromBins->chromTbl);
    freeMem(chromBins);
    *chromBinsPtr = NULL;
    }
}

struct slName *chromBinsGetChroms(struct chromBins *chromBins)
/* get list of chromosome names in the object.  Result should
 * be freed with slFreeList() */
{
struct slName *chroms = NULL;
struct hashCookie cookie = hashFirst(chromBins->chromTbl);
struct hashEl* hel;
while ((hel = hashNext(&cookie)) != NULL)
    slSafeAddHead(&chroms, slNameNew(hel->name));
return chroms;
}

struct binKeeper *chromBinsGet(struct chromBins* chromBins, char *chrom,
                               boolean create)
/* get chromosome binKeeper, optionally creating if it doesn't exist */
{
if (create)
    {
    struct hashEl *hel = hashStore(chromBins->chromTbl, chrom);
    if (hel->val == NULL)
        hel->val = binKeeperNew(0, MAX_CHROM_SIZE);
    return (struct binKeeper*)hel->val;
    }
else
    return (struct binKeeper*)hashFindVal(chromBins->chromTbl, chrom);
}

void chromBinsAdd(struct chromBins *chromBins, char *chrom, int start, int end,
                  void *obj)
/* add an object to a by chrom binKeeper hash */
{
struct binKeeper *bins = chromBinsGet(chromBins, chrom, TRUE);
binKeeperAdd(bins, start, end, obj);
}


struct binElement *chromBinsFind(struct chromBins *chromBins, char *chrom,
                                 int start, int end)
/* get list of overlaping objects in a by chrom binRange hash */
{
struct binKeeper *bins = chromBinsGet(chromBins, chrom, FALSE);
if (bins != NULL)
    return binKeeperFind(bins, start, end);
else
    return NULL;
}
