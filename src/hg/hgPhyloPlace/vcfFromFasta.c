/* Align user sequence to the reference genome and write VCF */

/* Copyright (C) 2020 The Regents of the University of California */

#include "common.h"
#include "fa.h"
#include "genoFind.h"
#include "iupac.h"
#include "parsimonyProto.h"
#include "phyloPlace.h"
#include "psl.h"
#include "trashDir.h"

double maxNs = 0.5;

// Using blat defaults for these:
int gfRepMatch = 1024*4;
char *gfOoc = NULL;
int gfStepSize = gfTileSize;
int gfOutMinIdentityPpt = 900;

// The default minScore for gfLongDnaInMem's 4500 base preferred size is 30, but we want way
// higher, expecting very similar sequences. -- But watch out for chunking effects in
// gfLongDnaInMem.  ... would be nice if gfLongDnaInMem scaled minMatch for the last little
// chunk.
int gfMinScore = 50;
// Normally this would be 12 for a genome with size 29903
// (see hgBlat.c findMinMatch... k = ceil(log4(genomeSize*250)))
// but we expect a really good alignment so we can crank it up.  (Again modulo chunking)
int gfIMinMatch = 30;


static void replaceNewickChars(char *seqName)
/* If seqName includes any characters with special meaning in the Newick format, then substitute
 * _ for each problematic character in seqName. */
{
if (strchr(seqName, '(') || strchr(seqName, ')') || strchr(seqName, ':') || strchr(seqName, ';') ||
    strchr(seqName, ','))
    {
     subChar(seqName, '(', '_');
     subChar(seqName, ')', '_');
     subChar(seqName, ':', '_');
     subChar(seqName, ';', '_');
     subChar(seqName, ',', '_');
     // Strip any underscores from beginning/end; strip double-underscores
     while (seqName[0] == '_')
         memmove(seqName, seqName+1, strlen(seqName)+1);
     char *p;
     while (*(p = seqName + strlen(seqName) - 1) == '_')
         *p = '\0';
     strSwapStrs(seqName, strlen(seqName), "__", "_");
    }
}

static struct seqInfo *checkSequences(struct dnaSeq *seqs, struct hash *treeNames,
                                      int minSeqSize, int maxSeqSize, struct slPair **retFailedSeqs)
