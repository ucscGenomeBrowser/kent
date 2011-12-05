/* spMapper - map swissprot accessions to KnownGene ids.  Intended to use in
 * building gene sorter tables.  Can optionally map uniref entry ids. */
#include "common.h"
#include "spMapper.h"
#include "localmem.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "spKgMap.h"
#include "unirefTbl.h"
#include "linefile.h"
#include "hgRelate.h"
#include "verbose.h"
#include "obscure.h"


/* values for noMapTbl */
char noUnirefMapping = 'R';           /* indicates no uniref mapping */
char noKGMapping = 'K';               /* indicates no kg mapping */

static struct kgEntry *kgEntryAdd(struct spMapper *sm, struct hashEl* kgHel, struct kgPair *kgPair)
/* Add a new kgEntry to a hash chain */
{
struct kgEntry *kgEntry;
lmAllocVar(sm->kgPairMap->lm, kgEntry);
kgEntry->id = kgHel->name;
kgEntry->kgPair = kgPair;
slAddHead((struct kgEntry**)&kgHel->val, kgEntry);
return kgEntry;
}

static void kgPairAdd(struct spMapper *sm, struct hashEl* kg1Hel,
                      struct hashEl* kg2Hel, float score)
/* add a new kgPair object to the hash */
{
struct kgPair *kgPair;

lmAllocVar(sm->kgPairMap->lm, kgPair);
kgPair->score = score;
slAddHead(&sm->kgPairList, kgPair);

/* link both kgId objects in struct */
kgPair->kg1Entry = kgEntryAdd(sm, kg1Hel, kgPair);
kgPair->kg2Entry = kgEntryAdd(sm, kg2Hel, kgPair);
}

static struct kgPair *kgPairFind(struct hashEl* queryHel, struct hashEl* targetHel)
/* find a kgPair object for a pair of kg ids, or NULL if not in table */
{
struct kgEntry *entry;

/* search for an entry for the pair.  Ok to compare string ptrs here, since
 * they are in same local memory.  Don't know order in structure, so must
 * check both ways.
 */
for (entry = queryHel->val; entry != NULL; entry = entry->next)
    {
    if (((entry->kgPair->kg1Entry->id == queryHel->name)
         && (entry->kgPair->kg2Entry->id == targetHel->name))
        || ((entry->kgPair->kg1Entry->id == targetHel->name)
            && (entry->kgPair->kg2Entry->id == queryHel->name)))
        return entry->kgPair;
    }
return NULL;
}

static void kgPairSave(struct spMapper *sm, char *qSpId, char *tSpId,
                       char *queryId, char *targetId, float score)
/* save a bi-directional e-value for a query/target pair */
{
struct hashEl *queryHel = hashStore(sm->kgPairMap, queryId);
struct hashEl *targetHel = hashStore(sm->kgPairMap, targetId);
struct kgPair *kgPair = kgPairFind(queryHel, targetHel);
if (kgPair != NULL)
    {
    if (((sm->scoreDir < 0) && (score < kgPair->score))
        || ((sm->scoreDir > 0) && (score > kgPair->score)))
        {
        kgPair->score = score; /* better score */
        verbose(3, "updating: %s <> %s -> %s <> %s: %0.3f\n", qSpId, tSpId,
                queryId, targetId, score);
        }
    }
else
    {
    kgPairAdd(sm, queryHel, targetHel, score);
    verbose(3, "mapping: %s <> %s -> %s <> %s: %0.3f\n", qSpId, tSpId,
            queryId, targetId, score);
    }
}

static void savePairs(struct spMapper *sm, char *qSpId, char *tSpId,
                      struct slName *qIds, struct slName *tIds, float score)
/* add all combinations of ids. */
{
struct slName *qId, *tId;

/* transform to common structure */
for (qId = qIds; qId != NULL; qId = qId->next)
    {
    for (tId = tIds; tId != NULL; tId = tId->next)
        kgPairSave(sm, qSpId, tSpId, qId->name, tId->name, score);
    }
}

struct spMapper *spMapperNew(struct sqlConnection *conn, int scoreDir,
                             char *unirefFile, char *orgFilter)
/* construct a new spMapper object. scoreDir is 1 if higher score is better,
 * -1 if lower score is better If unirefFile is not null, this is a tab file
 * reformatting of the uniref xml file with member data extracted (see
 * uniref.h).  If orgFilter is not null, then only uniref members from this
 * organism are loaded.
 */
{
struct spMapper *sm;
AllocVar(sm);
sm->scoreDir = scoreDir;
sm->spKgMap = spKgMapNew(conn);
if (unirefFile != NULL)
    sm->unirefTbl = unirefTblNew(unirefFile, orgFilter);
sm->kgPairMap = hashNew(23);
sm->noMapTbl = hashNew(22);
return sm;
}

void spMapperFree(struct spMapper **smPtr)
/* free smMapper object */
{
struct spMapper *sm = *smPtr;
if (sm != NULL)
    {
    spKgMapFree(&sm->spKgMap);
    unirefTblFree(&sm->unirefTbl);
    hashFree(&sm->noMapTbl);
    freez(smPtr);
    }
}

