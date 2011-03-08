/* refPepRepair - repair refseq peptide links */
#include "common.h"
#include "jksql.h"
#include "gbDefs.h"
#include "gbVerb.h"
#include "gbFa.h"
#include "fa.h"
#include "extFileTbl.h"
#include "seqTbl.h"
#include "localmem.h"
#include "sqlDeleter.h"

static char const rcsid[] = "$Id: refPepRepair.c,v 1.11 2006/01/26 00:40:32 genbank Exp $";

/* N.B. 
 *  - The code for getting an ext file record is based on code in gbSanity/chkSeqTbl.c.
 *    It should be generalzied at some point.
 */

struct brokenRefPep
/* data about a refPep with broken extFile link.  protein acc+ver used in case
 * there ware multiple versions in fasta files */
{
    struct brokenRefPep *next;
    unsigned protSeqId;         /* gbSeq id of protein */
    char *protAcc;              /* protein acc (not alloced here) */
    short protVer;              /* protein version  */
    char mrnaAcc[GB_ACC_BUFSZ]; /* mRNA acc, or NULL if not in db */
    char newFaPath[PATH_LEN];   /* new path for fasta file */
    HGID newFaId;               /* file id of new file, or -1 if not found */
    off_t newFaOff;             /* new offset in fasta, or -1 if not found */
    unsigned newSeqSize;        /* sequence size from fasta */
    unsigned newRecSize;        /* record size in fasta */
};

struct brokenRefPepTbl
/* table of brokenRefPep objects, indexable by protAcc+version and file path */
{
    struct hash *protAccHash;     /* hash by protAcc */
    struct hash *protFaHash;      /* hash of protFa, doesn't point to anything. */
    int numToRepair;              /* number that need repaired */
    int numToDrop;                /* number that need dropped */
};

static struct extFile* extFileGet(struct extFileTbl* extFileTbl, char *acc, int extFileId)
/* get the extFile for an accession given it's id.  Return NULL if id is not
 * valid */
{
struct extFile* extFile = extFileTblFindById(extFileTbl, extFileId);
if (extFile == NULL)
    gbVerbMsg(3, "%s: gbExtFile id %d not in gbExtFile table", acc, extFileId);
return extFile;
}

/* Check a location of a sequence against bounds of a faFile */
static boolean extFileChkBounds(char *protAcc, struct extFile* extFile,
                                off_t faOff, unsigned recSize)
{
off_t faSize = fileSize(extFile->path);
if (faSize < 0)
    {
    gbVerbMsg(3, "%s: extFile does not exist or is not readable: %s", protAcc, extFile->path);
    return FALSE;
    }
if (faSize != extFile->size)
    {
    gbVerbMsg(3, "%s: extFile size (%lld) does match actual fasta file size (%lld): %s", protAcc, 
              (long long)extFile->size, (long long)faSize, extFile->path);
    return FALSE;
    }
if ((faOff+recSize) > faSize)
    {
    gbVerbMsg(3, "%s: fasta record end (%lld) does past end of (%lld): %s", protAcc, 
              (long long)(faOff+recSize), (long long)faSize, extFile->path);
    return FALSE;
    }
return TRUE;
}

/* Check a protein sequence, return FALSE if there is some reason it can't be
 * obtained or doesn't match */