/* Return a list of sequences that pass basic QC checks (appropriate size etc).
 * If any sequences have names that are already in the tree, add a prefix so usher doesn't
 * reject them.  If any sequence name contains characters that have special meaning in the Newick
 * format, replace them with something harmless.
 * Set retFailedSeqs to the list of sequences that failed checks. */
{
struct seqInfo *filteredSeqs = NULL;
struct hash *uniqNames = hashNew(0);
*retFailedSeqs = NULL;
struct dnaSeq *seq, *nextSeq;
for (seq = seqs;  seq != NULL;  seq = nextSeq)
    {
    boolean passes = TRUE;
    nextSeq = seq->next;
    if (seq->size < minSeqSize)
        {
        struct dyString *dy = dyStringCreate("Sequence %s has too few bases (%d, must have "
                                             "at least %d); skipping.",
                                             seq->name, seq->size, minSeqSize);
        slPairAdd(retFailedSeqs, dyStringCannibalize(&dy), seq);
        passes = FALSE;
        }
    else if (seq->size > maxSeqSize)
        {
        struct dyString *dy = dyStringCreate("Sequence %s has too many bases (%d, must have "
                                             "at most %d); skipping.",
                                             seq->name, seq->size, maxSeqSize);
        slPairAdd(retFailedSeqs, dyStringCannibalize(&dy), seq);
        passes = FALSE;
        }
    if (!passes)
        continue;
    // blat requires lower case sequence.  Lowercasing makes it easier to find n's and N's too:
    toLowerN(seq->dna, seq->size);
    // Many sequences begin and end with blocks of N bases; treat those differently than Ns in middle
    int nCountTotal = countChars(seq->dna, 'n');
    int nCountStart = 0;
    while (seq->dna[nCountStart] == 'n')
        nCountStart++;
    int nCountEnd = 0;
    if (nCountStart < seq->size)
        {
        while (seq->dna[seq->size - 1 - nCountEnd] == 'n')
            nCountEnd++;
        }
    int nCountMiddle = nCountTotal - (nCountStart + nCountEnd);
    int effectiveLength = seq->size - (nCountStart + nCountEnd);
    if (effectiveLength < minSeqSize)
        {
        struct dyString *dy = dyStringCreate("Sequence %s has too few bases (%d excluding %d Ns "
                                             "at beginning and %d Ns at end), "
                                             "must have at least %d); skipping.",
                                             seq->name, effectiveLength, nCountStart, nCountEnd,
                                             minSeqSize);
        slPairAdd(retFailedSeqs, dyStringCannibalize(&dy), seq);
        passes = FALSE;
        }
    if (!passes)
        continue;
    if (nCountMiddle > maxNs * effectiveLength)
        {
        struct dyString *dy = dyStringCreate("Sequence %s has too many N bases (%d out of %d "
                                             "> %0.2f); skipping.",
                                             seq->name, nCountMiddle, effectiveLength, maxNs);
        slPairAdd(retFailedSeqs, dyStringCannibalize(&dy), seq);
        passes = FALSE;
        }
    int ambigCount = 0;
    int i;
    for (i = 0;  i < seq->size;  i++)
        if (seq->dna[i] != 'n' && isIupacAmbiguous(seq->dna[i]))
            ambigCount++;
    if (passes)
        {
        replaceNewickChars(seq->name);
        if (hashLookup(uniqNames, seq->name))
            {
            struct dyString *dy = dyStringCreate("Sequence name '%s' has already been used; "
                                                 "ignoring subsequent usage "
                                                 "(%d bases, %d N's, %d ambiguous).",
                                                 seq->name, seq->size, nCountTotal, ambigCount);
            slPairAdd(retFailedSeqs, dyStringCannibalize(&dy), seq);
            }
        else
            {
            if (treeNames &&
                (isInternalNodeName(seq->name, 0) || hashLookup(treeNames, seq->name)))
                {
                // usher will reject any sequence whose name is already in the tree, even a
                // numeric internal node name.  Add a prefix so usher won't reject sequence.
                char newName[strlen(seq->name)+32];
                safef(newName, sizeof newName, "uploaded_%s", seq->name);
                freeMem(seq->name);
                seq->name = cloneString(newName);
                }
            struct seqInfo *si;
            AllocVar(si);
            si->seq = seq;
            si->nCountStart = nCountStart;
            si->nCountMiddle = nCountMiddle;
            si->nCountEnd = nCountEnd;
            si->ambigCount = ambigCount;
            hashAdd(uniqNames, seq->name, NULL);
            slAddHead(&filteredSeqs, si);
            }
        }
    }
slReverse(&filteredSeqs);
slReverse(retFailedSeqs);
hashFree(&uniqNames);
return filteredSeqs;
}

static struct psl *alignSequences(struct dnaSeq *refGenome, struct seqInfo *seqs,
                                  int *pStartTime)
/* Use BLAT server to align seqs to reference; keep only the best alignment for each seq.
 * (In normal usage, there should be one very good alignment per seq.) */
{
struct psl *alignments = NULL;
struct genoFind *gf = gfIndexSeq(refGenome, gfIMinMatch, gfMaxGap, gfTileSize, gfRepMatch, gfOoc,
                                 FALSE, FALSE, FALSE, gfStepSize, FALSE);
reportTiming(pStartTime, "gfIndexSeq");
struct tempName pslTn;
trashDirFile(&pslTn, "ct", "pp", ".psl");
FILE *f = mustOpen(pslTn.forCgi, "w");
struct gfOutput *gvo = gfOutputPsl(gfOutMinIdentityPpt, FALSE, FALSE, f, FALSE, TRUE);
struct seqInfo *si;
for (si = seqs;  si != NULL;  si = si->next)
    {
    // Positive strand only; we're expecting a mostly complete match to the reference
    gfLongDnaInMem(si->seq, gf, FALSE, gfMinScore, NULL, gvo, FALSE, FALSE);
    }
reportTiming(pStartTime, "gfLongDnaInMem all");
gfOutputFree(&gvo);
carefulClose(&f);
alignments = pslLoadAll(pslTn.forCgi);
reportTiming(pStartTime, "load PSL file");
//#*** TODO: keep only the best alignment for each seq.
return alignments;
}

