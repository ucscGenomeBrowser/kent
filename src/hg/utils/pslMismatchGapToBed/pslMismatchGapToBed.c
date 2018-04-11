/* pslMismatchGapToBed - Find mismatches and short indels between target and query (e.g. genome & transcript). */
#include "common.h"
#include "basicBed.h"
#include "fa.h"
#include "indelShift.h"
#include "linefile.h"
#include "hash.h"
#include "hdb.h"
#include "hgHgvs.h"
#include "options.h"
#include "psl.h"

// Arbitrary cutoff for minimum plausible intron size
#define MIN_INTRON 45

// Command line option default
static char *cdsFile = NULL;
static char *db = NULL;
static char *ignoreQNamePrefix = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslMismatchGapToBed - Find mismatches and too-short-to-be-intron indels (<%d bases)\n"
  "   between target and query (intended for comparing genome & transcripts)\n"
  "usage:\n"
  "   pslMismatchGapToBed in.psl target.2bit query.fa[.gz] outBaseName\n"
  "options:\n"
  "   -cdsFile=X                Get Genbank CDS strings from file X\n"
  "   -db=X                     Get RefSeq chromosome accessions for target from database X\n"
  "   -ignoreQNamePrefix=X      Ignore PSL rows whose qName starts with X\n"
  "5 BED files will be created: outBaseName.{mismatch,shortGap,shiftyGap,doubleGap,qSkipped}.bed\n"
  , MIN_INTRON);
}

// Command line validation table
static struct optionSpec options[] = {
    {"cdsFile", OPTION_STRING},
    {"db", OPTION_STRING},
    {"ignoreQNamePrefix", OPTION_STRING},
    {NULL, 0},
};

static void dnaSeqCopyMaybeRc(struct dnaSeq *seq, int start, int len, boolean isRc,
                              char *buf, size_t bufSize)
/* Copy len bases of seq into buf.  If isRc, reverse-complement the sequence. */
{
safencpy(buf, bufSize, seq->dna+start, len);
if (isRc)
    reverseComplement(buf, len);
}

static void addCoordCols(struct dyString *dy, char *acc, int start, int end)
/* Append three tab-prefixed items to dy. */
{
dyStringPrintf(dy, "\t%s\t%d\t%d", acc, start, end);
}

static void compareExonBases(struct psl *psl, int ix, struct seqWindow *gSeqWin,
                             struct dnaSeq *txSeq, struct genbankCds *cds, char *chromAcc,
                             struct bed4 **pMismatchList)