static boolean faCheckProtRec(char *protAcc, short protVer, struct extFile* extFile,
                              off_t faOff, unsigned seqSize, unsigned recSize)
{
static const int extraBytes = 8;  /* extra bytes to read to allow checking next record */
int askSize = recSize+extraBytes;
int readSize;
char *faBuf, *p, gotAcc[GB_ACC_BUFSZ];
short gotVer;
struct dnaSeq *protSeq;
FILE *fh = mustOpen(extFile->path, "r");

/* bounds have already been check; so error if we can read the bytes */
if (fseeko(fh, faOff, SEEK_SET) < 0)
    errnoAbort("%s: can't seek to %lld in %s", protAcc, (long long)faOff, extFile->path);
faBuf = needMem(askSize+1);
readSize = fread(faBuf, 1, askSize, fh);
if (readSize < 0)
    errnoAbort("%s: read failed at %lld in %s", protAcc, (long long)faOff, extFile->path);
if (readSize < recSize)
    errAbort("%s: can't read %d bytes at %lld in %s", protAcc, recSize, (long long)faOff, extFile->path);
carefulClose(&fh);
faBuf[readSize] = '\0';

/* check that it starts with a '>' and that there are no extra bases after the
 * end of sequence */
if (faBuf[0] != '>')
    {
    gbVerbMsg(3, "%s: fasta record at %lld does not start with a '>': %s", protAcc, 
              (long long)faOff, extFile->path);
    freeMem(faBuf);
    return FALSE;
    }
p = skipLeadingSpaces(faBuf+recSize);
if (!((*p == '>') || (*p == '\0')))
    {
    gbVerbMsg(3, "%s: fasta record at %lld for %d has extra characters following the record: %s", protAcc, 
              (long long)faOff, recSize, extFile->path);
    freeMem(faBuf);
    return FALSE;
    }
protSeq = faSeqFromMemText(faBuf, FALSE);
gotVer = gbSplitAccVer(protSeq->name, gotAcc);
if (!(sameString(gotAcc, protAcc) && (gotVer == protVer)))
    {
    gbVerbMsg(3, "%s: expected sequence %s.%d, found %s.%d in fasta record at %lld : %s", protAcc,
              protAcc, protVer, gotAcc, gotVer, (long long)faOff, extFile->path);
    dnaSeqFree(&protSeq);
    return FALSE;
    }

if (protSeq->size != seqSize)
    {
    gbVerbMsg(3, "%s: expected sequence of %d chars, got %d from fasta record at %lld : %s", protAcc,
              seqSize, protSeq->size, (long long)faOff, extFile->path);
    dnaSeqFree(&protSeq);
    return FALSE;
    }

dnaSeqFree(&protSeq);
return TRUE;
}

static struct brokenRefPep *brokenRefPepObtain(struct brokenRefPepTbl *brpTbl,
                                               char *protAcc, int protSeqId,
                                               short protVer)
/* get a brokenRefPep object if it exists, or create a new one. protSeqId
 * is 0 if not known, protVer is -1 if not known.*/
{
struct hashEl *hel = hashStore(brpTbl->protAccHash, protAcc);
struct brokenRefPep *brp = hel->val;
if (brp == NULL)
    {
    lmAllocVar(brpTbl->protAccHash->lm, brp);
    hel->val = brp;
    brp->protSeqId = protSeqId;
    brp->protAcc = hel->name;
    brp->protVer = protVer;
    brp->newFaId = -1;
    brp->newFaOff = -1;
    }
else
    {
    /* exists, update ids if needed */
    if ((protSeqId > 0) && (brp->protSeqId > 0) && (protSeqId != brp->protSeqId))
        errAbort("%s protSeqId mismatch", protAcc);
    if (brp->protSeqId == 0)
        brp->protSeqId = protSeqId;
    if ((protVer >= 0) && (brp->protVer >= 0) && (protVer != brp->protVer))
        errAbort("%s protVer mismatch", protAcc);
    if (brp->protVer < 0)
        brp->protVer = protVer;
    }
return brp;
}

static void brokenRefPepGetBrokenLinks(struct sqlConnection *conn,
                                       struct brokenRefPepTbl *brpTbl)
/* load refSeq peps that are not linked to gbSeq */
{
static char *query = "select gbSeq.id, gbSeq.acc, gbSeq.version from gbSeq "
    "left join gbExtFile on gbSeq.gbExtFile=gbExtFile.id "
    "where ((gbSeq.acc like \"NP__%\") or (gbSeq.acc like \"YP__%\")) "
    "and (gbExtFile.id is NULL)";
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;

while ((row = sqlNextRow(sr)) != NULL)
    brokenRefPepObtain(brpTbl, row[1], sqlUnsigned(row[0]), sqlUnsigned(row[2]));

sqlFreeResult(&sr);
}

static void brokenRefPepLoadAcc(struct sqlConnection *conn,
                                struct brokenRefPepTbl *brpTbl,
                                char *acc)
/* load a brokenRefPep for the specified acc */
{
char query[512];
struct sqlResult *sr;
char **row;

if (!(startsWith("NP_", acc) || startsWith("YP_", acc)))
    errAbort("%s is not a RefSeq protein accession", acc);

safef(query, sizeof(query),
      "select gbSeq.id, gbSeq.acc, gbSeq.version from gbSeq "
      "where acc=\"%s\"", acc);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    fprintf(stderr, "Warning: %s not found in gbSeq\n", acc);
else
    brokenRefPepObtain(brpTbl, row[1], sqlUnsigned(row[0]), sqlUnsigned(row[2]));
sqlFreeResult(&sr);
}

static void brokenRefPepLoad(struct sqlConnection *conn,
                             struct brokenRefPepTbl *brpTbl,
                             struct slName *accs)
