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
#include "txAliDiff.h"

// Arbitrary cutoff for minimum plausible intron size
#define MIN_INTRON 45

// Different color options
#define MISMATCH_COLOR 0xff0000 //red
#define SHORTGAP_COLOR 0xffa500 //orange
#define SHIFTYGAP_COLOR 0xffa500 //orange
#define DOUBLEGAP_COLOR 0x888888 //grey
#define QSKIPPED_COLOR 0x800080 //purple

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

static void addCoordCols(struct txAliDiff *newDiff, char *acc, int start, int end)
/* Fill out the txName, txStart, and txEnd fields of a txAliDiff. */
{
newDiff->txName = cloneString(acc);
newDiff->txStart = start;
newDiff->txEnd = end;
}

static void compareExonBases(struct psl *psl, int ix, struct seqWindow *gSeqWin,
                             struct dnaSeq *txSeq, struct genbankCds *cds, char *chromAcc,
                             struct txAliDiff **differencesList)
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
        struct txAliDiff *newMismatch = NULL;
        AllocVar(newMismatch);
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
        dyStringPrintf(namePlus, "Mismatch: %s>%s", qRef, qAlt);
        newMismatch->name = cloneString(namePlus->string);
        newMismatch->strand[0] = psl->strand[0];
        addCoordCols(newMismatch, psl->qName, qStart, qStart+len);
        // Make HGVS g.
        struct bed3 variantBed;
        variantBed.chrom = psl->tName;
        variantBed.chromStart = tStart;
        variantBed.chromEnd = tStart+len;
        char alt[len+1];
        safencpy(alt, sizeof(alt), tAlt, len);
        char *hgvsG = hgvsGFromVariant(gSeqWin, &variantBed, alt, chromAcc, TRUE);
        newMismatch->hgvsG = cloneString(hgvsG);
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
        newMismatch->hgvsCN = cloneString(hgvsCN);
        newMismatch->chrom = cloneString(psl->tName);
        newMismatch->chromStart = tStart;
        newMismatch->chromEnd = tStart + len;
        newMismatch->thickStart = tStart;
        newMismatch->thickEnd = tStart + len;
        newMismatch->reserved = MISMATCH_COLOR;
        newMismatch->hgvsN = "";
        newMismatch->hgvsPosCN = "";
        slAddHead(differencesList, newMismatch);
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
                         struct txAliDiff **differencesList)
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
                warn("pslMismatchGapToBed: %s gapIx %d shifted left %d bases, but previous block "
                     "size is only %d; report to NCBI",
                     psl->qName, ix, shiftL, blockSize);
            if (psl->blockSizes[ix+1] < shiftR)
                warn("pslMismatchGapToBed: %s gapIx %d shifted right %d bases, but next block "
                     "size is only %d; report to NCBI",
                     psl->qName, ix, shiftR, psl->blockSizes[ix+1]);
            struct txAliDiff *shiftyItem = NULL;
            AllocVar(shiftyItem);
            shiftyItem->chrom = cloneString(psl->tName);
            shiftyItem->chromStart = tGapStart - shiftL;
            shiftyItem->chromEnd = tGapEnd + shiftR;
            shiftyItem->reserved = SHIFTYGAP_COLOR;
            shiftyItem->hgvsG = "";
            shiftyItem->hgvsN = "";
            shiftyItem->hgvsCN = "";
            dyStringClear(namePlus);
            dyStringPrintf(namePlus, "Shifted Gap shift: L%d,R%d", shiftL, shiftR);
            shiftyItem->name = cloneString(namePlus->string);

            int qStart = qGapStart - (isRc ? shiftR : shiftL);
            int qEnd = qGapEnd + (isRc ? shiftL : shiftR);
            addCoordCols(shiftyItem, psl->qName, qStart, qEnd);

            shiftyItem->gSkipped = tGapLen;
            shiftyItem->txSkipped = qGapLen;
            shiftyItem->shiftL = shiftL;
            shiftyItem->shiftR = shiftR;

            dyStringClear(namePlus);
            dyStringPrintf(namePlus, "%s:", psl->qName);
            addCNRange(namePlus, qStart, qEnd, cds);
            shiftyItem->hgvsPosCN = cloneString(namePlus->string);
            shiftyItem->strand[0] = psl->strand[0];

            int thickSize = tGapLen;
            if (qGapLen > thickSize)
                thickSize = qGapLen;
            if (isRc)
                {
                shiftyItem->thickStart = shiftyItem->chromStart;
                shiftyItem->thickEnd = shiftyItem->chromStart + thickSize;
                if (shiftyItem->thickEnd > shiftyItem->chromEnd)
                    shiftyItem->thickEnd = shiftyItem->chromEnd;
                }
            else
                {
                shiftyItem->thickStart = shiftyItem->chromEnd - thickSize;
                if (shiftyItem->thickStart < shiftyItem->chromStart)
                    shiftyItem->thickStart = shiftyItem->chromStart;
                shiftyItem->thickEnd = shiftyItem->chromEnd;
                }
            slAddHead(differencesList, shiftyItem);
            }
        struct txAliDiff *shortItem = NULL;
        AllocVar(shortItem);
        dyStringClear(namePlus);
        dyStringPrintf(namePlus, "Short Gap skip: G%d,T%d", tGapLen, qGapLen);
        shortItem->name = cloneString(namePlus->string);
        shortItem->chrom = cloneString(psl->tName);
        shortItem->chromStart = tGapStart;
        shortItem->chromEnd = tGapEnd;
        shortItem->thickStart = tGapStart;
        shortItem->thickEnd = tGapEnd;
        shortItem->reserved = SHORTGAP_COLOR;
        shortItem->strand[0] = psl->strand[0];
        shortItem->hgvsPosCN = "";
        shortItem->hgvsCN = "";

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

        addCoordCols(shortItem, psl->qName, vpTx->start.txOffset, vpTx->end.txOffset);
        shortItem->gSkipped = tGapLen;
        shortItem->txSkipped = qGapLen;
        shortItem->hgvsG = cloneString(hgvsG);
        shortItem->hgvsN = cloneString(hgvsCN);
        slAddHead(differencesList, shortItem);
        freeMem(hgvsG);
        vpTxFree(&vpTx);
        }
    else if (qGapLen > 0)
        {
        // Not too short, but skips transcript bases
        struct txAliDiff *doubleItem = NULL;
        AllocVar(doubleItem);
        doubleItem->chrom = cloneString(psl->tName);
        doubleItem->chromStart = tGapStart;
        doubleItem->chromEnd = tGapEnd;
        doubleItem->reserved = DOUBLEGAP_COLOR;
        doubleItem->hgvsG = "";
        doubleItem->hgvsN = "";
        doubleItem->hgvsPosCN = "";

        dyStringClear(namePlus);
        dyStringPrintf(namePlus, "Double Gap: G%d,T%d", tGapLen, qGapLen);
        doubleItem->name = cloneString(namePlus->string);

        doubleItem->strand[0] = psl->strand[0];
        addCoordCols(doubleItem, psl->qName, qGapStart, qGapEnd);
        doubleItem->gSkipped = tGapLen;
        doubleItem->txSkipped = qGapLen;
        dyStringClear(namePlus);
        dyStringPrintf(namePlus, "%s:", psl->qName);
        addCNRange(namePlus, qGapStart, qGapEnd, cds);
        dyStringAppend(namePlus, "del");
        doubleItem->hgvsCN = cloneString(namePlus->string);

        doubleItem->thickStart = tGapStart;
        doubleItem->thickEnd = tGapEnd;
        slAddHead(differencesList, doubleItem);
        }
    }
}

