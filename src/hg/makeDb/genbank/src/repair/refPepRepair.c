/* refPepRepair - repair refseq peptide links */
#include "common.h"
#include "jksql.h"
#include "gbDefs.h"
#include "gbVerb.h"
#include "gbFa.h"
#include "extFileTbl.h"
#include "seqTbl.h"

static char const rcsid[] = "$Id: refPepRepair.c,v 1.3 2006/01/14 07:59:30 markd Exp $";

struct brokenRefPep
/* data about a refPep with broken extFile link */
{
    struct brokenRefPep *next;
    unsigned protSeqId;     /* gbSeq id of protein */
    char *protAcc;          /* protein acc (not alloced here) */
    char *mrnaAcc;          /* mRNA acc, or NULL if not in db */
    char *protPathNew;      /* new path derived from mrna path, NULL if
                             * mRNA not found (not alloced here) */
    off_t fileOff;          /* new offset in fasta, or -1 if not found in
                             * fasta */
    unsigned seqSize;       /* sequence size from fasta */
    unsigned recSize;       /* record size in fasta */
};

struct brokenRefPepTbl
/* table of brokenRefPep objects, indexable by protAcc and file path */
{
    struct hash *protAccHash;     /* hash by protAcc */
    struct hash *protPathHash;    /* hash of protPathNew, doesn't point
                                   * to anything. */
    int numToRepair;              /* number that need repaired */
};

static struct brokenRefPep *brokenRefPepNew(struct brokenRefPepTbl *brpTbl,
                                            unsigned protSeqId,
                                            char *protAcc)
/* construct a brokenRefPep object and add to the table */
{
struct brokenRefPep *brp;
struct hashEl *hel = hashStore(brpTbl->protAccHash, protAcc);
if (hel->val != NULL)
    errAbort("dup refPep: %s", protAcc);
AllocVar(brp);
hel->val = brp;
brp->protSeqId = protSeqId;
brp->protAcc = hel->name;
brp->fileOff = -1;
return brp;
}

static void brokenRefPepGetBroken(struct sqlConnection *conn,
                                  struct brokenRefPepTbl *brpTbl)
/* load refSeq peps that are not linked to gbSeq */
{
static char *query = "select gbSeq.id, gbSeq.acc from gbSeq "
    "left join gbExtFile on gbSeq.gbExtFile=gbExtFile.id "
    "where ((gbSeq.acc like \"NP__%\") or (gbSeq.acc like \"YP__%\")) "
    "and (gbExtFile.id is NULL)";
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;

while ((row = sqlNextRow(sr)) != NULL)
    brokenRefPepNew(brpTbl, sqlUnsigned(row[0]), row[1]);

sqlFreeResult(&sr);
}

static void brokenRefPepGetPath(struct sqlConnection *conn,
                                struct brokenRefPepTbl *brpTbl,
                                struct brokenRefPep* brp)
/* get path information for one broken refPeps from mrna */
{
char query[512], **row;
struct sqlResult *sr;
safef(query, sizeof(query),
      "select refLink.mrnaAcc, gbExtFile.path "
      "from refLink,gbSeq,gbExtFile "
      "where refLink.protAcc=\"%s\" and gbSeq.acc=refLink.mrnaAcc and gbSeq.gbExtFile=gbExtFile.id",
      brp->protAcc);
sr= sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    gbVerbPr(1, "Note: no mRNA found for refPep %s", brp->protAcc); 
else
    {
    struct hashEl *hel;
    char protPath[PATH_LEN];
    char *mrnaPath = row[1];
    chopSuffixAt(mrnaPath, '/');
    safef(protPath, sizeof(protPath), "%s/pep.fa", mrnaPath);
    hel = hashStore(brpTbl->protPathHash, protPath);
    brp->protPathNew = hel->name;
    brp->mrnaAcc = cloneString(row[0]);
    brpTbl->numToRepair++;
    }
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

static struct brokenRefPepTbl *brokenRefPepTblNew(struct sqlConnection *conn)
/* construct a brokenRefPepTbl object, loading data from database  */
{
struct brokenRefPepTbl *brpTbl;
AllocVar(brpTbl);
brpTbl->protAccHash = hashNew(21);
brpTbl->protPathHash = hashNew(18);

brokenRefPepGetBroken(conn, brpTbl);
brokenRefPepGetMrnas(conn, brpTbl);
return brpTbl;
}

static void getFastaOffsets(struct brokenRefPepTbl *brpTbl,
                            char *faPath)
/* parse fasta file to get offsets of proteins */
{
struct gbFa *fa = gbFaOpen(faPath, "r");
char acc[GB_ACC_BUFSZ];
struct brokenRefPep *brp;

while (gbFaReadNext(fa))
    {
    gbSplitAccVer(fa->id, acc);
    brp = hashFindVal(brpTbl->protAccHash, acc);
    if (brp != NULL)
        {
        gbFaGetSeq(fa); /* force read of sequence data */
        brp->fileOff = fa->recOff;
        brp->seqSize = fa->seqLen;
        brp->recSize = fa->off-fa->recOff;
        }
    }
gbFaClose(&fa);
}

static void fillInFastaOffsets(struct sqlConnection *conn,
                               struct brokenRefPepTbl *brpTbl)
/* get offsets of proteins in fasta files */
{
struct hashCookie cookie = hashFirst(brpTbl->protPathHash);
struct hashEl *hel;

/* fill in */
while ((hel = hashNext(&cookie)) != NULL)
    getFastaOffsets(brpTbl, hel->name);

/* check if any missing */
cookie = hashFirst(brpTbl->protAccHash);
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct brokenRefPep *brp = hel->val;
    if ((brp->mrnaAcc != NULL) && (brp->fileOff < 0))
        {
        /* in one case, this was a pseudoGene mistakenly left in as an
         * mRNA, so make it a warning */
        fprintf(stderr, "Warning: %s: refPep %s (for %s) not found in %s\n",
                sqlGetDatabase(conn), brp->protAcc, brp->mrnaAcc,
                brp->protPathNew);
        }
    }
}