/* load refSeq peps that are listed in a file */
{
struct slName *acc;
for (acc = accs; acc != NULL; acc = acc->next)
    brokenRefPepLoadAcc(conn, brpTbl, acc->name);
}

static void brokenRefPepSeqCheck(struct sqlConnection *conn,
                                 struct extFileTbl* extFileTbl,
                                 struct brokenRefPepTbl *brpTbl,
                                 int protSeqId, char *protAcc, short protVer,
                                 unsigned seqSize, int extFileId, off_t faOff,
                                 unsigned recSize)
/* check peptide from seq table, add to broken list if there are problems.
 * check if in extFile, file exists and bounds are sane, and it points to
 * a sequence. */
 {
struct extFile* extFile = extFileGet(extFileTbl, protAcc, extFileId);
if ((extFile == NULL)
    || !extFileChkBounds(protAcc, extFile, faOff, recSize)
    || !faCheckProtRec(protAcc, protVer, extFile, faOff, seqSize, recSize))
    {
    /* add to table */
    brokenRefPepObtain(brpTbl, protAcc, protSeqId, protVer);
    }
}

static void brokenRefPepGetSeqScan(struct sqlConnection *conn,
                                   struct extFileTbl* extFileTbl,
                                   struct brokenRefPepTbl *brpTbl)
/* load refSeq peps that have seq or extFile problems, including
 * checking fasta file contents*/
{
static char *query = "select id, acc, version, size, gbExtFile, file_offset, file_size "
    "from gbSeq where (acc like \"NP__%\") or (acc like \"YP__%\")";
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;

while ((row = sqlNextRow(sr)) != NULL)
    brokenRefPepSeqCheck(conn, extFileTbl, brpTbl,
                         sqlSigned(row[0]), row[1], sqlSigned(row[2]),
                         sqlUnsigned(row[3]), sqlUnsigned(row[4]),
                         sqlLongLong(row[5]), sqlUnsigned(row[6]));
sqlFreeResult(&sr);
}


static void saveProtFastaPath(struct brokenRefPepTbl* brpTbl,
                              struct brokenRefPep* brp,
                              char *mrnaAcc, char *mrnaFa)
/* save protein fasta file path; mangles mrnaFa string */
{
struct hashEl *hel;
char protFa[PATH_LEN];
safef(brp->mrnaAcc, sizeof(brp->mrnaAcc), "%s", mrnaAcc);
chopSuffixAt(mrnaFa, '/');
safef(protFa, sizeof(protFa), "%s/pep.fa", mrnaFa);
hel = hashStore(brpTbl->protFaHash, protFa);
safef(brp->newFaPath, sizeof(brp->newFaPath), "%s", protFa);
brpTbl->numToRepair++;
}

static void brokenRefPepGetPath(struct sqlConnection *conn,
                                struct brokenRefPepTbl *brpTbl,
                                struct brokenRefPep* brp)
/* get path information for one broken refPeps from mrna in reflink.  This
 * saves the path in the brp and also saves the path in protFaHash so they can
 * be scanned */
{
char query[512], **row;
struct sqlResult *sr;

safef(query, sizeof(query),
      "select refLink.mrnaAcc, gbExtFile.path "
      "from refLink,gbSeq,gbExtFile "
      "where refLink.protAcc=\"%s\" and gbSeq.acc=refLink.mrnaAcc and gbSeq.gbExtFile=gbExtFile.id",
      brp->protAcc);
sr= sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    saveProtFastaPath(brpTbl, brp, row[0], row[1]);
else
    brpTbl->numToDrop++;
sqlFreeResult(&sr);
}

static void brokenRefPepGetMrnas(struct sqlConnection *conn,
                                 struct brokenRefPepTbl *brpTbl)
/* load mrna information for broken refPeps */
{
struct hashCookie cookie = hashFirst(brpTbl->protAccHash);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    brokenRefPepGetPath(conn, brpTbl, (struct brokenRefPep*)hel->val);
}

static struct brokenRefPepTbl *brokenRefPepTblNew(struct sqlConnection *conn,
                                                  struct slName *accs)
/* construct a brokenRefPepTbl object, loading data from database, optionally
 * using a list of accessions instead of finding them */
{
struct brokenRefPepTbl *brpTbl;
AllocVar(brpTbl);
brpTbl->protAccHash = hashNew(21);
brpTbl->protFaHash = hashNew(18);

if (accs != NULL)
    brokenRefPepLoad(conn, brpTbl, accs);
else
    brokenRefPepGetBrokenLinks(conn, brpTbl);
return brpTbl;
}

