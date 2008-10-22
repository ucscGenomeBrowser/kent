/* output - output alignments to files in various formats. */

#include "common.h"
#include "dystring.h"
#include "dnautil.h"
#include "dnaSeq.h"
#include "maf.h"
#include "chain.h"
#include "splix.h"
#include "splat.h"

static int tOffsetToChromIx(struct splix *splix, bits32 tOffset)
/* Figure out index of chromosome containing tOffset */
{
int i;
int chromCount = splix->header->chromCount;
/* TODO - convert to binary search */
for (i=0; i<chromCount; ++i)
    {
    int chromStart = splix->chromOffsets[i];
    int chromEnd = chromStart + splix->chromSizes[i];
    if (tOffset >= chromStart && tOffset < chromEnd)
        return i;
    }
errAbort("tOffset %d out of range\n", tOffset);
return -1;
}

static void splatOutputMaf(struct splatAlign *ali, 
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, struct splix *splix, FILE *f)
/* Output alignment to maf file. */
{
struct cBlock *bStart = ali->blockList;
struct cBlock *bEnd = slLastEl(bStart);
int chromIx = tOffsetToChromIx(splix, bStart->tStart);
char strand = ali->strand;
struct dnaSeq *qSeq = (strand == '-' ? qSeqR : qSeqF);

/* Create symbols - DNA alignment with dashes for inserts - a long process. */
struct dyString *qSym = dyStringNew(0), *tSym = dyStringNew(0);

/* Loop through blocks... */
struct cBlock *b, *bNext;
for (b = bStart; b != NULL; b = bNext)
    {
    /* Output letters in block. */
    int bSize = b->tEnd - b->tStart;
    DNA *qDna = qSeq->dna + b->qStart;
    DNA *tDna = splix->allDna + b->tStart;
    int i;
    for (i=0; i<bSize; ++i)
	{
	dyStringAppendC(qSym, qDna[i]);
	dyStringAppendC(tSym, tDna[i]);
	}

    /* If got another block, output gap to it. */
    bNext = b->next;
    if (bNext != NULL)
        {
	int qGapSize = bNext->qStart - b->qEnd;
	if (qGapSize > 0)
	    {
	    for (i=0; i<qGapSize; ++i)
		{
		dyStringAppendC(qSym, qDna[bSize+i]);
		dyStringAppendC(tSym, '-');
		}
	    }
	int tGapSize = bNext->tStart - b->tEnd;
	if (tGapSize > 0)
	    {
	    for (i=0; i<tGapSize; ++i)
		{
		dyStringAppendC(tSym, tDna[bSize+i]);
		dyStringAppendC(qSym, '-');
		}
	    }
	}
    }

int symSize = tSym->stringSize;

/* Build up a maf component for query */
struct mafComp *qComp;
AllocVar(qComp);
qComp->src = cloneString(qSeq->name);
qComp->srcSize = qSeq->size;
qComp->strand = strand;
qComp->start = bStart->qStart;
qComp->size = bEnd->qEnd - bStart->qStart;
qComp->text = dyStringCannibalize(&qSym);

/* Build up a maf component for target */
struct mafComp *tComp;
AllocVar(tComp);
tComp->src = cloneString(splix->chromNames[chromIx]);
tComp->srcSize = splix->chromSizes[chromIx];
tComp->strand = '+';
tComp->start = bStart->tStart - splix->chromOffsets[chromIx];
tComp->size = bEnd->tEnd - bStart->tStart;
tComp->text = dyStringCannibalize(&tSym);

/* Build up a maf alignment structure . */
struct mafAli *maf;
AllocVar(maf);
maf->score = ali->score;
maf->textSize = symSize;
maf->components = tComp;
tComp->next = qComp;

/* Save it to file, and free it up. */
mafWrite(f, maf);
mafAliFree(&maf);
}

static void splatOutputMafs(struct splatAlign *aliList, 
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, struct splix *splix, FILE *f)
/* Output alignment list to maf file. */
{
struct splatAlign *ali;
for (ali = aliList; ali != NULL; ali = ali->next)
    splatOutputMaf(ali, qSeqF, qSeqR, splix, f);
}


