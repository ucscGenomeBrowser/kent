/* vcfToHgvs - Transform VCF variant calls into HGVS terms. */

/* Copyright (C) 2017 The Regents of the University of California
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "genbank.h"
#include "hdb.h"
#include "hgHgvs.h"
#include "hui.h"
#include "knetUdc.h"
#include "options.h"
#include "udc.h"
#include "variantProjector.h"
#include "vcf.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "vcfToHgvs - Transform VCF variant calls into HGVS terms\n"
  "usage:\n"
  "   vcfToHgvs db in.vcf out.tab\n"
  "options:\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote VCF files.\n"
  "                           Will create this directory if it does not exist\n"
  );
}

static boolean breakDelIns = FALSE;

/* Command line validation table. */
static struct optionSpec options[] = {
   {"udcDir", OPTION_STRING},
   {NULL, 0},
};

struct dnaSeq *hGetNmAccAndSeq(char *db, char *nmAccIn)
/* Return a cached sequence with ->name set to the versioned NM accession,
 * given a possibly unversioned NM accession, or NULL if not found.
 * db must never change.  Do not modify or free the returned sequence. */
{
static struct hash *hash = NULL;
static char *firstDb = NULL;
static boolean useNcbi = FALSE;
if (hash == NULL)
    {
    hash = hashNew(0);
    firstDb = cloneString(db);
    useNcbi = hDbHasNcbiRefSeq(db);
    }
if (!sameString(db, firstDb))
    errAbort("hGetNmAccAndSeq: only works for one db.  %s was passed in earlier, now %s.",
             firstDb, db);
struct dnaSeq *accSeq = hashFindVal(hash, nmAccIn);
if (accSeq == NULL)
    {
    if (useNcbi)
        {
        // nmAccIn is already versioned.
        accSeq = hDnaSeqGet(db, nmAccIn, "seqNcbiRefSeq", "extNcbiRefSeq");
        }
    else
        {
        accSeq = hGenBankGetMrna(db, nmAccIn, NULL);
        if (accSeq)
            {
            // Query to get versioned nmAcc and replace accSeq->name with it.
            char query[1024];
            struct sqlConnection *conn = hAllocConn(db);
            sqlSafef(query, sizeof(query),
                     "select concat(s.acc, concat('.', s.version)) "
                     "from %s s where s.acc = '%s';", gbSeqTable, nmAccIn);
            char nmAcc[1024];
            if (sqlQuickQuery(conn, query, nmAcc, sizeof(nmAcc)))
                {
                freeMem(accSeq->name);
                accSeq->name = cloneString(nmAcc);
                }
            hFreeConn(&conn);
            }
        }
    if (accSeq == NULL)
        // Store a dnaSeq with NULL name and seq so we don't waste time sql querying this again.
        accSeq = newDnaSeq(NULL, 0, NULL);
    hashAdd(hash, nmAccIn, accSeq);
    }
return (accSeq->name == NULL) ? NULL : accSeq;
}

struct dnaSeq *hGetNpAccAndSeq(char *db, char *nmAcc)
/* Return a cached protein sequence with ->name set to the versioned NP accession,
 * given a possibly unversioned NM accession, or NULL if not found.
 * db must never change.  Do not modify or free the returned sequence. */
{
static struct hash *hash = NULL;
static char *firstDb = NULL;
static boolean useNcbi = FALSE;
if (hash == NULL)
    {
    hash = hashNew(0);
    firstDb = cloneString(db);
    useNcbi = hDbHasNcbiRefSeq(db);
    }
if (!sameString(db, firstDb))
    errAbort("hGetNpAccAndSeq: only works for one db.  %s was passed in earlier, now %s.",
             firstDb, db);
struct dnaSeq *accSeq = hashFindVal(hash, nmAcc);
if (accSeq == NULL)
    {
    char query[1024];
    struct sqlConnection *conn = hAllocConn(db);
    if (useNcbi)
        {
        // One handy query will get both already-versioned NP accession and sequence:
        sqlSafef(query, sizeof(query),
                 "select p.name, p.seq from ncbiRefSeqLink l, ncbiRefSeqPepTable p "
                 "where l.mrnaAcc = '%s' and l.protAcc = p.name", nmAcc);
        struct slPair *accRawSeq = sqlQuickPairList(conn, query);
        if (accRawSeq)
            {
            char *seq = accRawSeq->val;
            accSeq = newDnaSeq(seq, strlen(seq), accRawSeq->name);
            slPairFree(&accRawSeq);
            }
        }
    else
        {
        // First get both unversioned and versioned NP acc:
        sqlSafef(query, sizeof(query),
                 "select l.protAcc, concat(l.protAcc, concat('.', s.version)) "
                 "from %s l, %s s where l.mrnaAcc = '%s' and l.protAcc = s.acc;",
                 refLinkTable, gbSeqTable, nmAcc);
        struct slPair *accAndVersionedAcc = sqlQuickPairList(conn, query);
        if (accAndVersionedAcc)
            {
            char *npAccNoVersion = accAndVersionedAcc->name;
            char *npAcc = accAndVersionedAcc->val;
            // Use unversioned acc to get pep sequence from gbSeq/gbExtfile/filesystem:
            accSeq = hGenBankGetPep(db, npAccNoVersion, NULL);
            if (accSeq)
                {
                // Replace unversioned acc
                freeMem(accSeq->name);
                accSeq->name = npAcc;
                }
            slPairFree(&accAndVersionedAcc);
            }
        }
    if (accSeq == NULL)
        // Store a dnaSeq with NULL name and seq so we don't waste time sql querying this again.
        accSeq = newDnaSeq(NULL, 0, NULL);
    hashAdd(hash, nmAcc, accSeq);
    hFreeConn(&conn);
    }
return (accSeq->name == NULL) ? NULL : accSeq;
}
void singleAlleleToHgvsOut(char *db, struct bed3 *gBed3, char *ref, char *alt, char *name,
                           char *vcfChrom, struct seqWindow *gSeqWin, struct psl *psl,
                           struct genbankCds *cds, struct dnaSeq *txSeq, FILE *f)
