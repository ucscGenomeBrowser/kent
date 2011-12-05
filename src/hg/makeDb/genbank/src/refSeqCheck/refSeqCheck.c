/* refSecCheck - check consistency of refseq tables for an assembly. */

#include "common.h"
#include "options.h"
#include "portable.h"
#include "gbDefs.h"
#include "psl.h"
#include "pslReader.h"
#include "genePred.h"
#include "genePredReader.h"
#include "hdb.h"
#include "jksql.h"
#include "hash.h"
#include "gbVerb.h"
#include "refSeqStatus.h"
#include "refLink.h"
#include "gbSeq.h"
#include "gbExtFile.h"


/**
 *  Note: 
 *   - this only checks starting with aligned sequences.
 *   - doesn't validate gbCdnaInfo
 */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"verbose", OPTION_INT},
    {NULL, 0}
};

static int gErrCount = 0;  // count of total errors
struct chkInfo;

void chkError(struct chkInfo *ci, char *acc, char *format, ...)
/* print and count an error */
#ifdef __GNUC__
__attribute__((format(printf, 3, 4)))
#endif
;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "refSeqCheck - check consistency of refseq tables for an assembly\n"
  "\n"
  "Usage:\n"
  "   refSeqCheck [options] database\n"
  "\n"
  "   o database - database to check\n"
  "\n"
  "Options:\n"
  "    -verbose=n - enable verbose output\n"
  "              n >= 1 - basic information abort each step\n"
  "              n >= 2 - more details\n"
  "              n >= 5 - SQL queries\n"
  "\n"
  );
}


struct chkInfo
/* information about structure being checked */
{
    struct sqlConnection *conn;  // connection to the database
    unsigned orgCat;             // organism category
    char *geneTable;             // name of gene table
    char *alignTable;            // name of alignment table
};

void chkError(struct chkInfo *ci, char *acc, char *format, ...)
/* print and count an error */
{
va_list args;
fprintf(stderr, "Error: %s: %s %s: ", sqlGetDatabase(ci->conn), gbOrgCatName(ci->orgCat), acc);
va_start(args, format);
vfprintf(stderr, format, args);
va_end(args);
fputc('\n', stderr);
gErrCount++;
}

static struct hash *loadGenePredTbl(struct sqlConnection *conn, char *tbl, struct hash *accTbl)
/* load a genePred table into a hash, indexed by accession.  Duplication annotations
 * are linked */
{
struct hash *gpTbl = hashNew(20);
struct genePred *gp, *allGp = genePredReaderLoadQuery(conn, tbl, NULL);
while ((gp = slPopHead(&allGp)) != NULL)
    {
    struct hashEl *hel = hashStore(gpTbl, gp->name);
    slAddHead((struct genePred**)&hel->val, gp);
    hashStore(accTbl, gp->name);
    }
return gpTbl;
}

static void freeGenePredTbl(struct hash **gpTblPtr)
/* free a hash of genePreds */
{
struct hash *gpTbl = *gpTblPtr;
if (gpTbl != NULL)
    {
    struct hashCookie hc = hashFirst(gpTbl);
    struct hashEl *hel;
    while ((hel = hashNext(&hc)) != NULL)
        genePredFreeList((struct genePred**)&hel->val);
    hashFree(gpTblPtr);
    }
}

static struct hash *loadPslTbl(struct sqlConnection *conn, char *tbl, struct hash *accTbl)
/* load a psl table into a hash, indexed by accession.  Duplication annotations
 * are linked */
{
struct hash *pslTbl = hashNew(20);
struct psl *psl, *allPsl = pslReaderLoadQuery(conn, tbl, NULL);
while ((psl = slPopHead(&allPsl)) != NULL)
    {
    struct hashEl *hel = hashStore(pslTbl, psl->qName);
    slAddHead((struct psl**)&hel->val, psl);
    hashStore(accTbl, psl->qName);
    }
return pslTbl;
}

static void freePslTbl(struct hash **pslTblPtr)
/* free a hash of psls */
{
struct hash *pslTbl = *pslTblPtr;
if (pslTbl != NULL)
    {
    struct hashCookie hc = hashFirst(pslTbl);
    struct hashEl *hel;
    while ((hel = hashNext(&hc)) != NULL)
        pslFreeList((struct psl**)&hel->val);
    hashFree(pslTblPtr);
    }
}

static void checkGbExtFile(struct chkInfo *ci, char *acc, char *refDesc, struct gbSeq *gbSeq)
/* validate an accession is in the gbExtFile table */
{
struct gbExtFile *gbExtFile = sqlQueryObjs(ci->conn, (sqlLoadFunc)gbExtFileLoad, 0,
                                           "SELECT * FROM gbExtFile WHERE id=%d", gbSeq->gbExtFile);
if (gbExtFile == NULL)
    chkError(ci, acc, "no gbExtFile entry %d, gbSeq %d, %s", gbSeq->gbExtFile, gbSeq->id, refDesc);
}

static void checkGbSeqExtFile(struct chkInfo *ci, char *acc, char *refDesc)
/* validate an accession is in the gbSeq and gbExtFile tables */
{
if (refDesc == NULL)
    refDesc = "";
struct gbSeq *gbSeq = sqlQueryObjs(ci->conn, (sqlLoadFunc)gbSeqLoad, sqlQueryMulti,
                                 "SELECT * FROM gbSeq WHERE acc=\"%s\"", acc);
if (gbSeq == NULL)
    chkError(ci, acc, "no gbSeq entry, %s", refDesc);
else if (slCount(gbSeq) > 1)
    chkError(ci, acc, "%d gbSeq entries, %s", slCount(gbSeq), refDesc);
else
    checkGbExtFile(ci, acc, refDesc, gbSeq);
gbSeqFreeList(&gbSeq);
}

