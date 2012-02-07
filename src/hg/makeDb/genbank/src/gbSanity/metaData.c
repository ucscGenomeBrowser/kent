/* Object to store metadata from database for use in verification, also
 * a few shared functions. */


#include "metaData.h"
#include "common.h"
#include "localmem.h"
#include "errabort.h"


struct metaDataTbls
/* Object with metadata collect from various tables */
{
    struct hash* accHash;
    struct hashCookie accCookie;
    struct hash* protAccHash;
};

struct metaDataTbls* metaDataTblsNew()
/* create a metaData table object */
{
struct metaDataTbls* mdt;
AllocVar(mdt);
mdt->accHash = hashNew(22);
mdt->protAccHash = hashNew(18);
return mdt;
}

struct metaData* metaDataTblsFind(struct metaDataTbls* mdt,
                                  char* acc)
/* Find metadata by acc or NULL if not found.*/
{
return hashFindVal(mdt->accHash, acc);
}

struct metaData* metaDataTblsGet(struct metaDataTbls* mdt, char* acc)
/* Get or create metadata table entry for an acc. */
{
struct hashEl* hel = hashStore(mdt->accHash, acc);
if (hel->val == NULL)
    {
    struct lm* lm = mdt->accHash->lm;
    struct metaData* md;
    lmAllocVar(lm, md);
    hel->val = md;
    safef(md->acc, sizeof(md->acc), "%s", hel->name);
    /* guess source database from accession */
    md->typeFlags |= gbGuessSrcDb(acc);
    }
return (struct metaData*)hel->val;
}

void metaDataTblsAddProtAcc(struct metaDataTbls* mdt, struct metaData* md)
/* Add protein accession to protAccHash */
{
struct hashEl* hel = hashStore(mdt->protAccHash, md->rlProtAcc);
if (hel->val != NULL)
    errAbort("protein %s already entered", md->rlProtAcc);
hel->val = md;
}

struct metaData* metaDataTblsGetByPep(struct metaDataTbls* mdt, char* pepAcc)
/* Get metadata table entry for an peptide acc, or NULL if not found */
{
return hashFindVal(mdt->protAccHash, pepAcc);
}

void metaDataTblsFirst(struct metaDataTbls* mdt)
/* Set the pointer so the next call to metaDataTblsNext returns the first
 * entry */
{
mdt->accCookie = hashFirst(mdt->accHash);
}

struct metaData* metaDataTblsNext(struct metaDataTbls* mdt)
/* Get the next entry on serial reading of the table, */
{
struct hashEl* hel = hashNext(&mdt->accCookie);
if (hel == NULL)
    return NULL;
else
    return hel->val;
}

void metaDataTblsFree(struct metaDataTbls** mdtPtr)
/* Free a metaDataTbls object */
{
struct metaDataTbls* mdt = *mdtPtr;
hashFree(&mdt->accHash);
hashFree(&mdt->protAccHash);
freez(mdtPtr);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


