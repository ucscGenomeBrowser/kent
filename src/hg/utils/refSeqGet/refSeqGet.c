/* refSeqGet - retrieve refseq data from the database. */
#include "common.h"
#include "refSeqVerInfo.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "psl.h"
#include "pslReader.h"
#include "genePred.h"
#include "genePredReader.h"
#include "genbank.h"
#include "fa.h"

static char const rcsid[] = "$Id: refSeqGet.c,v 1.3 2010/01/21 20:43:23 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "refSeqGet - retrieve refseq data from the database\n"
  "usage:\n"
  "   refSeqGet db\n"
  "\n"
  "Get a consistent snapshot of the RefSeq data from a genome browser database.\n"
  "All accessions will include version numbers.  Only ReqSeqs for the organism\n"
  "associated with the specified genome assembly are retrieved.\n"
  "\n"
  "options:\n"
  "   -aligns=pslFile - get the PSL alignments of the RefSeqs to the genome.\n"
  "   -geneAnns=genePred - get the genome annotations in genePred format.\n"
  "   -rnaSeqs=mrnaFa - get a FASTA file of the mRNA/RNA sequences with the CDS capitalized.\n"
  "   -protSeqs=protFa - get a FASTA file of the protein sequences.\n"
  "   -metaData=metaTsv - get a tab-separated file of the meta-data for the RefSeqs\n"
  "   -getNM - get NM sequences, if neither -getNM or -getNR is specified, all are return\n"
  "   -getNR - get the NR sequences\n"
  "   -accList=accfile - only get data for this set of accessions.  If version is included,\n"
  "    then only the specified version is retrieved, otherwise the version loaded is\n"
  "    retrieved.  This is incompatible with -getNR or -inclNM\n");
}

/* Notes:
 * This isn't very fast, it was done for simplicity more than speed.
 */

static struct optionSpec options[] = {
    {"aligns", OPTION_STRING},
    {"geneAnns", OPTION_STRING},
    {"rnaSeqs", OPTION_STRING},
    {"protSeqs", OPTION_STRING},
    {"metaData", OPTION_STRING},
    {"getNM", OPTION_BOOLEAN},
    {"getNR", OPTION_BOOLEAN},
    {"accList", OPTION_STRING},
    {NULL, 0},
};

static char *addVer(char *acc, int ver, char *buf, int bufSize)
/* construct acc.ver in buffer */
{
safef(buf, bufSize, "%s.%d", acc, ver);
return buf;
}

static void processPsl(FILE *fh, struct hash *refSeqVerInfoTbl, struct psl *psl)
/* check if a psl has been select, if so, write including version in qName */
{
struct refSeqVerInfo *rsvi = hashFindVal(refSeqVerInfoTbl, psl->qName);
if (rsvi != NULL)
    {
    char buf[GENBANK_ACC_BUFSZ], *hold = psl->qName;
    psl->qName = addVer(psl->qName, rsvi->ver, buf, sizeof(buf));
    pslTabOut(psl, fh);
    psl->qName = hold;
    }
}

static void getAligns(struct sqlConnection *conn, struct hash *refSeqVerInfoTbl, char *outFile)
/* get request alignments from database */
{
struct psl *psls = pslReaderLoadQuery(conn, "refSeqAli", NULL);
slSort(psls, pslCmpQuery);
FILE *fh = mustOpen(outFile, "w");
struct psl *psl;
for (psl = psls; psl != NULL; psl = psl->next)
    processPsl(fh, refSeqVerInfoTbl, psl);
carefulClose(&fh);
pslFreeList(&psls);
}

static void processGenePred(FILE *fh, struct hash *refSeqVerInfoTbl, struct genePred *gp)
/* check if a genePred has been select, if so, write including version in name */
{
struct refSeqVerInfo *rsvi = hashFindVal(refSeqVerInfoTbl, gp->name);
if (rsvi != NULL)
    {
    char buf[GENBANK_ACC_BUFSZ], *hold = gp->name;
    gp->name = addVer(gp->name, rsvi->ver, buf, sizeof(buf));
    genePredTabOut(gp, fh);
    gp->name = hold;
    }
}