/* Compare exon ix sequence from genome and transcript to find mismatched bases. */
{
boolean isRc = (pslQStrand(psl) == '-');
int blockSize = psl->blockSizes[ix];
char exonT[blockSize+1];
seqWindowCopy(gSeqWin, psl->tStarts[ix], blockSize, exonT, sizeof(exonT));
char exonQ[blockSize+1];
int qExonStart = isRc ? psl->qSize - psl->qStarts[ix] - blockSize : psl->qStarts[ix];
dnaSeqCopyMaybeRc(txSeq, qExonStart, blockSize, isRc, exonQ, sizeof(exonQ));
static struct dyString *namePlus = NULL;
if (namePlus == NULL)
    namePlus = dyStringNew(0);
int jx;
for (jx = 0;  jx < blockSize;  jx++)
    {
    if (exonT[jx] != exonQ[jx])
        {
        int tStart = psl->tStarts[ix] + jx;
        char *tRef = exonT + jx;
        char *tAlt = exonQ + jx;
        int len = 1;
        while (jx < blockSize - 1 && exonT[jx+len] != exonQ[jx+len])
            len++;
        char qRef[len+1];
        char qAlt[len+1];
        safencpy(qRef, sizeof(qRef), tAlt, len);
        safencpy(qAlt, sizeof(qAlt), tRef, len);
        int qStart = psl->qStarts[ix] + jx;
        if (isRc)
            {
            int qEnd = qStart + len;
            qStart = psl->qSize - qEnd;
            reverseComplement(qRef, len);
            reverseComplement(qAlt, len);
            }
        dyStringClear(namePlus);
        dyStringPrintf(namePlus, "%s>%s", qRef, qAlt);
        dyStringPrintf(namePlus, "\t%c", psl->strand[0]);
        addCoordCols(namePlus, psl->qName, qStart, qStart+len);
        // Make HGVS g.
        struct bed3 variantBed;
        variantBed.chrom = psl->tName;
        variantBed.chromStart = tStart;
        variantBed.chromEnd = tStart+len;
        char alt[len+1];
        safencpy(alt, sizeof(alt), tAlt, len);
        char *hgvsG = hgvsGFromVariant(gSeqWin, &variantBed, alt, chromAcc, TRUE);
        dyStringPrintf(namePlus, "\t%s", hgvsG);
        // Make HGVS c./n.
        char gRef[len+1];
        safencpy(gRef, sizeof(gRef), tRef, len);
        struct vpTx vpTx;
        vpTx.txName = psl->qName;
        vpTx.start.region = vpTx.end.region = vpExon;
        vpTx.start.aliBlkIx = vpTx.end.aliBlkIx = ix;
        vpTx.start.txOffset = qStart;
        vpTx.start.gOffset = tStart;
        vpTx.end.txOffset = qStart+len;
        vpTx.end.gOffset = tStart+len;
        vpTx.gRef = qAlt;
        vpTx.gAlt = qRef;
        vpTx.txRef = qRef;
        vpTx.txAlt = qAlt;
        char *hgvsCN;
        if (cds)
            hgvsCN = hgvsCFromVpTx(&vpTx, gSeqWin, psl, cds, txSeq, TRUE);
        else
            hgvsCN = hgvsNFromVpTx(&vpTx, gSeqWin, psl, txSeq, TRUE);
        dyStringPrintf(namePlus, "\t%s", hgvsCN);
        slAddHead(pMismatchList,
                  bed4New(psl->tName, tStart, tStart+len, namePlus->string));
        freeMem(hgvsG);
        jx += (len - 1);
        }
    }
}

static void addCNRange(struct dyString *dy, int start, int end, struct genbankCds *cds)
/* Append an HGVS c. or n. position or range to dy. */
{
if (cds)
    {
    dyStringPrintf(dy, "c.");
    char cdsPrefix[2];
    cdsPrefix[0] = '\0';
    uint cdsCoord = hgvsTxToCds(start, cds, TRUE, cdsPrefix);
    dyStringPrintf(dy, "%s%u", cdsPrefix, cdsCoord);
    if (end > start)
        {
        cdsCoord = hgvsTxToCds(end, cds, FALSE, cdsPrefix);
        dyStringPrintf(dy, "_%s%u", cdsPrefix, cdsCoord);
        }
    }
else
    {
    dyStringPrintf(dy, "n.%d", start+1);
    if (end > start+1)
        dyStringPrintf(dy, "_%d", end);
    }
}

static void checkIntrons(struct psl *psl, int ix, struct seqWindow *gSeqWin, struct dnaSeq *txSeq,
                         struct genbankCds *cds, char *chromAcc,
                         struct bed4 **pShortGapList, struct bed **pShiftyGapList,
                         struct bed4 **pDoubleGapList)