struct psl *checkAlignments(struct psl *psls, struct seqInfo *userSeqs,
                            struct slPair **retFailedPsls)
/* No thresholds applied at this point - just makes sure there aren't multiple PSLs per seq. */
{
struct psl *filteredPsls = NULL;
*retFailedPsls = NULL;
struct hash *userSeqsByName = hashNew(0);
struct seqInfo *si;
for (si = userSeqs;  si != NULL;  si = si->next)
    hashAdd(userSeqsByName, si->seq->name, si);
struct hash *alignedSeqs = hashNew(0);
struct psl *psl, *nextPsl;
for (psl = psls;  psl != NULL;  psl = nextPsl)
    {
    nextPsl = psl->next;
    boolean passes = TRUE;
    struct psl *otherPsl = hashFindVal(alignedSeqs, psl->qName);
    if (otherPsl)
        {
        struct dyString *dy = dyStringCreate("Warning: multiple alignments to reference found for "
                                             "sequence %s (%d-%d and %d-%d).  "
                                             "Skipping alignment of %d-%d",
                                              psl->qName, otherPsl->qStart, otherPsl->qEnd,
                                              psl->qStart, psl->qEnd, psl->qStart, psl->qEnd);
        slPairAdd(retFailedPsls, dyStringCannibalize(&dy), psl);
        passes = FALSE;
        }
    else
        hashAdd(alignedSeqs, psl->qName, psl);
    if (passes)
        {
        si = hashFindVal(userSeqsByName, psl->qName);
        if (si)
            si->psl = psl;
        else
            warn("Aligned sequence name '%s' does not match any input sequence name", psl->qName);
        slAddHead(&filteredPsls, psl);
        }
    }
hashFree(&alignedSeqs);
hashFree(&userSeqsByName);
slReverse(&filteredPsls);
slReverse(retFailedPsls);
return filteredPsls;
}

static void removeUnalignedSeqs(struct seqInfo **pSeqs, struct psl *alignments,
                                struct slPair **pFailedSeqs)
/* If any seqs were not aligned, then remove them from *pSeqs and add them to *pFailedSeqs. */
{
struct seqInfo *alignedSeqs = NULL;
struct slPair *newFailedSeqs = NULL;
struct seqInfo *seq, *nextSeq;
for (seq = *pSeqs;  seq != NULL;  seq = nextSeq)
    {
    nextSeq = seq->next;
    boolean found = FALSE;
    struct psl *psl;
    for (psl = alignments;  psl != NULL;  psl = psl->next)
        {
        if (sameString(psl->qName, seq->seq->name))
            {
            found = TRUE;
            break;
            }
        }
    if (found)
        slAddHead(&alignedSeqs, seq);
    else
        {
        struct dyString *dy = dyStringCreate("Sequence %s could not be aligned to the reference; "
                                             "skipping", seq->seq->name);
        slPairAdd(&newFailedSeqs, dyStringCannibalize(&dy), seq);
        }
    }
slReverse(&alignedSeqs);
slReverse(&newFailedSeqs);
*pSeqs = alignedSeqs;
*pFailedSeqs = slCat(*pFailedSeqs, newFailedSeqs);
}

struct snvInfo
/* Summary of SNV with sample genotypes (position and samples are externally defined) */
    {
    int altCount;     // number of alternate alleles
    int altAlloced;   // number of allocated array elements for alt alleles
    char *altBases;   // alternate allele values (not '\0'-terminated string)
    int *altCounts;   // number of occurrences of each alt allele
    int *genotypes;   // -1 for no-call, 0 for ref, otherwise 1+index in altBases
    int unkCount;     // number of no-calls (genotype is -1)
    char refBase;     // reference allele
    };

