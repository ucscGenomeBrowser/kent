/* hgvsTester -- exercise lib/hgHgvs.c */

/* Copyright (C) 2016 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hgHgvs.h"
#include "genbank.h"
#include "hdb.h"
#include "knetUdc.h"
#include "linefile.h"
#include "memalloc.h"
#include "options.h"
#include "chromAlias.h"
#include "psl.h"
#include "seqWindow.h"
#include "variantProjector.h"

static struct optionSpec optionSpecs[] = {
    {"variants", OPTION_BOOLEAN},
    {"noTx", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/* explain usage and exit */
{
errAbort(
"hgvsTester - test program for lib/hgHgvs.c\n\n"
"usage:\n"
"    hgvsTester db inputFile\n"
"options:\n"
"    -variants  Each line of inputFile is \"chrom start end alt\" (0-based start, alt '-'\n"
"               for none).  Print the HGVS g. term, then the n., c. and p. terms for each\n"
"               overlapping ncbiRefSeqPsl transcript.\n"
"    -noTx      With -variants, print only the g. term.\n"
"Without -variants, each line of inputFile is either a \"#\"-comment line or an\n"
"HGVS(-like) term.\n"
         );
}

int testHgvsTerms(char *db, char *inputFile)
{
// Each line of input is an HGVS term.  Map it to reference assembly coords and print position.
struct dyString *dy = dyStringNew(0);
struct lineFile *lf = lineFileOpen(inputFile, TRUE);
printf("# db: %s\n"
       "# inputFile: %s\n", db, inputFile);
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    char *term = trimSpaces(line);
    if (term[0] == '\0' || term[0] == '#')
        {
        puts(line);
        continue;
        }
    struct hgvsVariant *hgvsList = hgvsParseTerm(term);
    if (hgvsList == NULL)
        hgvsList = hgvsParsePseudoHgvs(db, term);
    if (hgvsList == NULL)
        printf("# Failed to parse '%s' as HGVS\n", term);
    else
        {
        struct hgvsVariant *hgvs = NULL;
        struct hash *bedHash = hashNew(0);
        int okCount = 0;
        for (hgvs = hgvsList; hgvs != NULL; hgvs = hgvs->next)
            {
            struct bed *region = hgvsMapToGenome(db, hgvs, NULL);
            if (region != NULL)
                {
                okCount++;
                // Now that hgvsParsePsuedoHgvs returns a list of valid positions
                // we only want to print the unique ones in the list
                dyStringClear(dy);;
                dyStringPrintf(dy, "%s\t%d\t%d\t%s\t0\t%c\n",
                    region->chrom, region->chromStart, region->chromEnd, term, region->strand[0]);
                if (hashFindVal(bedHash, dy->string) == NULL)
                    hashAdd(bedHash, cloneString(dy->string), region);
                }
            }
        // Complain only if none of the HGVS terms in the list could be mapped
        if (okCount == 0)
            printf("# Failed to map '%s' to %s assembly\n", term, db);
        struct hashEl *hel, *helList = hashElListHash(bedHash);
        for (hel = helList; hel != NULL; hel = hel->next)
            {
            struct bed *region = (struct bed *)hel->val;
            printf("%s\t%d\t%d\t%s\t0\t%c\n",
                   region->chrom, region->chromStart, region->chromEnd, term, region->strand[0]);
            }
        }
    }
lineFileClose(&lf);
carefulCheckHeap();
return 0;
}

static struct psl *txPslsInRange(char *db, char *chrom, uint start, uint end)
/* Return NM_ and NR_ alignments from ncbiRefSeqPsl that overlap chrom:start-end,
 * sorted by name. */
{
struct psl *pslList = NULL;
struct sqlConnection *conn = hAllocConn(db);
int binOffset = 0;
uint queryEnd = (end > start) ? end : start + 1;
struct sqlResult *sr = hRangeQuery(conn, "ncbiRefSeqPsl", chrom, start, queryEnd, NULL,
                                   &binOffset);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (!startsWith("X", row[binOffset+9]))
        slAddHead(&pslList, pslLoad(row+binOffset));
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slSort(&pslList, pslCmpQuery);
return pslList;
}

static boolean getTxCds(char *db, char *acc, struct genbankCds *retCds)
/* Fill in retCds from ncbiRefSeqCds and return TRUE, or return FALSE if acc has no CDS. */
{
ZeroVar(retCds);
char query[1024];
sqlSafef(query, sizeof(query), "select cds from ncbiRefSeqCds where id = '%s'", acc);
struct sqlConnection *conn = hAllocConn(db);
char cdsBuf[1024];
char *cdsStr = sqlQuickQuery(conn, query, cdsBuf, sizeof(cdsBuf));
hFreeConn(&conn);
if (isEmpty(cdsStr))
    return FALSE;
genbankCdsParse(cdsStr, retCds);
return (retCds->start < retCds->end && retCds->start >= 0);
}