/* Examine each intron to see if it's too short to be plausible.  Report too-short introns
 * and determine whether they are ambiguously placed, i.e. if they could be shifted left
 * and/or right on genome and transcript (see also vpExpandIndelGaps).  If not too short,
 * check for skipped query bases. */
{
boolean isRc = (pslQStrand(psl) == '-');
static struct dyString *namePlus = NULL;
if (namePlus == NULL)
    namePlus = dyStringNew(0);
if (ix < psl->blockCount - 1)
    {
    int blockSize = psl->blockSizes[ix];
    int tGapStart = psl->tStarts[ix] + blockSize;
    int tGapEnd = psl->tStarts[ix+1];
    int tGapLen = tGapEnd - tGapStart;
    int qGapStart = isRc ? psl->qSize - psl->qStarts[ix+1] : psl->qStarts[ix] + blockSize;
    int qGapEnd = isRc ? psl->qSize - psl->qStarts[ix] - blockSize : psl->qStarts[ix+1];
    int qGapLen = qGapEnd - qGapStart;
    if (tGapLen < MIN_INTRON)
        {
        // See if it is possible to shift the gap left and/or right on the genome.
        uint gStartL = tGapStart;
        uint gEndL = tGapEnd;
        uint gStartR = gStartL, gEndR = gEndL;
        char txCpyL[qGapLen+1], txCpyR[qGapLen+1];
        dnaSeqCopyMaybeRc(txSeq, qGapStart, qGapLen, isRc, txCpyL, sizeof(txCpyL));
        safencpy(txCpyR, sizeof(txCpyR), txCpyL, qGapLen);
        int shiftL = indelShift(gSeqWin, &gStartL, &gEndL, txCpyL, INDEL_SHIFT_NO_MAX, isdLeft);
        int shiftR = indelShift(gSeqWin, &gStartR, &gEndR, txCpyR, INDEL_SHIFT_NO_MAX, isdRight);
        if (shiftL || shiftR)
            {
            if (blockSize < shiftL)
                errAbort("pslMismatchGapToBed: support for overlapping blocks not implemented");
            if (psl->blockSizes[ix+1] < shiftR)
                errAbort("pslMismatchGapToBed: support for overlapping blocks not implemented");
            struct bed *bed;
            AllocVar(bed);
            bed->chrom = cloneString(psl->tName);
            bed->chromStart = tGapStart - shiftL;
            bed->chromEnd = tGapEnd + shiftR;
            dyStringClear(namePlus);
            dyStringPrintf(namePlus, "shift:L%d,R%d", shiftL, shiftR);
            int qStart = qGapStart - (isRc ? shiftR : shiftL);
            int qEnd = qGapEnd + (isRc ? shiftL : shiftR);
            addCoordCols(namePlus, psl->qName, qStart, qEnd);
            dyStringPrintf(namePlus, "\t%d", tGapLen);
            dyStringPrintf(namePlus, "\t%d", qGapLen);
            dyStringPrintf(namePlus, "\t%d", shiftL);
            dyStringPrintf(namePlus, "\t%d", shiftR);
            dyStringPrintf(namePlus, "\t%s:", psl->qName);
            addCNRange(namePlus, qStart, qEnd, cds);
            bed->name = cloneString(namePlus->string);
            bed->strand[0] = psl->strand[0];
            int thickSize = tGapLen;
            if (qGapLen > thickSize)
                thickSize = qGapLen;
            if (isRc)
                {
                bed->thickStart = bed->chromStart;
                bed->thickEnd = bed->chromStart + thickSize;
                if (bed->thickEnd > bed->chromEnd)
                    bed->thickEnd = bed->chromEnd;
                }
            else
                {
                bed->thickStart = bed->chromEnd - thickSize;
                if (bed->thickStart < bed->chromStart)
                    bed->thickStart = bed->chromStart;
                bed->thickEnd = bed->chromEnd;
                }
            slAddHead(pShiftyGapList, bed);
            }
        dyStringClear(namePlus);
        dyStringPrintf(namePlus, "skip:G%d,T%d", tGapLen, qGapLen);
        // Make HGVS g.
        struct bed3 variantBed;
        variantBed.chrom = psl->tName;
        variantBed.chromStart = gStartR;
        variantBed.chromEnd = gEndR;
        char *hgvsG = hgvsGFromVariant(gSeqWin, &variantBed, txCpyR, chromAcc, TRUE);
        // Make HGVS c./n.
        variantBed.chromStart = tGapStart;
        variantBed.chromEnd = tGapEnd;
        char tRef[tGapLen+1];
        seqWindowCopy(gSeqWin, variantBed.chromStart, tGapLen, tRef, sizeof(tRef));
        struct vpTx *vpTx = vpGenomicToTranscript(gSeqWin, &variantBed, tRef, psl, txSeq);
        char *hgvsCN;
        if (cds)
            hgvsCN = hgvsCFromVpTx(vpTx, gSeqWin, psl, cds, txSeq, TRUE);
        else
            hgvsCN = hgvsNFromVpTx(vpTx, gSeqWin, psl, txSeq, TRUE);
        dyStringPrintf(namePlus, "\t%c", psl->strand[0]);
        addCoordCols(namePlus, psl->qName, vpTx->start.txOffset, vpTx->end.txOffset);
        dyStringPrintf(namePlus, "\t%d", tGapLen);
        dyStringPrintf(namePlus, "\t%d", qGapLen);
        dyStringPrintf(namePlus, "\t%s", hgvsG);
        dyStringPrintf(namePlus, "\t%s", hgvsCN);
        slAddHead(pShortGapList,
                  bed4New(psl->tName, tGapStart, tGapEnd, namePlus->string));
        freeMem(hgvsG);
        vpTxFree(&vpTx);
        }
    else if (qGapLen > 0)
        {
        // Not too short, but skips transcript bases
        dyStringClear(namePlus);
        dyStringPrintf(namePlus, "skip:G%d,T%d", tGapLen, qGapLen);
        dyStringPrintf(namePlus, "\t%c", psl->strand[0]);
        addCoordCols(namePlus, psl->qName, qGapStart, qGapEnd);
        dyStringPrintf(namePlus, "\t%d", tGapLen);
        dyStringPrintf(namePlus, "\t%d", qGapLen);
        dyStringPrintf(namePlus, "\t%s:", psl->qName);
        addCNRange(namePlus, qGapStart, qGapEnd, cds);
        dyStringAppend(namePlus, "del");
        slAddHead(pDoubleGapList,
                  bed4New(psl->tName, tGapStart, tGapEnd, namePlus->string));
        }
    }
}