static void splatOutputPsl(struct splatAlign *ali, 
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, struct splix *splix, FILE *f)
/* Output alignment to psl file. */
{
struct cBlock *bStart = ali->blockList;
struct cBlock *bEnd = slLastEl(bStart);
int chromIx = tOffsetToChromIx(splix, bStart->tStart);
char strand = ali->strand;
struct dnaSeq *qSeq = (strand == '-' ? qSeqR : qSeqF);

/* Point to DNA */
DNA *qDna = qSeq->dna;
DNA *tDna = splix->allDna;

/* Loop through blocks and calculate summary fields. */
unsigned match = 0;	/* Number of bases that match that aren't repeats */
unsigned misMatch = 0;	/* Number of bases that don't match */
unsigned repMatch = 0;	/* Number of bases that match but are part of repeats */
unsigned nCount = 0;	/* Number of 'N' bases */
unsigned qNumInsert = 0;	/* Number of inserts in query */
int qBaseInsert = 0;	/* Number of bases inserted in query */
unsigned tNumInsert = 0;	/* Number of inserts in target */
int tBaseInsert = 0;	/* Number of bases inserted in target */
struct cBlock *b, *bNext;
for (b = bStart; b != NULL; b = bNext)
    {
    /* Figure out block size, mismatch count, and match count. */
    int bSize = b->tEnd - b->tStart;
    int miss = countDnaDiffs(qDna + b->qStart, tDna + b->tStart, bSize);
    match += bSize - miss;
    misMatch += miss;

    /* If not last block, figure out gap info. */
    bNext = b->next;
    if (bNext != NULL)
        {
	int qGapSize = bNext->qStart - b->qEnd;
	if (qGapSize > 0)
	    {
	    qNumInsert += 1;
	    qBaseInsert += qGapSize;
	    }
	int tGapSize = bNext->tStart - b->tEnd;
	if (tGapSize > 0)
	    {
	    tNumInsert += 1;
	    tBaseInsert += tGapSize;
	    }
	}
    }

/* Write out summary fields */
fprintf(f, "%d\t", match);
fprintf(f, "%d\t", misMatch);
fprintf(f, "%d\t", repMatch);
fprintf(f, "%d\t", nCount);
fprintf(f, "%d\t", qNumInsert);
fprintf(f, "%d\t", qBaseInsert);
fprintf(f, "%d\t", tNumInsert);
fprintf(f, "%d\t", tBaseInsert);

/* Write out basic info on query */
fprintf(f, "%c\t", strand);
fprintf(f, "%s\t", qSeq->name);
fprintf(f, "%d\t", qSeq->size);

/* Handle qStart/qEnd field, which requires some care on - strand. */
int qStart = bStart->qStart;
int qEnd = bEnd->qEnd;
if (strand == '-')
    reverseIntRange(&qStart, &qEnd, qSeq->size);
fprintf(f, "%d\t%d\t", qStart, qEnd);

/* Target is always on plus strand, so easier. */
fprintf(f, "%s\t", splix->chromNames[chromIx]);		// tName
fprintf(f, "%d\t", (int)splix->chromSizes[chromIx]);	// tSize
fprintf(f, "%d\t", bStart->tStart);			// tStart
fprintf(f, "%d\t", bEnd->tEnd);				// tEnd

/* Now deal with block fields. */
fprintf(f, "%d\t", slCount(bStart));

/* blockSizes */
for (b = bStart; b != NULL; b = b->next)
    fprintf(f, "%d,", b->tEnd - b->tStart);
fputc('\t', f);

/* qStarts */
for (b = bStart; b != NULL; b = b->next)
    fprintf(f, "%d,", b->qStart);
fputc('\t', f);

/* tStarts */
for (b = bStart; b != NULL; b = b->next)
    fprintf(f, "%d,", b->tStart);
fputc('\n', f);
}

static void splatOutputPsls(struct splatAlign *aliList, 
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, struct splix *splix, FILE *f)
/* Output alignment list to psl file. */
{
struct splatAlign *ali;
for (ali = aliList; ali != NULL; ali = ali->next)
    splatOutputPsl(ali, qSeqF, qSeqR, splix, f);
}

static void dnaOutUpperMatch(DNA a, DNA b, FILE *f)
/* Write out a in upper case if it matches b, otherwise in lower case. */
{
if (a == b)
    a = toupper(a);
else
    a = tolower(a);
fputc(a, f);
}

