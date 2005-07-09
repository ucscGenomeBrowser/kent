/* spMapper - map swissprot accessions to KnownGene ids.  Intended to use in
 * building gene sorter tables.  Can optionally map uniref entry ids. */
#include "common.h"
#include "spMapper.h"
#include "jksql.h"
#include "hdb.h"
#include "spKgMap.h"
#include "unirefTbl.h"
#include "linefile.h"
#include "hgRelate.h"
#include "verbose.h"
#include "obscure.h"

static char const rcsid[] = "$Id: spMapper.c,v 1.1 2005/07/09 05:18:56 markd Exp $";

/* values for noMapTbl */
char noUnirefMapping = 'R';           /* indicates no uniref mapping */
char noKGMapping = 'K';               /* indicates no kg mapping */

static void spMapperIdAdd(struct spMapperId **head, char *id)
/* add a new spMapperId to a list if it doesn't exist. id is not copy,
 * memory must be stable. */
{
struct spMapperId *ent;
for (ent = *head; (ent != NULL) && !sameString(ent->id, id); ent = ent->next)
    continue;
if (ent == NULL)
    {
    /* new entry */
    AllocVar(ent);
    ent->id = id;
    slAddHead(head, ent);
    }
}

static void spMapperPairsAdd(struct spMapperPairs **head, char *qId, char *tId)
/* add to kg id pair to list, if not already defined.  Ids not copied, memory
 * must be stable */
{
/* ok that this is O(N^2), since lists should be small */
struct spMapperPairs *pairs;
for (pairs = *head; (pairs != NULL) && !sameString(pairs->qId, qId);
     pairs = pairs->next)
    continue;

if (pairs == NULL)
    {
    /* new entry */
    AllocVar(pairs);
    pairs->qId = qId;
    slAddHead(head, pairs);
    }
spMapperIdAdd(&pairs->tIds, tId);
}

static void spMapperPairsAddIds(struct spMapperPairs **spMapperPairs, struct slName *qIds, struct slName *tIds)
/* add all combinations of ids. */
{
struct slName *qId, *tId;

/* transform to common structure */
for (qId = qIds; qId != NULL; qId = qId->next)
    {
    for (tId = tIds; tId != NULL; tId = tId->next)
        spMapperPairsAdd(spMapperPairs, qId->name, tId->name);
    }
}

void spMapperPairsFree(struct spMapperPairs **head)
/* free list of spMapperPairs objects */
{
struct spMapperPairs *pairs;
for (pairs = *head;  pairs != NULL; pairs = pairs->next)
    slFreeList(&pairs->tIds);
slFreeList(head);
}

struct spMapper *spMapperNew(struct sqlConnection *conn,
                             char *unirefFile, char *orgFilter)
/* construct a new spMapper object. If unirefFile is not null, this is a tab
 * file reformatting of the uniref xml file with member data extracted (see
 * uniref.h).  If orgFilter is not null, then only uniref members from
 * this organism are loaded.
 */
{
struct spMapper *sm;
AllocVar(sm);
sm->spKgMap = spKgMapNew(conn);
if (unirefFile != NULL)
    sm->unirefTbl = unirefTblNew(unirefFile, orgFilter);
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

static struct spMapperPairs *mapUniprot(struct spMapper *sm, char *qSpId, char *tSpId)
/* map uniprot/swissprot ids without uniref mappings */
{
struct slName *qIds = mapSpId(sm, qSpId);
struct slName *tIds = mapSpId(sm, tSpId);
struct spMapperPairs *pairs = NULL;

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
    spMapperPairsAddIds(&pairs, qIds, tIds);
    sm->qtMapCnt++;
    }
return pairs;
}

static struct spMapperPairs * mapUnirefEntries(struct spMapper *sm,
                                               struct uniref *qUniref, struct uniref *tUniref,
                                               char *qSpId, char *tSpId)
/* all combinations of uniref entries */
{
struct spMapperPairs *pairs = NULL;
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
            spMapperPairsAddIds(&pairs, qIds, tIds);
        }
    }
if (pairs == NULL)
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
return pairs;
}

static struct spMapperPairs *mapUniref(struct spMapper *sm, char *qSpId, char *tSpId)
/* load a rankProt with uniref mappings */
{
struct spMapperPairs *pairs = NULL;
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
    pairs = mapUnirefEntries(sm, qUniref, tUniref, qSpId, tSpId);
    }
return pairs;
}

struct spMapperPairs *spMapperMapPair(struct spMapper *sm, char *qSpId, char *tSpId)
/* map a pair of swissprot/uniprot ids to one or more pairs of known gene
 * ids. */
{
if (sm->unirefTbl != NULL)
    return mapUniref(sm, qSpId, tSpId);
else
    return mapUniprot(sm, qSpId, tSpId);
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

