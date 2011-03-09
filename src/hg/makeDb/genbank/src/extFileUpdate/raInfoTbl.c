/* table of seq extFileInfo from a ra file */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "sqlNum.h"
#include "gbDefs.h"
#include "gbVerb.h"
#include "ra.h"
#include "raInfoTbl.h"

struct raInfoTbl
/* table of seq extFileInfo from a ra file */
{
    struct hash *accMap;  /* acc.ver map */
};

static void raInfoAdd(struct raInfoTbl *rit, struct hash *raRec,
                      char *acc, short ver, char *seqSzFld, char *offFld, char *recSzFld,
                      unsigned extFileId)
/* add a ra mrna or pep */
{
struct hashEl *hel;
struct raInfo *ri;
char accVer[GB_ACC_BUFSZ];
if (extFileId == 0)
    errAbort("no extFileId for %s.%d", acc, ver); 
gbVerbPr(10, "raAdd %s.%d ext %d", acc, ver, extFileId);
lmAllocVar(rit->accMap->lm, ri);
safef(accVer, sizeof(accVer), "%s.%d", acc, ver);
hel = hashAdd(rit->accMap, accVer, ri);
ri->acc = lmCloneString(rit->accMap->lm, acc);
ri->version = ver;
ri->size = sqlUnsigned((char*)hashMustFindVal(raRec, seqSzFld));
ri->offset = sqlLongLong((char*)hashMustFindVal(raRec, offFld));
ri->fileSize = sqlUnsigned((char*)hashMustFindVal(raRec, recSzFld));
ri->extFileId = extFileId;
}

static boolean raRecLoad(struct raInfoTbl *rit, unsigned srcDb, struct lineFile *raLf,
                         unsigned cdnaExtId, unsigned pepExtId)
/* load next ra record */
{
char *acc, *protAccVer, protAcc[GB_ACC_BUFSZ];
int ver;
struct hash *raRec = raNextRecord(raLf);
if (raRec == NULL)
    return FALSE;
acc = hashMustFindVal(raRec, "acc");
ver = sqlSigned((char*)hashMustFindVal(raRec, "ver"));
raInfoAdd(rit, raRec, acc, ver, "siz", "fao", "fas", cdnaExtId);

if ((srcDb == GB_REFSEQ) && ((protAccVer = hashFindVal(raRec, "prt")) != NULL))
    {
    if (pepExtId == 0)
        errAbort("%s has protein %s, but no pep.fa file", acc, protAccVer);
    ver = gbSplitAccVer(protAccVer, protAcc);
    raInfoAdd(rit, raRec, protAcc, ver, "prs", "pfo", "pfs", pepExtId);
    }
#ifdef DUMP_HASH_STATS
hashPrintStats(raRec, "raRec", stderr);
#endif
hashFree(&raRec);
return TRUE;
}

struct raInfoTbl *raInfoTblNew()
/* construct a new raInfoTbl from a ra file */
{
struct raInfoTbl *rit;
AllocVar(rit);
rit->accMap = hashNew(23);

return rit;
}

void raInfoTblRead(struct raInfoTbl *rit, unsigned srcDb, char *raFile, unsigned cdnaExtId, unsigned pepExtId)
/* read a ra file into the table */
{
struct lineFile *raLf = lineFileOpen(raFile, TRUE);
while (raRecLoad(rit, srcDb, raLf, cdnaExtId, pepExtId))
    continue;

lineFileClose(&raLf);
}

struct raInfo *raInfoTblGet(struct raInfoTbl *rit, char *accVer)
/* look up an entry, or return null */
{
return (struct raInfo*)hashFindVal(rit->accMap, accVer);
}

struct raInfo *raInfoTblMustGet(struct raInfoTbl *rit, char *accVer)
/* look up an entry, or abort if not found */
{
struct raInfo *ri = raInfoTblGet(rit, accVer);
if (ri == NULL)
    errAbort("can't find \"%s\" in genbank ra files", accVer);
return ri;
}

void raInfoTblFree(struct raInfoTbl **ritPtr)
/* free a raInfoTbl object */
{
struct raInfoTbl *rit = *ritPtr;
if (rit != NULL)
    {
    hashFree(&rit->accMap);
    freeMem(rit);
    *ritPtr = NULL;
    }
}
