/* unirefTbl - load and manage uniref data. */
#include "common.h"
#include "unirefTbl.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"

static char const rcsid[] = "$Id: unirefTbl.c,v 1.2 2006/07/06 21:09:27 fanhsu Exp $";

struct unirefTbl
/* table of uniref entries.*/
{
    struct hash *entryMap;    /* hash by entry id to unirefEntry object */
    struct hash *accEntryMap; /* map of protein accession to unirefEntry
                               * containing protein */
    char *orgFilter;          /* restrict to this org if not null */
};

struct unirefEntry
/* on uniref entry */
{
    char *id;               /* entry id (memory not owned) */
    struct uniref *mems;    /* list of members */
};

static void addEntry(struct unirefTbl *ut, struct uniref *prot)
/* Add an prot to the table. prot rec will be copied */
{
struct hashEl *entryEl = hashStore(ut->entryMap, prot->entryId);
struct unirefEntry *entry = entryEl->val;
struct uniref *protCp;

if (entry == NULL)
    {
    /* new entry */
    lmAllocVar(ut->entryMap->lm, entry);
    entry->id = entryEl->name;
    entryEl->val = entry;
    }

/* copy uniref object, storing in localmem and reusing strings */
lmAllocVar(ut->entryMap->lm, protCp);
*protCp = *prot;
if (ut->orgFilter != NULL)
    protCp->org = ut->orgFilter;  /* just save by not cloning */
else
    protCp->org = lmCloneString(ut->entryMap->lm, prot->org);

slAddHead(&entry->mems, protCp);

/* link accession back to entry. Accessions maybe in multiple entries, which
 * means multiple entries in the hash table */
hashAdd(ut->accEntryMap, protCp->upAcc, entry);
}

struct unirefTbl *unirefTblNew(char *unirefTabFile, char *orgFilter)
/* construct a unirefTbl object from the tab seperated file.  If orgFilter is
 * not null, load only records for this organism */
{
struct lineFile *lf;
char *row[UNIREF_NUM_COLS];
struct uniref prot;
struct unirefTbl *ut;
AllocVar(ut);
ut->entryMap = hashNew(21);
ut->accEntryMap = hashNew(22);
if (orgFilter != NULL)
    ut->orgFilter = lmCloneString(ut->entryMap->lm, orgFilter);

lf = lineFileOpen(unirefTabFile, TRUE);
while (lineFileNextRowTab(lf, row, UNIREF_NUM_COLS))
    {
    unirefStaticLoad(row, &prot);
    if ((orgFilter == NULL) || sameString(prot.org, orgFilter))
        addEntry(ut, &prot);
    }
lineFileClose(&lf);
return ut;
}

void unirefTblFree(struct unirefTbl **utPtr)
/* free a unirefTbl object. */
{
struct unirefTbl *ut = *utPtr;
if (ut != NULL)
    {
    hashFree(&ut->entryMap);
    hashFree(&ut->accEntryMap);
    freeMem(ut);
    *utPtr = NULL;
    }
}

struct uniref *unirefTblGetEntryById(struct unirefTbl *ut, char *entryId)
/* Get the uniref entry list (ptr to rep) for an entry id, or NULL */
{
struct unirefEntry *entry = hashFindVal(ut->entryMap, entryId);
if (entry != NULL)
    return entry->mems;
else
    return NULL;
}

struct uniref *unirefTblGetEntryByAcc(struct unirefTbl *ut, char *acc)
/* Get the entry list (ptr to rep) give any accession in the entry */
{
#if 0
/* FIXME: acc can be in multiple uniref entries, so this needs to be
   restructured */
struct unirefEntry *entry = hashFindVal(ut->accEntryMap, acc);
if (entry != NULL)
    return entry->mems;
else
    return NULL;
#else
errAbort("unirefTblGetEntryByAcc not implemented");
return NULL;
#endif
}