/* Output first 5 VCF columns to identify the input and allele,
 * followed by tab sep columns of **hgvsG, hgvsN, hgvsC and **hgvsP. */
{
if (sameString(alt, "."))
    alt = ref;
else if (!isAllNt(alt, strlen(alt)))
    {
    fprintf(f, "# %s:%d:%s/%s: alt is non-[AGCTN] '%s', skipping.\n",
            gBed3->chrom, gBed3->chromStart+1, ref, alt, alt);
    return;
    }
char *chromAcc = hRefSeqAccForChrom(db, gBed3->chrom);
char *hgvsG = hgvsGFromVariant(gSeqWin, gBed3, alt, chromAcc, breakDelIns);
vpExpandIndelGaps(psl, gSeqWin, txSeq);
struct vpTx *vpTx = vpGenomicToTranscript(gSeqWin, gBed3, alt, psl, txSeq);
char *hgvsN = hgvsNFromVpTx(vpTx, gSeqWin, psl, txSeq, breakDelIns);
char *hgvsC = NULL;
char *hgvsP = NULL;
if (cds && cds->start != cds->end && cds->start >= 0)
    {
    hgvsC = hgvsCFromVpTx(vpTx, gSeqWin, psl, cds, txSeq, breakDelIns);
    struct dnaSeq *protSeq = hGetNpAccAndSeq(db, psl->qName);
    struct vpPep *vpPep = vpTranscriptToProtein(vpTx, cds, txSeq, protSeq);
    boolean addParensToP = FALSE;  // could be option
    hgvsP = hgvsPFromVpPep(vpPep, protSeq, addParensToP);
    // do not free protSeq
    vpPepFree(&vpPep);
    }
int pos = gBed3->chromStart + 1;
fprintf(f, "%s\t%d\t%s\t%s\t%s\t%s\t"
        "%s\t%s\t%s\t%s\n",
        vcfChrom, pos, name, ref, alt, psl->qName,
        hgvsG, hgvsN, emptyForNull(hgvsC), emptyForNull(hgvsP));
freeMem(hgvsG);
freeMem(hgvsN);
freeMem(hgvsC);
freeMem(hgvsP);
vpTxFree(&vpTx);
}

void singleVcfToHgvsOut(char *db, struct vcfRecord *rec, struct seqWindow *gSeqWin,
                        struct psl *psl, struct genbankCds *cds, struct dnaSeq *txSeq,
                        FILE *f)
/* For each alternate allele in rec, output first 5 VCF columns to identify the input and allele,
 * followed by tab sep columns of hgvsG, hgvsN, hgvsC and hgvsP. */
{
//#*** TODO: watch out for VCF symbolic alleles like <DEL>
struct bed3 gBed3 = { NULL, psl->tName, rec->chromStart, rec->chromEnd };
char *ref = rec->alleles[0];
// If there is no alt, show consequence for genomic ref allele
if (rec->alleleCount == 1)
    singleAlleleToHgvsOut(db, &gBed3, ref, ref, rec->name, rec->chrom, gSeqWin,
                          psl, cds, txSeq, f);
// Show consequence of each alternate allele
int ix;
for (ix = 1; ix < rec->alleleCount;  ix++)
    singleAlleleToHgvsOut(db, &gBed3, ref, rec->alleles[ix], rec->name, rec->chrom, gSeqWin,
                          psl, cds, txSeq, f);
}

