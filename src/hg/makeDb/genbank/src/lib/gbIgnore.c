#include "common.h"
#include "localmem.h"
#include "linefile.h"
#include "hash.h"
#include "gbDefs.h"
#include "gbFileOps.h"
#include "gbIgnore.h"
#include "gbIndex.h"
#include "gbRelease.h"

static char const rcsid[] = "$Id: gbIgnore.c,v 1.6 2007/11/16 18:19:50 markd Exp $";

/* column indices in ignore.idx files */
#define IGIDX_ACC_COL       0
#define IGIDX_MODDATE_COL   1
#define IGIDX_SRCDB_COL     2
#define IGIDX_COMMENT_COL   3
#define IGIDX_NUM_COLS      4

/* size power of ignore entry hash */
#define IGNORE_HASH_SIZE   18

struct gbIgnore
/* table of accessions and moddates to ignore */
{
    struct hash* accHash;  /* hash by acc, NULL if none loaded */
};

static void parseRow(struct gbRelease* release, struct gbIgnore* ignore,
                     struct lineFile* lf, char **row)
/* read and parse a gbidx file record */
{
struct gbIgnoreAcc *igAcc;
struct hashEl *hel;
unsigned srcDb = 0;

/* determine srcDb, skip if it doesn't match release */
if (sameString(row[IGIDX_SRCDB_COL], "GenBank"))
    srcDb = GB_GENBANK;
else if (sameString(row[IGIDX_SRCDB_COL], "RefSeq"))
    srcDb = GB_REFSEQ;
else
    errAbort("ignored srcdb must be \"GenBank\" or \"RefSeq\", got \"%s\":"
             " %s:%u", row[IGIDX_SRCDB_COL], lf->fileName, lf->lineIx);
if (srcDb == release->srcDb)
    {
    if (ignore->accHash == NULL)
        ignore->accHash = hashNew(IGNORE_HASH_SIZE);

    hel = hashStore(ignore->accHash, row[IGIDX_ACC_COL]);
    lmAllocVar(ignore->accHash->lm, igAcc);
    igAcc->acc = hel->name;
    igAcc->modDate = gbParseDate(lf, row[IGIDX_MODDATE_COL]);
    igAcc->srcDb = srcDb;
    slAddHead((struct gbIgnoreAcc**)&hel->val, igAcc);
    }
}

static void parseIndex(struct gbRelease* release, struct gbIgnore* ignore,
                       char *path)
/* read and parse ignore file */
{
char* row[IGIDX_NUM_COLS];
struct lineFile* lf = lineFileOpen(path, TRUE);
while (lineFileNextRowTab(lf, row, ArraySize(row)))
    parseRow(release, ignore, lf, row);

lineFileClose(&lf);
}

struct gbIgnore* gbIgnoreNew(struct gbRelease* release)
/* Load the ignore index. */
{
static char *IGNORE_IDX = "etc/ignore.idx";
char ignoreIdx[PATH_LEN];
struct gbIgnore* ignore;
AllocVar(ignore);

safef(ignoreIdx, sizeof(ignoreIdx), "%s/%s", release->index->gbRoot,
      IGNORE_IDX);
if (fileExists(ignoreIdx))
    parseIndex(release, ignore, ignoreIdx);
return ignore;
}

void gbIgnoreFree(struct gbIgnore** ignorePtr)
/* Free gbIgnore object */
{
struct gbIgnore* ignore = *ignorePtr;
if (ignore != NULL)
    {
#ifdef DUMP_HASH_STATS
    if (ignore->accHash != NULL)
        hashPrintStats(ignore->accHash, "ignoreAcc", stderr);
#endif
    hashFree(&ignore->accHash);
    freeMem(ignore);
    *ignorePtr = NULL;
    }
}

struct gbIgnoreAcc* gbIgnoreGet(struct gbIgnore *ignore, char *acc,
                                time_t modDate)
/* Get he ignore entry for an accession and modedate, or NULL  */
{
if (ignore->accHash != NULL)
    {
    struct gbIgnoreAcc *igAcc = hashFindVal(ignore->accHash, acc);
    while (igAcc != NULL)
        {
        if (igAcc->modDate == modDate)
            return igAcc;
        igAcc = igAcc->next;
        }
    }
return NULL;
}

struct gbIgnoreAcc* gbIgnoreFind(struct gbIgnore *ignore, char *acc)
/* get the list of gbIgnore entries for an accession, or NULL */
{
if (ignore->accHash == NULL)
    return NULL;
else
    return hashFindVal(ignore->accHash, acc);
}

struct hashCookie gbIgnoreFirst(struct gbIgnore *ignore)
/* get cookie to iterate over hash */
{
// a zero cookie will return NULL on hashNext() call 
struct hashCookie cookie;
if (ignore->accHash == NULL)
    ZeroVar(&cookie);
else
    cookie = hashFirst(ignore->accHash);
return cookie;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