static void refPepRepairOne(struct sqlConnection *conn,
                            struct brokenRefPep *brp,
                            struct seqTbl* seqTbl,
                            struct extFileTbl* extFileTbl)
/* repair a refPep */
{
HGID newFaId = extFileTblGet(extFileTbl, conn, brp->protPathNew);
seqTblMod(seqTbl, brp->protSeqId, 0, newFaId,
          brp->seqSize, brp->fileOff, brp->recSize);
}

static void makeRepairs(struct sqlConnection *conn,
                        struct brokenRefPepTbl *brpTbl,
                        boolean dryRun)
/* make repairs once data is collected */
{
struct hashCookie cookie;
struct hashEl *hel;
int repairCnt = 0;
struct seqTbl* seqTbl = seqTblNew(conn, "/var/tmp", (gbVerbose > 2));
struct extFileTbl* extFileTbl = extFileTblLoad(conn);

cookie = hashFirst(brpTbl->protAccHash);
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct brokenRefPep *brp = hel->val;
    if (brp->fileOff >= 0)
        {
        gbVerbPr(2, "repair %s", brp->protAcc);
        if (!dryRun)
            {
            refPepRepairOne(conn, brp, seqTbl, extFileTbl);
            repairCnt++;
            }
        }
    }
if (dryRun)
    gbVerbMsg(1, "%s: would have repaired %d refseq protein gbExtFile entries", 
              sqlGetDatabase(conn), repairCnt);
else
    {
    seqTblCommit(seqTbl, conn);
    gbVerbMsg(1, "%s: repaired %d refseq protein gbExtFile entries",
              sqlGetDatabase(conn), repairCnt);
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

void refPepList(struct sqlConnection *conn,
                FILE* outFh)
/* list of sequences needing repair */
{
struct brokenRefPepTbl *brpTbl;
struct hashCookie cookie;
struct hashEl *hel;

if (!checkForRefLink(conn))
    return;

brpTbl = brokenRefPepTblNew(conn);
cookie = hashFirst(brpTbl->protAccHash);

while ((hel = hashNext(&cookie)) != NULL)
    {
    struct brokenRefPep *brp = hel->val;
    fprintf(outFh, "%s\n", brp->protAcc);
    }
gbVerbMsg(1, "%s: need to repair %d refseq protein gbExtFile entries",
          sqlGetDatabase(conn), brpTbl->numToRepair);
}

void refPepRepair(struct sqlConnection *conn,
                  boolean dryRun)
/* fix dangling repPep gbSeq entries. */
{
struct brokenRefPepTbl *brpTbl;
if (!checkForRefLink(conn))
    return;

gbVerbMsg(1, "%s: repairing refseq protein gbExtFile entries%s",
          sqlGetDatabase(conn), (dryRun? " (dry run)" : ""));

brpTbl = brokenRefPepTblNew(conn);
fillInFastaOffsets(conn, brpTbl);
if (brpTbl->numToRepair > 0)
    makeRepairs(conn, brpTbl, dryRun);
else
    gbVerbMsg(1, "%s: no refseq proteins to repair", sqlGetDatabase(conn));
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