static void brokenRefPepTblFree(struct brokenRefPepTbl **brpTblPtr)
/* free brokenRefPepTbl */
{
struct brokenRefPepTbl *brpTbl = *brpTblPtr;
/* all dynamic memory is hash localmem */
#ifdef DUMP_HASH_STATS
hashPrintStats(brpTbl->protAccHash, "protAcc", stderr);
hashPrintStats(brpTbl->protFaHash, "protFa", stderr);
#endif
hashFree(&brpTbl->protAccHash);
hashFree(&brpTbl->protFaHash);
freeMem(brpTbl);
*brpTblPtr = NULL;
}

static void getFastaOffsets(struct brokenRefPepTbl *brpTbl,
                            struct sqlConnection *conn,
                            struct extFileTbl* extFileTbl,
                            char *faPath)
/* parse fasta file to get offsets of proteins */
{
struct gbFa *fa = gbFaOpen(faPath, "r");
char acc[GB_ACC_BUFSZ];
struct brokenRefPep *brp;
HGID extId = extFileTblGet(extFileTbl, conn, faPath);

gbVerbMsg(5, "scanning fasta: %s", faPath);
while (gbFaReadNext(fa))
    {
    gbVerbMsg(5, "   %s: %lld", fa->id, (long long)fa->recOff);
    /* save only if same acecss, version, and file (to match mrna fa) */
    short ver = gbSplitAccVer(fa->id, acc);
    brp = hashFindVal(brpTbl->protAccHash, acc);
    if ((brp != NULL) && (ver == brp->protVer) && sameString(faPath, brp->newFaPath))
        {
        gbFaGetSeq(fa); /* force read of sequence data */
        brp->newFaId = extId;
        brp->newFaOff = fa->recOff;
        brp->newSeqSize = fa->seqLen;
        brp->newRecSize = fa->off-fa->recOff;
        gbVerbMsg(5, "      save: %s %lld for %lld\n", fa->id, (long long)fa->recOff, (long long)fa->off);
        }
    }
gbFaClose(&fa);
}

static void fillInFastaOffsets(struct brokenRefPepTbl *brpTbl,
                               struct sqlConnection *conn,
                               struct extFileTbl* extFileTbl)
/* get offsets of proteins in fasta files */
{
struct hashCookie cookie = hashFirst(brpTbl->protFaHash);
struct hashEl *hel;

/* fill in */
while ((hel = hashNext(&cookie)) != NULL)
    getFastaOffsets(brpTbl, conn, extFileTbl, hel->name);

/* check if any missing */
cookie = hashFirst(brpTbl->protAccHash);
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct brokenRefPep *brp = hel->val;
    if (strlen(brp->mrnaAcc) && (brp->newFaOff < 0))
        {
        /* in one case, this was a pseudoGene mistakenly left in as an
         * mRNA, so make it a warning */
        fprintf(stderr, "Warning: %s: refPep %s (for %s) not found in %s\n",
                sqlGetDatabase(conn), brp->protAcc, brp->mrnaAcc,
                brp->newFaPath);
        }
    }
}

static void refPepRepairOne(struct sqlConnection *conn,
                            struct brokenRefPep *brp,
                            struct seqTbl* seqTbl,
                            struct extFileTbl* extFileTbl,
                            boolean dryRun)
/* repair a refPep */
{
gbVerbPrStart(2, "%s\t%s\trepair", sqlGetDatabase(conn), brp->protAcc);
gbVerbPrMore(3, "\tsz: %d\toff: %lld\trecSz: %d\tfid: %d\t%s",
             brp->newSeqSize, (long long)brp->newFaOff, brp->newRecSize,
             brp->newFaId, brp->newFaPath);
gbVerbPrMore(2, "\n");
if (!dryRun)
    seqTblMod(seqTbl, brp->protSeqId, -1, brp->newFaId,
              brp->newSeqSize, brp->newFaOff, brp->newRecSize);
}

static void refPepDropOne(struct sqlConnection *conn,
                          struct brokenRefPep *brp,
                          struct sqlDeleter* seqTblDeleter,
                          boolean dryRun)
/* drop a refPep */
{
gbVerbPr(2, "%s\t%s\tdrop", sqlGetDatabase(conn), brp->protAcc);
if (!dryRun)
    sqlDeleterAddAcc(seqTblDeleter, brp->protAcc);
}

static void makeRepairs(struct brokenRefPepTbl *brpTbl,
                        struct sqlConnection *conn,
                        struct extFileTbl* extFileTbl,
                        boolean dryRun)