static void checkUnalignedTxSeq(struct psl *psl, struct seqWindow *gSeqWin, struct dnaSeq *txSeq,
                                struct genbankCds *cds, struct txAliDiff **differencesList)
/* Check for skipped q bases before alignment start and non-polyA skipped q bases after end
 * of alignment. */
{
boolean isRc = (pslQStrand(psl) == '-');
static struct dyString *namePlus = NULL;
if (namePlus == NULL)
    namePlus = dyStringNew(0);
if (psl->qStarts[0] != 0)
    {
    struct txAliDiff *newItem = NULL;
    AllocVar(newItem);
    newItem->chrom = cloneString(psl->tName);
    newItem->chromStart = psl->tStart;
    newItem->chromEnd = psl->tStart;
    newItem->thickStart =  psl->tStart;
    newItem->thickEnd = psl->tStart;
    newItem->reserved = QSKIPPED_COLOR;
    newItem->hgvsG = "";
    newItem->hgvsN = "";
    newItem->hgvsPosCN = "";
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
            dyStringPrintf(namePlus, "Transcript Skip: last%ddel", psl->qStarts[0]);
            newItem->name = cloneString(namePlus->string);
            dyStringClear(namePlus);
            newItem->strand[0] = psl->strand[0];
            addCoordCols(newItem, psl->qName, qSkipStart, psl->qSize);
            dyStringPrintf(namePlus, "%s:", psl->qName);
            addCNRange(namePlus, qSkipStart, psl->qSize, cds);
            dyStringAppend(namePlus, "del");
            newItem->hgvsCN = cloneString(namePlus->string);
            slAddHead(differencesList, newItem);
            }
        }
    else
        {
        // Unaligned transcript sequence at start
        dyStringClear(namePlus);
        dyStringPrintf(namePlus, "Transcript Skip: first%ddel", psl->qStarts[0]);
        newItem->name = cloneString(namePlus->string);
        dyStringClear(namePlus);
        newItem->strand[0] = psl->strand[0];
        addCoordCols(newItem, psl->qName, 0, psl->qStarts[0]);
        dyStringPrintf(namePlus, "%s:", psl->qName);
        addCNRange(namePlus, 0, psl->qStarts[0], cds);
        dyStringAppend(namePlus, "del");
        newItem->hgvsCN = cloneString(namePlus->string);
        slAddHead(differencesList, newItem);
        }
    }