static struct snvInfo *snvInfoNew(char refBase, char altBase, int gtIx, int gtCount)
/* Alloc & return a new snvInfo for ref & alt.  If alt == ref, just record the genotype call. */
{
struct snvInfo *snv = NULL;
AllocVar(snv);
snv->refBase = refBase;
snv->altAlloced = 4;
AllocArray(snv->altBases, snv->altAlloced);
AllocArray(snv->altCounts, snv->altAlloced);
AllocArray(snv->genotypes, gtCount);
// Initialize snv->genotypes to all no-call
int i;
for (i = 0;  i < gtCount;  i++)
    snv->genotypes[i] = -1;
if (altBase == refBase)
    snv->genotypes[gtIx] = 0;
else if (altBase == 'n' || altBase == '-')
    {
    snv->genotypes[gtIx] = -1;
    snv->unkCount = 1;
    }
else
    {
    snv->altCount = 1;
    snv->altBases[0] = altBase;
    snv->altCounts[0] = 1;
    snv->genotypes[gtIx] = 1;
    }
return snv;
}

static void snvInfoAddCall(struct snvInfo *snv, char refBase, char altBase, int gtIx)
/* Add a new genotype call to snv. */
{
if (refBase != snv->refBase)
    errAbort("snvInfoAddCall: snv->refBase is %c but incoming refBase is %c", snv->refBase, refBase);
// If we have already recorded a call for this sample at this position, then there's an overlap
// in alignments (perhaps a deletion in q); we're just looking for SNVs so ignore this base.
if (snv->genotypes[gtIx] >= 0)
    return;
if (altBase == refBase)
    snv->genotypes[gtIx] = 0;
else if (altBase == 'n' || altBase == '-')
    {
    snv->genotypes[gtIx] = -1;
    snv->unkCount++;
    }
else
    {
    int altIx;
    for (altIx = 0;  altIx < snv->altCount;  altIx++)
        if (altBase == snv->altBases[altIx])
            break;
    if (altIx < snv->altCount)
        snv->altCounts[altIx]++;
    else
        {
        // We haven't seen this alt before; record the new alt, expanding arrays if necessary.
        if (snv->altCount == snv->altAlloced)
            {
            int newAlloced = snv->altAlloced * 2;
            ExpandArray(snv->altBases, snv->altAlloced, newAlloced);
            ExpandArray(snv->altCounts, snv->altAlloced, newAlloced);
            snv->altAlloced = newAlloced;
            }
        snv->altCount++;
        snv->altBases[altIx] = altBase;
        snv->altCounts[altIx] = 1;
        }
    snv->genotypes[gtIx] = 1 + altIx;
    }
}

static struct seqInfo *mustFindSeqAndIx(char *qName, struct seqInfo *querySeqs, int *retGtIx)
/* Find qName and its index in querySeqs or errAbort. */
{
struct seqInfo *qSeq;
int gtIx;
for (gtIx = 0, qSeq = querySeqs;  qSeq != NULL;  gtIx++, qSeq = qSeq->next)
    if (sameString(qName, qSeq->seq->name))
        break;
if (!qSeq)
    errAbort("PSL query '%s' not found in input sequences", qName);
*retGtIx = gtIx;
return qSeq;
}

static int addCall(struct snvInfo **snvsByPos, int tStart, char refBase, char altBase, int gtIx,
                    int gtCount)
/* If refBase is not N, add a call (or no-call if altBase is N or '-') to snvsByPos[tStart].
 * Return the VCF genotype code (-1 for missing/N, 0 for ref, 1 for first alt etc). */
{
if (refBase != 'n')
    {
    if (snvsByPos[tStart] == NULL)
        snvsByPos[tStart] = snvInfoNew(refBase, altBase, gtIx, gtCount);
    else
        snvInfoAddCall(snvsByPos[tStart], refBase, altBase, gtIx);
    return snvsByPos[tStart]->genotypes[gtIx];
    }
return -1;
}

static void extractSnvs(struct psl *psls, struct dnaSeq *ref, struct seqInfo *querySeqs,
                        struct snvInfo **snvsByPos, struct slName **maskSites)