static void checkUnalignedTxSeq(struct psl *psl, struct seqWindow *gSeqWin, struct dnaSeq *txSeq,
                                struct genbankCds *cds, struct bed4 **pQSkippedList)
/* Check for skipped q bases before alignment start and non-polyA skipped q bases after end
 * of alignment. */
{
boolean isRc = (pslQStrand(psl) == '-');
static struct dyString *namePlus = NULL;
if (namePlus == NULL)
    namePlus = dyStringNew(0);
if (psl->qStarts[0] != 0)
    {
    if (isRc)
        {
        // Is it polyA?  Using the same heuristic as in hgTracks/cds.c, consider it close
        // enough if it's within 3 bases of the number of unaligned bases.
        int polyASize = tailPolyASizeLoose(txSeq->dna, txSeq->size);
        boolean isPolyA = (polyASize > 0 && polyASize + 3 >= psl->qStarts[0]);
        if (! isPolyA)
            {
            // Unaligned transcript sequence at end
            int qSkipStart = psl->qSize - psl->qStarts[0];
            dyStringClear(namePlus);
            dyStringPrintf(namePlus, "last%ddel", psl->qStarts[0]);
            dyStringPrintf(namePlus, "\t%c", psl->strand[0]);
            addCoordCols(namePlus, psl->qName, qSkipStart, psl->qSize);
            dyStringPrintf(namePlus, "\t%s:", psl->qName);
            addCNRange(namePlus, qSkipStart, psl->qSize, cds);
            dyStringAppend(namePlus, "del");
            slAddHead(pQSkippedList,
                      bed4New(psl->tName, psl->tStart, psl->tStart, namePlus->string));
            }
        }
    else
        {
        // Unaligned transcript sequence at start
        dyStringClear(namePlus);
        dyStringPrintf(namePlus, "first%ddel", psl->qStarts[0]);
        dyStringPrintf(namePlus, "\t%c", psl->strand[0]);
        addCoordCols(namePlus, psl->qName, 0, psl->qStarts[0]);
        dyStringPrintf(namePlus, "\t%s:", psl->qName);
        addCNRange(namePlus, 0, psl->qStarts[0], cds);
        dyStringAppend(namePlus, "del");
        slAddHead(pQSkippedList, bed4New(psl->tName, psl->tStart, psl->tStart, namePlus->string));
        }
    }
// Unlike psl->qStarts[], psl->qStart and psl->qEnd are not reversed; for my sanity, keep it
// all on genomic + until making tx coords.
int lastIx = psl->blockCount - 1;
int qEnd = psl->qStarts[lastIx] + psl->blockSizes[lastIx];
int qSkipped = psl->qSize - qEnd;
if (qSkipped > 0)
    {
    if (isRc)
        {
        // Unaligned transcript sequence at start
        dyStringClear(namePlus);
        dyStringPrintf(namePlus, "first%ddel", qSkipped);
        dyStringPrintf(namePlus, "\t%c", psl->strand[0]);
        addCoordCols(namePlus, psl->qName, 0, qSkipped);
        dyStringPrintf(namePlus, "\t%s:", psl->qName);
        addCNRange(namePlus, 0, qSkipped, cds);
        dyStringAppend(namePlus, "del");
        slAddHead(pQSkippedList, bed4New(psl->tName, psl->tEnd, psl->tEnd, namePlus->string));
        }
    else
        {
        // Is it polyA?  Using the same heuristic as in hgTracks/cds.c, consider it close
        // enough if it's within 3 bases of the number of unaligned bases.
        int polyASize = tailPolyASizeLoose(txSeq->dna, txSeq->size);
        boolean isPolyA = (polyASize > 0 && polyASize + 3 >= qSkipped);
        if (! isPolyA)
            {
            // Unaligned transcript sequence at end
            dyStringClear(namePlus);
            dyStringPrintf(namePlus, "last%ddel", qSkipped);
            dyStringPrintf(namePlus, "\t%c", psl->strand[0]);
            addCoordCols(namePlus, psl->qName, qEnd, psl->qSize);
            dyStringPrintf(namePlus, "\t%s:", psl->qName);
            addCNRange(namePlus, qEnd, psl->qSize, cds);
            dyStringAppend(namePlus, "del");
            slAddHead(pQSkippedList, bed4New(psl->tName, psl->tEnd, psl->tEnd, namePlus->string));
            }
        }
    }
}