#define UPDOWN_FUDGE 5000

struct psl *pslsInRange(char *db, char *table, char *chrom, int chromStart, int chromEnd)
/* No caching even though this may return the same answer for many little regions.
 * Free returned list when done. */
{
struct psl *pslList = NULL;
struct sqlConnection *conn = hAllocConn(db);
int binOffset = 0;
int start = max(0, chromStart - UPDOWN_FUDGE);
int end = chromEnd + UPDOWN_FUDGE;
struct sqlResult *sr = hRangeQuery(conn, table, chrom, start, end, NULL, &binOffset);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    slAddHead(&pslList, pslLoad(row+binOffset));
hFreeConn(&conn);
return pslList;
}

void getCds(struct hash *txCdsHash, char *db, boolean hasNcbiRefSeq, char *acc,
            struct genbankCds *retCds)
/* Put a possibly cached cds's values into cds */
{
struct genbankCds *cds = hashFindVal(txCdsHash, acc);
if (cds == NULL)
    {
    AllocVar(cds);
    char query[2048];
    if (hasNcbiRefSeq)
        sqlSafef(query, sizeof(query), "select cds from ncbiRefSeqCds where id = '%s'", acc);
    else
        sqlSafef(query, sizeof(query),
                 "select c.name from %s as c, %s as g "
                 "where (g.acc = '%s') and (g.cds != 0) and (g.cds = c.id)"
                 , cdsTable, gbCdnaInfoTable, acc);
    struct sqlConnection *conn = hAllocConn(db);
    char cdsBuf[2048];
    char *cdsStr = sqlQuickQuery(conn, query, cdsBuf, sizeof(cdsBuf));
    hFreeConn(&conn);
    if (isNotEmpty(cdsStr))
        genbankCdsParse(cdsStr, cds);
    hashAdd(txCdsHash, acc, cds);
    }
memcpy(retCds, cds, sizeof(*retCds));
}

char *getChrom(char *db, char *vcfChrom)
{
char *chrom = hgOfficialChromName(db, vcfChrom);
if (chrom == NULL && !startsWith("chr", vcfChrom))
    {
    char chrChrom[2048];
    safef(chrChrom, sizeof(chrChrom), "chr%s", vcfChrom);
    chrom = hgOfficialChromName(db, chrChrom);
    }
return chrom;
}

void vcfToHgvs(char *db, char *vcfIn, char *hgvsOut)
/* vcfToHgvs - Transform VCF variant calls into HGVS terms. */
{
// UDC cache dir: first check for hg.conf setting, then override with command line option if given.
setUdcCacheDir();
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
knetUdcInstall();
struct vcfFile *vcff = vcfFileMayOpen(vcfIn, NULL, 0, 0, -1, -1, TRUE);
if (! vcff)
    errAbort("Sorry, can't open VCF file %s", vcfIn);
FILE *f = mustOpen(hgvsOut, "w");
initGenbankTableNames(db);
boolean hasNcbiRefSeq = hDbHasNcbiRefSeq(db);
char *pslTable = hasNcbiRefSeq ? "ncbiRefSeqPsl" : "refSeqAli";
struct vcfRecord *rec;
struct hash *txCdsHash = hashNew(0);
struct seqWindow *gSeqWin = chromSeqWindowNew(db, NULL, 0, 0);
for (rec = vcff->records;  rec != NULL;  rec = rec->next)
    {
    // If repetitive SQL queries are too slow then rewrite this to use annoGrator.
    char *chrom = getChrom(db, rec->chrom);
    if (isEmpty(chrom))
        errAbort("vcfToHgvs: Can't find UCSC %s name for VCF chrom/contig '%s'", db, rec->chrom);
    struct psl *txList = pslsInRange(db, pslTable, chrom, rec->chromStart, rec->chromEnd);
    struct psl *psl;
    for (psl = txList;  psl != NULL;  psl = psl->next)
        {
        struct dnaSeq *txSeq = hGetNmAccAndSeq(db, psl->qName);
        if (txSeq)
            {
            struct genbankCds cds;
            getCds(txCdsHash, db, hasNcbiRefSeq, psl->qName, &cds);
            singleVcfToHgvsOut(db, rec, gSeqWin, psl, &cds, txSeq, f);
            }
        else
            fprintf(f, "# *** NO SEQUENCE FOR %s ***\n", psl->qName);
        }
    pslFreeList(&txList);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
vcfToHgvs(argv[1], argv[2], argv[3]);
return 0;
}