static void splatOutputSplat(struct splatAlign *ali, int mapCount,
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, struct splix *splix, FILE *f)
/* Output alignment  to splat format output file. (Tag-align plus query name) */
{
struct cBlock *bStart = ali->blockList;
struct cBlock *bEnd = slLastEl(bStart);
int chromIx = tOffsetToChromIx(splix, bStart->tStart);
char strand = ali->strand;
struct dnaSeq *qSeq = (strand == '-' ? qSeqR : qSeqF);

/* Write chrom chromStart chromEnd */
fprintf(f, "%s\t", splix->chromNames[chromIx]);
int chromOffset = splix->chromOffsets[chromIx];
fprintf(f, "%d\t%d\t", bStart->tStart - chromOffset, bEnd->tEnd - chromOffset);

/* Write sequence including gaps (- for deletions, ^ for inserts) */
DNA *qDna = qSeq->dna;
DNA *tDna = splix->allDna;
struct cBlock *b, *bNext;
for (b = bStart; b != NULL; b = bNext)
    {
    /* Output DNA */
    int bSize = b->tEnd - b->tStart;
    int i;
    for (i=0; i<bSize; ++i)
        dnaOutUpperMatch(qDna[b->qStart + i], tDna[b->tStart + i], f);

    /* Output gaps if any. */
    bNext = b->next;
    if (bNext != NULL)
        {
	int qGapSize = bNext->qStart - b->qEnd;
	int tGapSize = bNext->tStart - b->tEnd;
	for (i=0; i<qGapSize; ++i)
	    {
	    fputc('^', f);
	    fputc(tolower(qDna[b->qEnd+i]), f);
	    }
	for (i=0; i<tGapSize; ++i)
	    fputc('-', f);
	}
    }
fputc('\t', f);

fprintf(f, "%d\t", 1000/mapCount);
fprintf(f, "%c\t", strand);
fprintf(f, "%s\n", qSeq->name);
}

static void splatOutputSplats(struct splatAlign *aliList, 
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, struct splix *splix, FILE *f)
/* Output list of alignments to splat format output file. (Tag-align plus query name)*/
{
int mapCount = slCount(aliList);
struct splatAlign *ali;
for (ali = aliList; ali != NULL; ali = ali->next)
    splatOutputSplat(ali, mapCount, qSeqF, qSeqR, splix, f);
}

static struct splatAlign *tagToAlign(struct splatTag *tag, 
	struct dnaSeq *qSeqF,  struct dnaSeq *qSeqR,
	struct splix *splix)
/* Convert splatTag to splatAlign on the basic level.  Don't (yet) 
 * fill in score field or do extension. */
{
/* Allocate and fill  out cBlock structure on first alignment block. */
char strand = tag->strand;
struct dnaSeq *qSeq = (strand == '-' ? qSeqF : qSeqR);
DNA *q = qSeq->dna;
DNA *t = splix->allDna;
struct cBlock *block1;
AllocVar(block1);
block1->qStart = tag->q1;
block1->qEnd = tag->q1 + tag->size1;
block1->tStart = tag->t1;
block1->tEnd = tag->t1 + tag->size1;

/* Allocate and fill out splatAlign struct. */
struct splatAlign *align;
AllocVar(align);
align->strand = tag->strand;
align->blockList = block1;
align->score = 2*qSeq->size - tag->divergence;	// TODO - calculate with matrix

/* If need be add second block to alignment. */
if (tag->size2 > 0)
    {
    struct cBlock *block2;
    AllocVar(block2);
    block2->qStart = tag->q2;
    block2->qEnd = tag->q2 + tag->size2;
    block2->tStart = tag->t2;
    block2->tEnd = tag->t2 + tag->size2;
    block1->next = block2;
    }
return align;
}


struct splatAlign *tagListToAliList(struct splatTag *tagList, 
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, struct splix *splix)
/* Convert a list of tags to a list of alignments. */
{
struct splatTag *tag;
struct splatAlign *aliList = NULL;
for (tag = tagList; tag != NULL; tag = tag->next)
    {
    struct splatAlign *ali = tagToAlign(tag, qSeqF, qSeqR, splix);
    slAddHead(&aliList, ali);
    }
slReverse(&aliList);
return aliList;
}

void splatOutTags(struct splatTag *tagList, char *outType,
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, struct splix *splix, FILE *f)
/* Output tag match */
{
/* TODO: Will move the tag to ali conversion elsewhere soon, this is just an intermediate
 * stage in refactoring to use aliList. */
struct splatAlign *aliList = tagListToAliList(tagList, qSeqF, qSeqR, splix);
if (sameString(outType, "maf"))
    splatOutputMafs(aliList, qSeqF, qSeqR, splix, f);
else if (sameString(outType, "psl"))
    splatOutputPsls(aliList, qSeqF, qSeqR, splix, f);
else if (sameString(outType, "splat"))
    splatOutputSplats(aliList, qSeqF, qSeqR, splix, f);
}

void splatOutHeader(char *target, char *query, char *outType, FILE *f)
/* Output file header if any.  Also check and abort if outType is not supported. */
{
if (sameString(outType, "maf"))
    {
    fprintf(f, "##maf version=1 program=splat\n");
    fprintf(f, "# splat.v%s %s %s\n", version, target, query);
    fprintf(f, "\n");
    }
else if (sameString(outType, "psl"))
    {
    }
else if (sameString(outType, "splat"))
    ;
else
    errAbort("Unrecognized output type %s", outType);
}