static void checkTranscript(struct psl *psl, struct seqWindow *gSeqWin, struct dnaSeq *txSeq,
                            struct genbankCds *cds,
                            struct bed4 **pMismatchList, struct bed4 **pShortGapList,
                            struct bed **pShiftyGapList, struct bed4 **pDoubleGapList,
                            struct bed4 **pQSkippedList)
/* Build up lists of mismatches and non-exon, non-polyA indels between genome and transcript. */
{
if (psl->qSize != txSeq->size)
    errAbort("%s psl->qSize is %d but txSeq->size is %d",
             psl->qName, psl->qSize, txSeq->size);
int fudge = 1024;
int winStart = (psl->tStart > fudge) ? psl->tStart - fudge : 0;
int winEnd = psl->tEnd + fudge;
gSeqWin->fetch(gSeqWin, psl->tName, winStart, winEnd);
touppers(txSeq->dna);
char *chromAcc = db ? hRefSeqAccForChrom(db, psl->tName) : psl->tName;
int ix;
for (ix = 0;  ix < psl->blockCount;  ix++)
    {
    compareExonBases(psl, ix, gSeqWin, txSeq, cds, chromAcc, pMismatchList);
    checkIntrons(psl, ix, gSeqWin, txSeq, cds, chromAcc,
                 pShortGapList, pShiftyGapList, pDoubleGapList);
    }
checkUnalignedTxSeq(psl, gSeqWin, txSeq, cds, pQSkippedList);
}

static void sortAndDumpBedNPlus(char *outBaseName, char *name, void *bedList, int n)
/* Sort the mostly-backwards bed list with n fields and extra fields in bed->name following \t
 * and dump to file named outBaseName.name.bed. */
{
char fileName[PATH_LEN];
safef(fileName, sizeof(fileName), "%s.%s.bed", outBaseName, name);
FILE *f = mustOpen(fileName, "w");
slReverse(&bedList);
slSort(&bedList, bedCmp);
struct bed *bed;
for (bed = (struct bed *)bedList;  bed != NULL;  bed = bed->next)
    {
    if (n == 4)
        fprintf(f, "%s\t%d\t%d\t%s\n", bed->chrom, bed->chromStart, bed->chromEnd, bed->name);
    else if (n == 8)
        {
        char *extra = strchr(bed->name, '\t');
        *extra++ = '\0';
        fprintf(f, "%s\t%d\t%d\t%s\t%d\t%s\t%d\t%d\t%s\n",
                bed->chrom, bed->chromStart, bed->chromEnd, bed->name,
                bed->score, bed->strand, bed->thickStart, bed->thickEnd, extra);
        }
    else
        errAbort("sortAndDumpBedNPlus: need to add support for bed%d", n);
    }
carefulClose(&f);
}

