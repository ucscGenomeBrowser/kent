/* spKgMap - map swissprot accs to known gene ids */
#include "common.h"
#include "spKgMap.h"
#include "jksql.h"
#include "kgXref.h"
#include "localmem.h"
#include "hash.h"
#include "linefile.h"

struct spKgMap
/* map of  swissprot accs to kg ids */
{
    struct hash *spHash;  /* hash to slName list, or to NULL if no mapping */
};


struct spKgMap *spKgMapNew(struct sqlConnection *conn)
/* load data from from kgXRef table to map from swissprot accs to
 * known gene ids. */
{
struct spKgMap *spKgMap;
char query[1024];
struct kgXref kgXref;
struct sqlResult *sr;
struct hashEl *hel;
struct slName *kgId;
char **row;

AllocVar(spKgMap);
spKgMap->spHash = hashNew(21);

safef(query, sizeof(query), "SELECT * from kgXref");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    kgXrefStaticLoad(row, &kgXref);
    hel = hashStore(spKgMap->spHash, kgXref.spID);
    kgId = lmSlName(spKgMap->spHash->lm, kgXref.kgID);
    slAddHead((struct slName**)&hel->val, kgId);
    }
sqlFreeResult(&sr);
return spKgMap;
}

void spKgMapFree(struct spKgMap **spKgMapPtr)
/* free a spKgMap structure */
{
struct spKgMap *spKgMap = *spKgMapPtr;
if (spKgMap != NULL)
    {
    hashFree(&spKgMap->spHash);
    freeMem(spKgMap);
    *spKgMapPtr = NULL;
    }
}

struct slName *spKgMapGet(struct spKgMap *spKgMap, char *spId)
/* Get the list of known gene ids for a swissprot acc.  If not found NULL is
 * returned and an entry is made in the table that can be used to get the
 * count of missing mappings. */
{
/* this will add an entry with a NULL kgId if not found */
struct hashEl *hel = hashStore(spKgMap->spHash, spId);
return hel->val;
}

int spKgMapCountMissing(struct spKgMap *spKgMap)
/* count the number swissprot accs with no corresponding known gene */
{
int missing = 0;
struct hashCookie cookie = hashFirst(spKgMap->spHash);
struct hashEl *hel;

while ((hel = hashNext(&cookie)) != NULL)
    {
    if (hel->val == NULL)
        missing++;
    }
return missing;
}

void spKgMapListMissing(struct spKgMap *spKgMap, char *file)
/* output the swissprot accs with no corresponding known gene */
{
FILE *fh = mustOpen(file, "w");
struct hashCookie cookie = hashFirst(spKgMap->spHash);
struct hashEl *hel;

while ((hel = hashNext(&cookie)) != NULL)
    {
    if (hel->val == NULL)
        fprintf(fh, "%s\n", hel->name);
    }
carefulClose(&fh);
}