/* For each PSL, find single-nucleotide differences between ref and query (one of querySeqs),
 * building up target position-indexed array of snvInfo w/genotypes in same order as querySeqs.
 * Add explicit no-calls for unaligned positions that are not masked. */
{
int gtCount = slCount(querySeqs);
struct psl *psl;
for (psl = psls;  psl != NULL;  psl = psl->next)
    {
    if (!sameString(psl->strand, "+"))
        errAbort("extractSnvs: expected strand to be '+' but it's '%s'", psl->strand);
    int gtIx;
    struct seqInfo *qSeq = mustFindSeqAndIx(psl->qName, querySeqs, &gtIx);
    // Add no-calls for unaligned bases before first block
    int tStart;
    for (tStart = 0;  tStart < psl->tStart;  tStart++)
        {
        if (!maskSites[tStart])
            {
            char refBase = ref->dna[tStart];
            addCall(snvsByPos, tStart, refBase, '-', gtIx, gtCount);
            }
        }
    int blkIx;
    for (blkIx = 0;  blkIx < psl->blockCount;  blkIx++)
        {
        // Add genotypes for aligned bases
        tStart = psl->tStarts[blkIx];
        int tEnd = tStart + psl->blockSizes[blkIx];
        int qStart = psl->qStarts[blkIx];
        for (;  tStart < tEnd;  tStart++, qStart++)
            {
            char refBase = ref->dna[tStart];
            char altBase = qSeq->seq->dna[qStart];
            if (!altBase)
                errAbort("nil altBase at %s:%d", qSeq->seq->name, qStart);
            int gt = addCall(snvsByPos, tStart, refBase, altBase, gtIx, gtCount);
            if (gt > 0)
                {
                struct singleNucChange *snc = sncNew(tStart, refBase, '\0', altBase);
                struct slName *maskedReasons = maskSites[tStart];
                if (maskedReasons)
                    {
                    slAddHead(&qSeq->maskedSncList, snc);
                    slAddHead(&qSeq->maskedReasonsList, slRefNew(maskedReasons));
                    }
                else
                    slAddHead(&qSeq->sncList, snc);
                }
            }
        if (blkIx < psl->blockCount - 1)
            {
            // Add no-calls for unaligned bases between this block and the next
            for (;  tStart < psl->tStarts[blkIx+1];  tStart++)
                {
                if (!maskSites[tStart])
                    {
                    char refBase = ref->dna[tStart];
                    addCall(snvsByPos, tStart, refBase, '-', gtIx, gtCount);
                    }
                }
            }
        }
    // Add no-calls for unaligned bases after last block
    for (tStart = psl->tEnd;  tStart < ref->size;  tStart++)
        {
        if (!maskSites[tStart])
            {
            char refBase = ref->dna[tStart];
            addCall(snvsByPos, tStart, refBase, '-', gtIx, gtCount);
            }
        }
    slReverse(&qSeq->sncList);
    }
}

static void writeVcfSnv(struct snvInfo **snvsByPos, int gtCount, char *chrom, int chromStart,
                        FILE *f)
/* If snvsByPos[chromStart] is not NULL, write out a VCF data row with genotypes. */
{
struct snvInfo *snv = snvsByPos[chromStart];
if (snv && (snv->altCount > 0 || snv->unkCount > 0))
    {
    if (snv->altCount == 0)
        snv->altBases[0] = '*';
    int pos = chromStart+1;
    fprintf(f, "%s\t%d\t%c%d%c",
            chrom, pos, toupper(snv->refBase), pos, toupper(snv->altBases[0]));
    int altIx;
    for (altIx = 1;  altIx < snv->altCount;  altIx++)
        fprintf(f, ",%c%d%c", toupper(snv->refBase), pos, toupper(snv->altBases[altIx]));
    fprintf(f, "\t%c\t%c", toupper(snv->refBase), toupper(snv->altBases[0]));
    for (altIx = 1;  altIx < snv->altCount;  altIx++)
        fprintf(f, ",%c", toupper(snv->altBases[altIx]));
    fputs("\t.\t.\t", f);
    fprintf(f, "AC=%d", snv->altCounts[0]);
    for (altIx = 1;  altIx < snv->altCount;  altIx++)
        fprintf(f, ",%d", snv->altCounts[altIx]);
    int callCount = gtCount;
    int gtIx;
    for (gtIx = 0;  gtIx < gtCount;  gtIx++)
        if (snv->genotypes[gtIx] < 0)
            callCount--;
    fprintf(f, ";AN=%d\tGT", callCount);
    for (gtIx = 0;  gtIx < gtCount;  gtIx++)
        if (snv->genotypes[gtIx] < 0)
            fputs("\t.", f);
        else
            fprintf(f, "\t%d", snv->genotypes[gtIx]);
    fputc('\n', f);
    }
}