static struct hash *hashDnaSeqFromFa(char *faFile)
/* Return a hash that maps sequence names from faFile to dnaSeq records. */
{
struct hash *hash = hashNew(17);
verbose(2, "Reading query fasta from %s\n", faFile);
struct dnaSeq *allSeqs = faReadAllDna(faFile), *seq;
verbose(2, "Got %d sequences from %s.\n", slCount(allSeqs), faFile);
for (seq = allSeqs;  seq != NULL;  seq = seq->next)
    hashAdd(hash, seq->name, seq);
return hash;
}

static struct hash *hashCdsFromFile(char *cdsFile)
/* Return a hash that maps sequence names from cdsFile to struct genbankCds *,
 * or NULL if cdsFile is NULL. */
{
struct hash *hash = NULL;
if (cdsFile)
    {
    verbose(2, "Reading Genbank CDS strings from %s\n", cdsFile);
    hash = hashNew(17);
    int count = 0;
    struct lineFile *lf = lineFileOpen(cdsFile, TRUE);
    char *line = NULL;
    while (lineFileNextReal(lf, &line))
        {
        char *words[2];
        int wordCount = chopTabs(line, words);
        lineFileExpectWords(lf, 2, wordCount);
        struct genbankCds *cds;
        AllocVar(cds);
        if (! genbankCdsParse(words[1], cds))
            lineFileAbort(lf, "Unable to parse '%s' as Genbank CDS string", words[1]);
        hashAdd(hash, words[0], cds);
        count++;
        }
    lineFileClose(&lf);
    verbose(2, "Got %d CDS strings from %s.\n", count, cdsFile);
    }
return hash;
}

void pslMismatchGapToBed(char *pslFile, char *targetTwoBit, char *queryFa, char *outBaseName)
/* pslMismatchGapToBed - Find mismatches and short indels between target and query
 * (e.g. genome & transcript). */
{
struct bed4 *baseMismatchList = NULL, *shortGapList = NULL, *doubleGapList = NULL,
    *qSkippedList = NULL;
struct bed *shiftyGapList = NULL;
struct seqWindow *gSeqWin = twoBitSeqWindowNew(targetTwoBit, NULL, 0, 0);
struct hash *qSeqHash = hashDnaSeqFromFa(queryFa);
struct hash *cdsHash = hashCdsFromFile(cdsFile);
struct lineFile *pslLf = pslFileOpen(pslFile);
struct psl *psl;
while ((psl = pslNext(pslLf)) != NULL)
    {
    if (ignoreQNamePrefix == NULL || !startsWith(ignoreQNamePrefix, psl->qName))
        {
        struct dnaSeq *txSeq = hashFindVal(qSeqHash, psl->qName);
        if (txSeq)
            {
            struct genbankCds *cds = cdsHash ? hashFindVal(cdsHash, psl->qName) : NULL;
            checkTranscript(psl, gSeqWin, txSeq, cds,
                            &baseMismatchList, &shortGapList, &shiftyGapList, &doubleGapList,
                            &qSkippedList);
            }
        else
            {
            errAbort("No sequence found for psl->qName = '%s' - exiting.", psl->qName);
            }
        pslFree(&psl);
        }
    }
lineFileClose(&pslLf);
sortAndDumpBedNPlus(outBaseName, "mismatch", baseMismatchList, 4);
sortAndDumpBedNPlus(outBaseName, "shortGap", shortGapList, 4);
sortAndDumpBedNPlus(outBaseName, "shiftyGap", shiftyGapList, 8);
sortAndDumpBedNPlus(outBaseName, "doubleGap", doubleGapList, 4);
sortAndDumpBedNPlus(outBaseName, "qSkipped", qSkippedList, 4);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
cdsFile = optionVal("cdsFile", cdsFile);
db = optionVal("db", db);
ignoreQNamePrefix = optionVal("ignoreQNamePrefix", ignoreQNamePrefix);
pslMismatchGapToBed(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