/* make repairs once data is collected */
{
static char *tmpDir = "/var/tmp";
struct hashCookie cookie;
struct hashEl *hel;
int repairCnt = 0;
int dropCnt = 0;
struct seqTbl* seqTbl = seqTblNew(conn, tmpDir, (gbVerbose > 3));
struct sqlDeleter* seqTblDeleter = sqlDeleterNew(tmpDir, (gbVerbose > 3));

cookie = hashFirst(brpTbl->protAccHash);
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct brokenRefPep *brp = hel->val;
    if ((brp->mrnaAcc != NULL) && (brp->newFaOff >= 0))
        {
        refPepRepairOne(conn, brp, seqTbl, extFileTbl, dryRun);
        repairCnt++;
        }
    else
        {
        refPepDropOne(conn, brp, seqTblDeleter, dryRun);
        dropCnt++;
        }
    }
if (dryRun)
    {
    gbVerbMsg(1, "%s: would have repaired %d refseq protein gbExtFile entries", 
              sqlGetDatabase(conn), repairCnt);
    gbVerbMsg(1, "%s: would have dropped %d refseq protein gbExtFile entries", 
              sqlGetDatabase(conn), dropCnt);
    }
else
    {
    seqTblCommit(seqTbl, conn);
    gbVerbMsg(1, "%s: repaired %d refseq protein gbExtFile entries",
              sqlGetDatabase(conn), repairCnt);
    sqlDeleterDel(seqTblDeleter, conn, SEQ_TBL, "acc");
    gbVerbMsg(1, "%s: dropped %d refseq protein gbExtFile entries",
              sqlGetDatabase(conn), dropCnt);
    }
}

static boolean checkForRefLink(struct sqlConnection *conn)
/* check if there is a refLink table, if none, issue message and return
 * false */
{
if (!sqlTableExists(conn, "refLink"))
    {
    fprintf(stderr, "Note: %s: no RefSeqs loaded, skipping",
            sqlGetDatabase(conn));
    return FALSE;
    }
else
    return TRUE;
}

void refPepList(char *db,
                FILE* outFh)
/* list of sequences needing repair */
{
struct sqlConnection *conn = sqlConnect(db);
struct brokenRefPepTbl *brpTbl;
struct hashCookie cookie;
struct hashEl *hel;
struct extFileTbl* extFileTbl = NULL;

if (!checkForRefLink(conn))
    {
    sqlDisconnect(&conn);
    return;
    }

extFileTbl = extFileTblLoad(conn);
brpTbl = brokenRefPepTblNew(conn, NULL);
brokenRefPepGetSeqScan(conn, extFileTbl, brpTbl);
brokenRefPepGetMrnas(conn, brpTbl);
extFileTblFree(&extFileTbl);

cookie = hashFirst(brpTbl->protAccHash);
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct brokenRefPep *brp = hel->val;
    fprintf(outFh, "%s\t%s\t%s\n", sqlGetDatabase(conn), brp->protAcc, (brp->mrnaAcc != NULL)? "repair" : "drop");
    }
gbVerbMsg(1, "%s: need to repair %d refseq protein gbExtFile entries",
          sqlGetDatabase(conn), brpTbl->numToRepair);
gbVerbMsg(1, "%s: need to drop %d refseq protein gbExtFile entries",
          sqlGetDatabase(conn), brpTbl->numToDrop);
}

void refPepRepair(char *db,
                  char *accFile,
                  boolean dryRun)
/* fix dangling repPep gbSeq entries. */
{
struct sqlConnection *conn = sqlConnect(db);
struct brokenRefPepTbl *brpTbl;
struct extFileTbl* extFileTbl;
struct slName *accs = (accFile == NULL) ? NULL : slNameLoadReal(accFile);
if (!checkForRefLink(conn))
    {
    sqlDisconnect(&conn);
    return;
    }

gbVerbMsg(1, "%s: repairing refseq protein gbExtFile entries%s",
          sqlGetDatabase(conn), (dryRun? " (dry run)" : ""));

extFileTbl = extFileTblLoad(conn);
brpTbl = brokenRefPepTblNew(conn, accs);
brokenRefPepGetSeqScan(conn, extFileTbl, brpTbl);
brokenRefPepGetMrnas(conn, brpTbl);

fillInFastaOffsets(brpTbl, conn, extFileTbl);
if (brpTbl->numToRepair > 0)
    makeRepairs(brpTbl, conn, extFileTbl, dryRun);
else
    gbVerbMsg(1, "%s: no refseq proteins to repair", sqlGetDatabase(conn));
brokenRefPepTblFree(&brpTbl);
extFileTblFree(&extFileTbl);
sqlDisconnect(&conn);
slFreeList(&accs);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


