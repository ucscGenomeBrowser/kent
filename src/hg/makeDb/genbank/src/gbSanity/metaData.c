/* Object to store metadata from database for use in verification, also
 * a few shared functions. */


#include "metaData.h"
#include "common.h"
#include "localmem.h"
#include "errabort.h"

static char const rcsid[] = "$Id: metaData.c,v 1.1 2003/06/03 01:27:44 markd Exp $";

struct metaDataTbls
/* Object with metadata collect from various tables */
{
    struct hash* accHash;
    struct hashCookie cookie;
};

int errorCnt = 0;  /* count of errors */
boolean testMode = FALSE; /* Ignore errors that occure in test db */

static char *gbDatabase = NULL;

void gbErrorSetDb(char *database)
/* set database to use in error messages */
{
gbDatabase = cloneString(database);
}

void gbError(char *format, ...)
/* print and count an error */
{
va_list args;
fprintf(stderr, "Error: %s: ", gbDatabase);
va_start(args, format);
vfprintf(stderr, format, args);
va_end(args);
fputc('\n', stderr);
errorCnt++;
}

struct metaDataTbls* metaDataTblsNew()
/* create a metaData table object */
{
struct metaDataTbls* mdt;
AllocVar(mdt);
mdt->accHash = hashNew(12);
return mdt;
}

struct metaData* metaDataTblsFind(struct metaDataTbls* mdt,
                                  char* acc)
/* Find metadata by acc or NULL if not found.  Record is buffered and will be
 * replaced on the next access. */
{
return hashFindVal(mdt->accHash, acc);
}

struct metaData* metaDataTblsGet(struct metaDataTbls* mdt, char* acc)
/* Get or create metadata table entry for an acc.  Record is buffered and will
 * be replaced on the next access. */
{
struct hashEl* hel = hashLookup(mdt->accHash, acc);
if (hel == NULL)
    {
    struct lm* lm = mdt->accHash->lm;
    struct metaData* md;
    lmAllocVar(lm, md);
    hel = hashAdd(mdt->accHash, acc, md);
    safef(md->acc, sizeof(md->acc), "%s", hel->name);
    /* guess source database from accession */
    md->typeFlags |= gbGuessSrcDb(acc);
    }
return (struct metaData*)hel->val;
}

void metaDataTblsFirst(struct metaDataTbls* mdt)
/* Set the pointer so the next call to metaDataTblsNext returns the first
 * entry */
{
mdt->cookie = hashFirst(mdt->accHash);
}

struct metaData* metaDataTblsNext(struct metaDataTbls* mdt)
/* Get the next entry on serial reading of the table, */
{
struct hashEl* hel = hashNext(&mdt->cookie);
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
freez(mdtPtr);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