static void writeSnvsToVcfFile(struct snvInfo **snvsByPos, struct dnaSeq *ref, char **sampleNames,
                               int sampleCount, struct slName **maskSites, char *vcfFileName)
/* Given an array of SNVs (one per base of ref, probably mostly NULL) and sequence of samples,
 * write a VCF header and rows with genotypes. */
{
FILE *f = mustOpen(vcfFileName, "w");
fprintf(f, "##fileformat=VCFv4.2\n");
fprintf(f, "##reference=%s\n", ref->name);
fprintf(f, "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\tFORMAT");
int i;
for (i = 0;  i < sampleCount;  i++)
    fprintf(f, "\t%s", sampleNames[i]);
fputc('\n', f);
int chromStart;
for (chromStart = 0;  chromStart < ref->size;  chromStart++)
    if (!maskSites[chromStart])
        writeVcfSnv(snvsByPos, sampleCount, ref->name, chromStart, f);
carefulClose(&f);
}

static void pslSnvsToVcfFile(struct psl *psls, struct dnaSeq *ref, struct seqInfo *querySeqs,
                             char *vcfFileName, struct slName **maskSites)
/* Find single-nucleotide differences between each query sequence and reference, and
 * write out a VCF file with genotype columns for the queries. */
{
struct snvInfo **snvsByPos = NULL;
AllocArray(snvsByPos, ref->size);
extractSnvs(psls, ref, querySeqs, snvsByPos, maskSites);
int sampleCount = slCount(querySeqs);
char *sampleNames[sampleCount];
struct seqInfo *qSeq;
int i;
for (i = 0, qSeq = querySeqs;  i < sampleCount;  i++, qSeq = qSeq->next)
    sampleNames[i] = qSeq->seq->name;
writeSnvsToVcfFile(snvsByPos, ref, sampleNames, sampleCount, maskSites, vcfFileName);
freeMem(snvsByPos);
}

static void analyzeGaps(struct seqInfo *filteredSeqs, struct dnaSeq *refGenome)
/* Tally up actual insertions and deletions in each psl; ignore skipped N bases. */
{
struct seqInfo *si;
for (si = filteredSeqs;  si != NULL;  si = si->next)
    {
    struct psl *psl = si->psl;
    if (psl && (psl->qBaseInsert || psl->tBaseInsert))
        {
        struct dyString *dyIns = dyStringNew(0);
        struct dyString *dyDel = dyStringNew(0);
        int insBases = 0, delBases = 0;
        int ix;
        for (ix = 0;  ix < psl->blockCount - 1;  ix++)
            {
            int qGapStart = psl->qStarts[ix] + psl->blockSizes[ix];
            int qGapEnd = psl->qStarts[ix+1];
            int qGapLen = qGapEnd - qGapStart;
            int tGapStart = psl->tStarts[ix] + psl->blockSizes[ix];
            int tGapEnd = psl->tStarts[ix+1];
            int tGapLen = tGapEnd - tGapStart;
            if (qGapLen > tGapLen)
                {
                int insLen = qGapLen - tGapLen;
                insBases += insLen;
                if (isNotEmpty(dyIns->string))
                    dyStringAppend(dyIns, ", ");
                if (insLen <= 12)
                    {
                    char insSeq[insLen+1];
                    safencpy(insSeq, sizeof insSeq, si->seq->dna + qGapEnd - insLen, insLen);
                    touppers(insSeq);
                    dyStringPrintf(dyIns, "%d-%d:%s", tGapEnd, tGapEnd+1, insSeq);
                    }
                else
                    dyStringPrintf(dyIns, "%d-%d:%d bases", tGapEnd, tGapEnd+1, insLen);
                }
            else if (tGapLen > qGapLen)
                {
                int delLen = tGapLen - qGapLen;;
                delBases += delLen;
                if (isNotEmpty(dyDel->string))
                    dyStringAppend(dyDel, ", ");
                if (delLen <= 12)
                    {
                    char delSeq[delLen+1];
                    safencpy(delSeq, sizeof delSeq, refGenome->dna + tGapEnd - delLen, delLen);
                    touppers(delSeq);
                    dyStringPrintf(dyDel, "%d-%d:%s", tGapEnd - delLen + 1, tGapEnd, delSeq);
                    }
                else
                    dyStringPrintf(dyDel, "%d-%d:%d bases", tGapEnd - delLen + 1, tGapEnd, delLen);
                }
            }
        si->insBases = insBases;
        si->delBases = delBases;
        si->insRanges = dyStringCannibalize(&dyIns);
        si->delRanges = dyStringCannibalize(&dyDel);
        }
    }
}