static void getGeneAnns(struct sqlConnection *conn, struct hash *refSeqVerInfoTbl, char *outFile)
/* get request genePred annotations from database */
{
struct genePred *gps = genePredReaderLoadQuery(conn, "refGene", NULL);
slSort(&gps, genePredNameCmp);
FILE *fh = mustOpen(outFile, "w");
struct genePred *gp;
for (gp = gps; gp != NULL; gp = gp->next)
    processGenePred(fh, refSeqVerInfoTbl, gp);
carefulClose(&fh);
genePredFreeList(&gps);
}

static void processRnaSeq(FILE *fh, struct sqlConnection *conn, struct refSeqVerInfo *rsvi)
/* get an RNA sequence, which already includes version in name */
{
struct dnaSeq *seq = hGenBankGetMrnaC(conn, rsvi->acc, NULL);
if (seq == NULL)
    errAbort("failed to get %s from database", rsvi->acc);
faWriteNext(fh, seq->name, seq->dna, seq->size);
dnaSeqFree(&seq);
}

static void getRnaSeqs(struct sqlConnection *conn, struct refSeqVerInfo *refSeqVerInfoLst, char *outFile)
/* get request RNA sequences from database */
{
FILE *fh = mustOpen(outFile, "w");
struct refSeqVerInfo *rsvi;
for (rsvi = refSeqVerInfoLst; rsvi != NULL; rsvi = rsvi->next)
    processRnaSeq(fh, conn, rsvi);
carefulClose(&fh);
}

static void processProtSeq(FILE *fh, struct sqlConnection *conn, struct refSeqVerInfo *rsvi, struct hash *doneProts)
/* get an protein sequence, which already includes version in name. Don't duplicate NPs */
{
char query[128];
safef(query, sizeof(query), "SELECT protAcc FROM refLink WHERE mrnaAcc = \"%s\"", rsvi->acc);
char *protAcc = sqlNeedQuickString(conn, query);
if (isNotEmpty(protAcc) && hashLookup(doneProts, protAcc) == NULL)
    {
    struct dnaSeq *seq = hGenBankGetPepC(conn, protAcc, NULL);
    if (seq == NULL)
        errAbort("failed to get %s from database", protAcc);
    faWriteNext(fh, seq->name, seq->dna, seq->size);
    dnaSeqFree(&seq);
    hashAdd(doneProts, protAcc, NULL);
    }
freeMem(protAcc);
}

static void getProtSeqs(struct sqlConnection *conn, struct refSeqVerInfo *refSeqVerInfoLst, char *outFile)
/* get request prot sequences from database */
{
struct hash *doneProts = hashNew(16);
FILE *fh = mustOpen(outFile, "w");
struct refSeqVerInfo *rsvi;
for (rsvi = refSeqVerInfoLst; rsvi != NULL; rsvi = rsvi->next)
    processProtSeq(fh, conn, rsvi, doneProts);
carefulClose(&fh);
hashFree(&doneProts);
}

static char *getProtAccVerIf(struct sqlConnection *conn, char *mrnaAcc, char *protAcc, char *buf, int bufSize)
/* if protAcc is not empty, get acc.version, otherwise return empty */
{
if (isEmpty(protAcc))
    return "";
else
    {
    int protVer = refSeqVerInfoGetVersion(protAcc, conn);
    if (protVer == 0)
        errAbort("no protein version entry for %s associated with %s", protAcc, mrnaAcc);
    safef(buf, bufSize, "%s.%d", protAcc, protVer);
    return buf;
    }
}

