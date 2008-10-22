/* output - output alignments to files in various formats. */

#include "common.h"
#include "dystring.h"
#include "dnautil.h"
#include "dnaSeq.h"
#include "maf.h"
#include "fuzzyFind.h"
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

static void splatOutputMafs(struct splatTag *tagList, 
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, struct splix *splix, FILE *f)
/* Output tags to maf file. */
{
struct splatTag *tag;
for (tag = tagList; tag != NULL; tag = tag->next)
    {
    int chromIx = tOffsetToChromIx(splix, tag->t1);
    /* Handle reverse-complementing if need be. */
    char strand = tag->strand;
    struct dnaSeq *qSeq = (strand == '-' ? qSeqR : qSeqF);

    /* Create symbols - DNA alignment with dashes for inserts - a long process. */
    struct dyString *qSym = dyStringNew(0), *tSym = dyStringNew(0);

    /* Fetch a bunch of vars from tag. */
    int q1 = tag->q1, q2 = tag->q2;
    int t1 = tag->t1, t2 = tag->t2;
    int size1 = tag->size1;
    int size2 = tag->size2;

    /* Normalize q2,t2 to make life easier in single block case */
    if (size2 == 0)
	{
	q2 = q1 + size1;
	t2 = t1 + size1;
	}

    /* Output first block */
    DNA *qDna = qSeq->dna + q1;
    DNA *tDna = splix->allDna + t1;
    int i;
    for (i=0; i<size1; ++i)
	{
	dyStringAppendC(qSym, qDna[i]);
	dyStringAppendC(tSym, tDna[i]);
	}

    /* If there's second block, output gaps, which is tricky. */
    int qGapSize = q2 - (q1+size1);
    int tGapSize = t2 - (t1+size1);
    if (size2)
	{
	for (i=0; i<qGapSize; ++i)
	    {
	    dyStringAppendC(qSym, qDna[size1+i]);
	    dyStringAppendC(tSym, '-');
	    }
	for (i=0; i<tGapSize; ++i)
	    {
	    dyStringAppendC(qSym, '-');
	    dyStringAppendC(tSym, tDna[size1+i]);
	    }

	/* Output second block. */
	qDna = qSeq->dna + q2;
	tDna = splix->allDna + t2;
	for (i=0; i<size2; ++i)
	    {
	    dyStringAppendC(qSym, qDna[i]);
	    dyStringAppendC(tSym, tDna[i]);
	    }
	}
    int symSize = tSym->stringSize;

    /* Build up a maf component for query */
    struct mafComp *qComp;
    AllocVar(qComp);
    qComp->src = cloneString(qSeq->name);
    qComp->srcSize = qSeq->size;
    qComp->strand = strand;
    qComp->start = q1;
    qComp->size = q2 + size2 - q1;
    qComp->text = dyStringCannibalize(&qSym);

    /* Build up a maf component for target */
    struct mafComp *tComp;
    AllocVar(tComp);
    tComp->src = cloneString(splix->chromNames[chromIx]);
    tComp->srcSize = splix->chromSizes[chromIx];
    tComp->strand = '+';
    tComp->start = t1 - splix->chromOffsets[chromIx];
    tComp->size = t2 + size2 - t1;
    tComp->text = dyStringCannibalize(&tSym);

    /* Build up a maf alignment structure . */
    struct mafAli *maf;
    AllocVar(maf);
    maf->score = 2*qComp->size - tag->divergence;
    maf->textSize = symSize;
    maf->components = tComp;
    tComp->next = qComp;

    /* Save it to file, and free it up. */
    mafWrite(f, maf);
    mafAliFree(&maf);
    }
}


static void splatOutputPsls(struct splatTag *tagList, 
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, struct splix *splix, 
	FILE *f)