static struct dnaSeq *getProtSeq(char *db, char *acc)
/* Return the protein sequence for transcript acc, named by its NP_ accession, or NULL. */
{
char query[1024];
sqlSafef(query, sizeof(query),
         "select p.name, p.seq from ncbiRefSeqLink l, ncbiRefSeqPepTable p "
         "where l.mrnaAcc = '%s' and l.protAcc = p.name", acc);
struct sqlConnection *conn = hAllocConn(db);
struct slPair *accSeq = sqlQuickPairList(conn, query);
hFreeConn(&conn);
if (accSeq == NULL)
    return NULL;
char *seq = accSeq->val;
return newDnaSeq(seq, strlen(seq), accSeq->name);
}

static void printTxTerms(char *db, struct seqWindow *gSeqWin, struct bed3 *variantBed, char *alt,
                         struct psl *psl)
/* Print the n., c. and p. terms for the variant projected onto psl's transcript. */
{
struct dnaSeq *txSeq = hDnaSeqGet(db, psl->qName, "seqNcbiRefSeq", "extNcbiRefSeq");
if (txSeq == NULL)
    {
    printf("#\t%s: no sequence\n", psl->qName);
    return;
    }
vpExpandIndelGaps(psl, gSeqWin, txSeq);
struct vpTx *vpTx = vpGenomicToTranscript(gSeqWin, variantBed, alt, psl, txSeq);
char *hgvsN = hgvsNFromVpTx(vpTx, gSeqWin, psl, txSeq, TRUE);
char *hgvsC = NULL;
char *hgvsP = NULL;
struct genbankCds cds;
if (getTxCds(db, psl->qName, &cds))
    {
    hgvsC = hgvsCFromVpTx(vpTx, gSeqWin, psl, &cds, txSeq, TRUE);
    struct dnaSeq *protSeq = getProtSeq(db, psl->qName);
    struct vpPep *vpPep = vpTranscriptToProtein(vpTx, &cds, txSeq, protSeq);
    hgvsP = hgvsPFromVpPep(vpPep, protSeq, FALSE);
    vpPepFree(&vpPep);
    dnaSeqFree(&protSeq);
    }
printf("\t%s\t%s\t%s\n", hgvsN, emptyForNull(hgvsC), emptyForNull(hgvsP));
freeMem(hgvsN);
freeMem(hgvsC);
freeMem(hgvsP);
vpTxFree(&vpTx);
dnaSeqFree(&txSeq);
}

int testHgvsVariants(char *db, char *inputFile, boolean doTx)
/* Each line of input is "chrom start end alt".  Print the HGVS g. term, and if doTx, the
 * n., c. and p. terms for each overlapping transcript. */
{
struct lineFile *lf = lineFileOpen(inputFile, TRUE);
printf("# db: %s\n"
       "# inputFile: %s\n", db, inputFile);
struct seqWindow *gSeqWin = chromSeqWindowNew(db, NULL, 0, 0);
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    char *trimmed = trimSpaces(line);
    if (trimmed[0] == '\0' || trimmed[0] == '#')
        {
        puts(line);
        continue;
        }
    char *words[4];
    int wordCount = chopByWhite(trimmed, words, ArraySize(words));
    lineFileExpectWords(lf, ArraySize(words), wordCount);
    struct bed3 variantBed;
    ZeroVar(&variantBed);
    variantBed.chrom = words[0];
    variantBed.chromStart = lineFileNeedFullNum(lf, words, 1);
    variantBed.chromEnd = lineFileNeedFullNum(lf, words, 2);
    char *alt = sameString(words[3], "-") ? "" : words[3];
    char *chromAcc = hRefSeqAccForChrom(db, variantBed.chrom);
    char *hgvsG = hgvsGFromVariant(gSeqWin, &variantBed, alt, chromAcc, TRUE);
    printf("%s\t%u\t%u\t%s\t%s\n", variantBed.chrom, variantBed.chromStart, variantBed.chromEnd,
           words[3], hgvsG);
    freeMem(hgvsG);
    if (doTx)
        {
        struct psl *psl, *pslList = txPslsInRange(db, variantBed.chrom, variantBed.chromStart,
                                                  variantBed.chromEnd);
        for (psl = pslList;  psl != NULL;  psl = psl->next)
            {
            char *altCpy = cloneString(alt);
            printTxTerms(db, gSeqWin, &variantBed, altCpy, psl);
            freeMem(altCpy);
            }
        pslFreeList(&pslList);
        }
    }
lineFileClose(&lf);
chromSeqWindowFree(&gSeqWin);
carefulCheckHeap();
return 0;
}

int main(int argc, char *argv[])
{
// Check args
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
char *db = argv[1];
char *inputFile = argv[2];
// Set up environment
pushCarefulMemHandler(LIMIT_2or6GB);
chromAliasSetup(db);
initGenbankTableNames(db);
if (udcCacheTimeout() < 300)
    udcSetCacheTimeout(300);
udcSetDefaultDir("./udcCache");
knetUdcInstall();

if (optionExists("variants"))
    return testHgvsVariants(db, inputFile, !optionExists("noTx"));
return testHgvsTerms(db, inputFile);
}