static char *getCds(struct sqlConnection *conn, char *acc)
/* get CDS for an NM, results should be freed */
{
char query[128];
safef(query, sizeof(query), "SELECT cds.name FROM gbCdnaInfo,cds WHERE (gbCdnaInfo.acc = \"%s\") AND (gbCdnaInfo.cds = cds.id)", acc);
return sqlNeedQuickString(conn, query);
}

static void processMetaData(FILE *fh, struct sqlConnection *conn, struct sqlConnection *conn2, struct refSeqVerInfo *rsvi)
/* get meta data for an accession */
{
boolean isCoding = genbankIsRefSeqCodingMRnaAcc(rsvi->acc);
char query[128];
safef(query, sizeof(query), "SELECT name,product,protAcc,locusLinkId FROM refLink WHERE mrnaAcc = \"%s\"", rsvi->acc);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = sqlNextRow(sr);
if (row == NULL)
    errAbort("no RefLink entry for %s", rsvi->acc);
char buf[32];
char *protAccVer = getProtAccVerIf(conn2, rsvi->acc, row[2], buf, sizeof(buf));
char *cds = isCoding ? getCds(conn2, rsvi->acc) : "";
fprintf(fh, "%s.%d\t%s\t%s\t%s\t%s\t%s\n", rsvi->acc, rsvi->ver, protAccVer, row[0], row[3], cds, row[1]);
sqlFreeResult(&sr);
if (isCoding)
    freeMem(cds);
}

static void getMetaData(struct sqlConnection *conn, struct refSeqVerInfo *refSeqVerInfoLst, char *outFile)
/* get request prot sequences from database */
{
struct sqlConnection *conn2 = sqlConnect(sqlGetDatabase(conn));
static char *hdr = "#mrnaAcc\t" "protAcc\t" "geneName\t" "ncbiGeneId\t" "cds\t" "product\n";
FILE *fh = mustOpen(outFile, "w");
fputs(hdr, fh);
struct refSeqVerInfo *rsvi;
for (rsvi = refSeqVerInfoLst; rsvi != NULL; rsvi = rsvi->next)
    processMetaData(fh, conn, conn2, rsvi);
carefulClose(&fh);
sqlDisconnect(&conn2);
}

static void refSeqGet(char *db, char *aligns, char *geneAnns, char *rnaSeqs, char *protSeqs,
                      char *metaData, boolean getNM, boolean getNR, char *accList)
/* refSeqGet - retrieve refseq data from the database. */
{
struct sqlConnection *conn = sqlConnect(db);
struct refSeqVerInfo *refSeqVerInfoLst = NULL;
struct hash *refSeqVerInfoTbl = (accList == NULL)
    ? refSeqVerInfoFromDb(conn, getNM, getNR, &refSeqVerInfoLst)
    : refSeqVerInfoFromFile(conn, accList, &refSeqVerInfoLst);
if (aligns != NULL)
    getAligns(conn, refSeqVerInfoTbl, aligns);
if (geneAnns != NULL)
    getGeneAnns(conn, refSeqVerInfoTbl, geneAnns);
if (rnaSeqs != NULL)
    getRnaSeqs(conn, refSeqVerInfoLst, rnaSeqs);
if (protSeqs != NULL)
    getProtSeqs(conn, refSeqVerInfoLst, protSeqs);
if (metaData != NULL)
    getMetaData(conn, refSeqVerInfoLst, metaData);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();

if (optionExists("accList"))
    {
    if (optionExists("getNM"))
        errAbort("can't specify both -accList and -getNM");
    if (optionExists("getNR"))
        errAbort("can't specify both -accList and -getNR");
    }
boolean getNM = optionExists("getNM");
boolean getNR = optionExists("getNR");
if (!(getNM || getNR))
    getNM = getNR = TRUE;

refSeqGet(argv[1],
          optionVal("aligns", NULL),
          optionVal("geneAnns", NULL),
          optionVal("rnaSeqs", NULL),
          optionVal("protSeqs", NULL),
          optionVal("metaData", NULL),
          getNM, getNR,
          optionVal("accList", NULL));
return 0;
}