struct tempName *vcfFromFasta(struct lineFile *lf, char *db, struct dnaSeq *refGenome,
                              struct slName **maskSites, struct hash *treeNames,
                              struct slName **retSampleIds, struct seqInfo **retSeqInfo,
                              struct slPair **retFailedSeqs, struct slPair **retFailedPsls,
                              int *pStartTime)
/* Read in FASTA from lf and make sure each item has a reasonable size and not too high
 * percentage of N's.  Align to reference, extract SNVs from alignment, and save as VCF
 * with sample genotype columns. */
{
struct tempName *tn = NULL;
struct slName *sampleIds = NULL;
struct dnaSeq *allSeqs = faReadAllMixedInLf(lf);
int minSeqSize = 0, maxSeqSize = 0;
// Default to SARS-CoV-2 or hMPXV values if setting is missing from config.ra.
char *minSeqSizeSetting = phyloPlaceDbSetting(db, "minSeqSize");
if (isEmpty(minSeqSizeSetting))
    minSeqSize = sameString(db, "wuhCor1") ? 10000 : 100000;
else
    minSeqSize = atoi(minSeqSizeSetting);
char *maxSeqSizeSetting = phyloPlaceDbSetting(db, "maxSeqSize");
if (isEmpty(maxSeqSizeSetting))
    maxSeqSize = sameString(db, "wuhCor1") ? 35000 : 220000;
else
    maxSeqSize = atoi(maxSeqSizeSetting);
struct seqInfo *filteredSeqs = checkSequences(allSeqs, treeNames, minSeqSize, maxSeqSize,
                                              retFailedSeqs);
reportTiming(pStartTime, "read and check uploaded FASTA");
if (filteredSeqs)
    {
    struct psl *alignments = alignSequences(refGenome, filteredSeqs, pStartTime);
    struct psl *filteredAlignments = checkAlignments(alignments, filteredSeqs, retFailedPsls);
    removeUnalignedSeqs(&filteredSeqs, filteredAlignments, retFailedSeqs);
    if (filteredAlignments)
        {
        AllocVar(tn);
        trashDirFile(tn, "ct", "pp", ".vcf");
        pslSnvsToVcfFile(filteredAlignments, refGenome, filteredSeqs, tn->forCgi, maskSites);
        struct psl *psl;
        for (psl = filteredAlignments;  psl != NULL;  psl = psl->next)
            slNameAddHead(&sampleIds, psl->qName);
        analyzeGaps(filteredSeqs, refGenome);
        slReverse(&sampleIds);
        reportTiming(pStartTime, "write VCF for uploaded FASTA");
        }
    }
*retSampleIds = sampleIds;
*retSeqInfo = filteredSeqs;
return tn;
}
 