static struct slName *mapSpId(struct spMapper *sm, char *spId)
/* get the list of ids associated with a swissprot.  If the swssprot acc has
 * an isoform extension, look up with and without the extension, */
{
struct slName *ids = spKgMapGet(sm->spKgMap, spId);
if (ids == NULL)
    {
    char *extPtr = strchr(spId, '-');
    if (extPtr != NULL)
        {
        *extPtr = '\0';
        ids = spKgMapGet(sm->spKgMap, spId);
        *extPtr = '-';
        }
    }
return ids;
}

static void handleNoMapping(struct spMapper *sm, char *id, char reason)
/* record id that can't be mapped */
{
struct hashEl *hel = hashStore(sm->noMapTbl, id);
if (hel->val == NULL)
    {
    if (reason == noUnirefMapping)
        sm->noUnirefMapCnt++;
    else if (reason == noKGMapping)
        sm->noSpIdMapCnt++;
    }
hel->val = intToPt(reason);
}

static void mapUniprot(struct spMapper *sm, char *qSpId, char *tSpId, float score)
/* map uniprot/swissprot ids without uniref mappings */
{
struct slName *qIds = mapSpId(sm, qSpId);
struct slName *tIds = mapSpId(sm, tSpId);

if ((qIds == NULL) || (tIds == NULL))
    {
    if (qIds == NULL)
        handleNoMapping(sm, qSpId, noKGMapping);
    if (tIds == NULL)
        handleNoMapping(sm, tSpId, noKGMapping);
    sm->qtNoSpIdMapCnt++;
    }
else 
    {
    savePairs(sm, qSpId, tSpId, qIds, tIds, score);
    sm->qtMapCnt++;
    }
}

static void mapUnirefEntries(struct spMapper *sm,
                             struct uniref *qUniref, struct uniref *tUniref,
                             char *qSpId, char *tSpId, float score)
/* add all combinations of uniref entries */
{
struct uniref *qUniprot, *tUniprot;
boolean qKgFound = FALSE, tKgFound = FALSE;

/* build list of kg pairs associate with these two uniref entries, avoiding
 * duplicated pairs. */
for (qUniprot = qUniref; qUniprot != NULL; qUniprot = qUniprot->next)
    {
    for (tUniprot = tUniref; tUniprot != NULL; tUniprot = tUniprot->next)
        {
        struct slName *qIds = mapSpId(sm, qUniprot->upAcc);
        struct slName *tIds = mapSpId(sm, tUniprot->upAcc);
        if (qIds != NULL)
            qKgFound = TRUE;
        if (tIds != NULL)
            tKgFound = TRUE;
        if ((qIds != NULL) && (tIds != NULL))
            savePairs(sm, qSpId, tSpId, qIds, tIds, score);
        }
    }
if (!(qKgFound && tKgFound))
    {
    /* couldn't map to known genes */
    if (!qKgFound)
        handleNoMapping(sm, qSpId, noKGMapping);
    if (!tKgFound)
        handleNoMapping(sm, tSpId, noKGMapping);
    sm->qtNoSpIdMapCnt++;
    }
else
    {
    sm->qtMapCnt++;
    }
}

static void mapUniref(struct spMapper *sm, char *qSpId, char *tSpId, float score)
/* load a rankProt with uniref mappings */
{
struct uniref *qUniref = unirefTblGetEntryById(sm->unirefTbl, qSpId);
struct uniref *tUniref = unirefTblGetEntryById(sm->unirefTbl, tSpId);

if ((qUniref == NULL) || (tUniref == NULL))
    {
    /* couldn't map to uniref */
    if (qUniref == NULL)
        handleNoMapping(sm, qSpId, noUnirefMapping);
    if (tUniref == NULL)
        handleNoMapping(sm, tSpId, noUnirefMapping);
    sm->qtNoUnirefMapCnt++;
    }
else
    {
    mapUnirefEntries(sm, qUniref, tUniref, qSpId, tSpId, score);
    }
}

void spMapperMapPair(struct spMapper *sm, char *qSpId, char *tSpId, float score)
/* map a pair of swissprot/uniprot ids to one or more pairs of known gene
 * ids and add them to the table */
{
if (sm->unirefTbl != NULL)
    return mapUniref(sm, qSpId, tSpId, score);
else
    return mapUniprot(sm, qSpId, tSpId, score);
}

void spMapperPrintNoMapInfo(struct spMapper *sm, char *outFile)
/* Print missed id table. First column is id, second column is R if it wasn't
 * found in uniref, or K if it couldn't be associated with a known gene */
{
FILE *fh = mustOpen(outFile, "w");
struct hashCookie cookie = hashFirst(sm->noMapTbl);
struct hashEl *hel;

while ((hel = hashNext(&cookie)) != NULL)
    fprintf(fh, "%s\t%c\n", hel->name, ptToInt(hel->val));

carefulClose(&fh);
}