/* Output tags to psl file. */
{
struct splatTag *tag;
for (tag = tagList; tag != NULL; tag = tag->next)
    {
    int chromIx = tOffsetToChromIx(splix, tag->t1);
    /* Handle reverse-complementing if need be. */
    char strand = tag->strand;
    struct dnaSeq *qSeq = (strand == '-' ? qSeqR : qSeqF);

    /* Fetch a bunch of vars from tag. */
    int q1 = tag->q1, q2 = tag->q2;
    int t1 = tag->t1, t2 = tag->t2;
    int size1 = tag->size1;
    int size2 = tag->size2;
    int totalSize = size1 + size2;

    /* Point to DNA */
    DNA *qDna = qSeq->dna;
    DNA *tDna = splix->allDna;

    /* Calculate and write the summary non-gappy fields. */
    int misMatches = countDnaDiffs(qDna + q1, tDna + t1, size1) 
	    + countDnaDiffs(qDna + q2, tDna + t2, size2);
    fprintf(f, "%d\t", totalSize - misMatches);	// matches
    fprintf(f, "%d\t", misMatches);			// misMatches
    fprintf(f, "0\t0\t");				// repMatches and nCount

    /* Calculate and write gappy fields. */
    int qGapSize = q2 - (q1+size1);
    int tGapSize = t2 - (t1+size1);
    fprintf(f, "%d\t", (qGapSize == 0 ? 0 : 1) );	// qNumInsert
    fprintf(f, "%d\t", qGapSize);			// qBaseInsert
    fprintf(f, "%d\t", (tGapSize == 0 ? 0 : 1) );	// tNumInsert
    fprintf(f, "%d\t", tGapSize);			// tBaseInsert

    /* A few more straightforward strands. */
    fprintf(f, "%c\t", strand);			// strand
    fprintf(f, "%s\t", qSeq->name);			// qName
    fprintf(f, "%d\t", qSeq->size);			// qSize

    /* Handle qStart/qEnd field, which requires some care on - strand. */
    int qStart = q1;
    int qEnd = q2 + size2;
    if (strand == '-')
	reverseIntRange(&qStart, &qEnd, qSeq->size);
    fprintf(f, "%d\t%d\t", qStart, qEnd);		// qStart and qEnd

    /* Target is always on plus strand, so easier. */
    fprintf(f, "%s\t", splix->chromNames[chromIx]);	// tName
    fprintf(f, "%d\t", (int)splix->chromSizes[chromIx]);	// tSize
    fprintf(f, "%d\t", t1);				// tStart
    fprintf(f, "%d\t", t2 + size2);			// tEnd

    /* Now deal with blocky fields depending if there are one or two. */
    if (size2 == 0)
	{
	/* One block case. */
	fprintf(f, "1\t");				// blockCount
	fprintf(f, "%d,\t", size1);			// blockSizes
	fprintf(f, "%d,\t", q1);			// qStarts
	fprintf(f, "%d,\n", t1);			// tStarts
	}
    else
	{
	/* Two block case. */
	fprintf(f, "2\t");				// blockCount
	fprintf(f, "%d,%d,\t", size1, size2);	// blockSizes
	fprintf(f, "%d,%d,\t", q1, q2);		// qStarts
	fprintf(f, "%d,%d,\n", t1, t2);		// tStart);
	}
    }
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

static void splatOutputSplats(struct splatTag *tagList, 
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, struct splix *splix, FILE *f)
/* Output tags to splat format output file. (Tag-align plus query name)*/
{
int mapCount = slCount(tagList);
struct splatTag *tag;
for (tag = tagList; tag != NULL; tag = tag->next)
    {
    int chromIx = tOffsetToChromIx(splix, tag->t1);
    char strand = tag->strand;
    struct dnaSeq *qSeq = (strand == '-' ? qSeqR : qSeqF);

    /* Write chrom chromStart chromEnd */
    fprintf(f, "%s\t", splix->chromNames[chromIx]);
    int chromOffset = splix->chromOffsets[chromIx];
    fprintf(f, "%d\t%d\t", tag->t1 - chromOffset, tag->t2 + tag->size2 - chromOffset);

    /* Write sequence including gaps (- for deletions, ^ for inserts) */
    int i;
    int size1 = tag->size1;
    int size2 = tag->size2;
    int q1 = tag->q1, q2 = tag->q2;
    int t1 = tag->t1, t2 = tag->t2;
    DNA *qDna = qSeq->dna + q1;
    DNA *tDna = splix->allDna + t1;
    for (i=0; i<tag->size1; ++i)
	 dnaOutUpperMatch(qDna[i], tDna[i], f);
    if (size2 > 0)
	{
	int qGapSize = q2 - (q1+size1);
	int tGapSize = t2 - (t1+size1);
	for (i=0; i<qGapSize; ++i)
	    {
	    fputc('^', f);
	    fputc(tolower(qDna[size1+i]), f);
	    }
	for (i=0; i<tGapSize; ++i)
	    fputc('-', f);
	qDna = qSeq->dna + q2;
	tDna = splix->allDna + t2;
	for (i=0; i<size2; ++i)
	    dnaOutUpperMatch(qDna[i], tDna[i], f);
	}
    fputc('\t', f);

    fprintf(f, "%d\t", 1000/mapCount);
    fprintf(f, "%c\t", strand);
    fprintf(f, "%s\n", qSeq->name);
    }
}

void splatOutTags(struct splatTag *tagList, char *outType,
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, struct splix *splix, FILE *f)
/* Output tag match */
{
if (sameString(outType, "maf"))
    splatOutputMafs(tagList, qSeqF, qSeqR, splix, f);
else if (sameString(outType, "psl"))
    splatOutputPsls(tagList, qSeqF, qSeqR, splix, f);
else if (sameString(outType, "splat"))
    splatOutputSplats(tagList, qSeqF, qSeqR, splix, f);
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