// Unlike psl->qStarts[], psl->qStart and psl->qEnd are not reversed; for my sanity, keep it
// all on genomic + until making tx coords.
int lastIx = psl->blockCount - 1;
int qEnd = psl->qStarts[lastIx] + psl->blockSizes[lastIx];
int qSkipped = psl->qSize - qEnd;
if (qSkipped > 0)
    {
    struct txAliDiff *newItem = NULL;
    AllocVar(newItem);
    newItem->chrom = cloneString(psl->tName);
    newItem->chromStart = psl->tEnd;
    newItem->chromEnd = psl->tEnd;
    newItem->thickStart =  psl->tEnd;
    newItem->thickEnd = psl->tEnd;
    newItem->reserved = QSKIPPED_COLOR;
    newItem->hgvsG = "";
    newItem->hgvsN = "";
    newItem->hgvsPosCN = "";
    if (isRc)
        {
        // Unaligned transcript sequence at start
        dyStringClear(namePlus);
        dyStringPrintf(namePlus, "Transcript Skip: first%ddel", qSkipped);
        newItem->name = cloneString(namePlus->string);
        dyStringClear(namePlus);
        newItem->strand[0] = psl->strand[0];
        addCoordCols(newItem, psl->qName, 0, qSkipped);
        dyStringPrintf(namePlus, "%s:", psl->qName);
        addCNRange(namePlus, 0, qSkipped, cds);
        dyStringAppend(namePlus, "del");
        newItem->hgvsCN = cloneString(namePlus->string);
        slAddHead(differencesList, newItem);
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
            dyStringPrintf(namePlus, "Transcript Skip: last%ddel", qSkipped);
            newItem->name = cloneString(namePlus->string);
            dyStringClear(namePlus);
            newItem->strand[0] = psl->strand[0];
            addCoordCols(newItem, psl->qName, qEnd, psl->qSize);
            dyStringPrintf(namePlus, "%s:", psl->qName);
            addCNRange(namePlus, qEnd, psl->qSize, cds);
            dyStringAppend(namePlus, "del");
            newItem->hgvsCN = cloneString(namePlus->string);
            slAddHead(differencesList, newItem);
           }
        }
    }
}

static void checkTranscript(struct psl *psl, struct seqWindow *gSeqWin, struct dnaSeq *txSeq,
                            struct genbankCds *cds,
                            struct txAliDiff **differencesList)
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
    compareExonBases(psl, ix, gSeqWin, txSeq, cds, chromAcc, differencesList);
    checkIntrons(psl, ix, gSeqWin, txSeq, cds, chromAcc, differencesList);
    }
checkUnalignedTxSeq(psl, gSeqWin, txSeq, cds, differencesList);
}

static void sortAndDumpBedPlus(char *outBaseName, struct txAliDiff *diffList)
/* Sort the mostly-backwards bed list and and dump to file named outBaseName.bed. */
{
char fileName[PATH_LEN];
safef(fileName, sizeof(fileName), "%s.bed", outBaseName);
FILE *f = mustOpen(fileName, "w");
slReverse(&diffList);
slSort(&diffList, bedCmp);
struct txAliDiff *diffItem;
for (diffItem = diffList; diffItem != NULL;  diffItem = diffItem->next)
    {
    txAliDiffOutput(diffItem, f, '\t', '\n');
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
struct txAliDiff *differencesList = NULL;
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
            checkTranscript(psl, gSeqWin, txSeq, cds, &differencesList);
            }
        else
            {
            errAbort("No sequence found for psl->qName = '%s' - exiting.", psl->qName);
            }
        pslFree(&psl);
        }
    }
lineFileClose(&pslLf);
sortAndDumpBedPlus(outBaseName, differencesList);
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