static void checkRefSeqStatus(struct chkInfo *ci, char *acc)
/* validate a RefSeqStatus record */
{
struct refSeqStatus *rss = sqlQueryObjs(ci->conn, (sqlLoadFunc)refSeqStatusLoad, sqlQueryMulti,
                                        "SELECT * FROM refSeqStatus WHERE mrnaAcc=\"%s\"", acc);
if (rss == NULL)
    chkError(ci, acc, "no refSeqStatus entry");
else if (slCount(rss) > 1)
    chkError(ci, acc, "%d refSeqStatus entries", slCount(rss));
refSeqStatusFreeList(&rss);
}

static void checkRefLinkProtCoding(struct chkInfo *ci, char *acc, struct refLink *rl)
/* validate a RefLink record for protein coding */
{
if (isEmpty(rl->product))
    chkError(ci, acc, "coding has no product in refLink");
if (isEmpty(rl->protAcc))
    chkError(ci, acc, "coding has no protAcc in refLink");
if (rl->prodId == 0)
    chkError(ci, acc, "coding has no prodId in refLink");
if (!isEmpty(rl->protAcc))
    {
    char refDesc[256];
    safef(refDesc, sizeof(refDesc), "via refLink for %s", acc);
    checkGbSeqExtFile(ci, rl->protAcc, refDesc);
    }
}

static void checkRefLinkNonCoding(struct chkInfo *ci, char *acc, struct refLink *rl)
/* validate a RefLink record for non-protein coding */
{
if (!isEmpty(rl->product))
    chkError(ci, acc, "non-coding has product in refLink");
if (!isEmpty(rl->protAcc))
    chkError(ci, acc, "non-coding has protAcc in refLink");
if (rl->prodId != 0)
    chkError(ci, acc, "non-coding has prodId in refLink");
}

static void checkRefLink(struct chkInfo *ci, char *acc)
/* validate a RefLink record */
{
struct refLink *rl = sqlQueryObjs(ci->conn, (sqlLoadFunc)refLinkLoad, sqlQueryMulti,
                                  "SELECT * FROM refLink WHERE mrnaAcc=\"%s\"", acc);
if (rl == NULL)
    chkError(ci, acc, "no refLink entry");
else if (slCount(rl) > 1)
    chkError(ci, acc, "%d refLink entries", slCount(rl));
else if (startsWith("NM_", acc))
    checkRefLinkProtCoding(ci, acc, rl);
else
    checkRefLinkNonCoding(ci, acc, rl);
refLinkFreeList(&rl);
}

static void checkOneRefSeq(struct chkInfo *ci, char *acc, struct genePred *genes, struct psl *aligns)
/* check one refseq */
{
if (!(startsWith("NM_", acc) || startsWith("NR_", acc)))
    chkError(ci, acc, "does not start with NM_ or NR_, %s %s, %s %s",
             ((genes != NULL) ? "in" : "not in"), ci->geneTable,
             ((aligns != NULL) ? "in" : "not in"), ci->alignTable);

if (slCount(genes) != slCount(aligns))
    chkError(ci, acc, "has %d rows in %s and %d rows in %s", slCount(genes), ci->geneTable,  slCount(aligns), ci->alignTable);
checkGbSeqExtFile(ci, acc, "via alignments");
checkRefSeqStatus(ci, acc);
checkRefLink(ci, acc);
}

static void checkOrgCat(struct chkInfo *ci)
/* validate an refseq orgCat */
{
struct hash *accTbl = hashNew(20);
struct hash *gpTbl = loadGenePredTbl(ci->conn, ci->geneTable, accTbl);
struct hash *pslTbl = loadPslTbl(ci->conn, ci->alignTable, accTbl);
struct hashCookie accHc = hashFirst(accTbl);
struct hashEl *accHel;
while ((accHel = hashNext(&accHc)) != NULL)
    checkOneRefSeq(ci, accHel->name, hashFindVal(gpTbl, accHel->name), hashFindVal(pslTbl, accHel->name));

freeGenePredTbl(&gpTbl);
freePslTbl(&pslTbl);
}

static void maybeCheckOrgCat(struct sqlConnection *conn, unsigned orgCat,
                             char *geneTable, char *alignTable)
/* validate an refseq orgCat, if either of the positional tables are define (causing an error if
 * only one is defined) */
{
if (sqlTableExists(conn, geneTable) || sqlTableExists(conn, alignTable))
    {
    struct chkInfo chkInfo;
    chkInfo.conn = conn;
    chkInfo.orgCat = orgCat;
    chkInfo.geneTable = geneTable;
    chkInfo.alignTable = alignTable;
    checkOrgCat(&chkInfo);
    }
}

static void refSeqCheck(char *db)
/* Load the database with the ORFeome tables. */
{
struct sqlConnection *conn = hAllocConn(db);
maybeCheckOrgCat(conn, GB_NATIVE, "refGene", "refSeqAli");
maybeCheckOrgCat(conn, GB_XENO, "xenoRefGene", "xenoRefSeqAli");
hFreeConn(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
setlinebuf(stdout);
setlinebuf(stderr);
optionInit(&argc, argv, optionSpecs);
if (argc != 2)
    usage();
gbVerbInit(optionInt("verbose", 0));
if (gbVerbose >= 5)
    sqlMonitorEnable(JKSQL_TRACE);
refSeqCheck(argv[1]);
return (gErrCount == 0) ? 0 : 1;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
