/* output - output alignments to files in various formats. */

#include "common.h"
#include "dystring.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "maf.h"
#include "chain.h"
#include "splix.h"
#include "splat.h"

static void splatOutputMaf(struct splatAlign *ali, 
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, FILE *f)
/* Output alignment to maf file. */
{
/* TODO: This is sufficiently refactored now that it can become a thin
 * wrapper around chainToMaf, mafWrite, mafFree. */
struct cBlock *bStart = ali->chain->blockList;
struct chain *chain = ali->chain;

/* Create symbols - DNA alignment with dashes for inserts - a long process. */
struct dyString *qSym = dyStringNew(0), *tSym = dyStringNew(0);

/* Loop through blocks... */
struct cBlock *b, *bNext;
for (b = bStart; b != NULL; b = bNext)
    {
    /* Output letters in block. */
    int bSize = b->tEnd - b->tStart;
    DNA *qDna = ali->qDna + b->qStart;
    DNA *tDna = ali->tDna + b->tStart;
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
qComp->src = cloneString(chain->qName);
qComp->srcSize = chain->qSize;
qComp->strand = chain->qStrand;
qComp->start = bStart->qStart;
qComp->size = chain->qEnd - chain->qStart;
qComp->text = dyStringCannibalize(&qSym);

/* Build up a maf component for target */
struct mafComp *tComp;
AllocVar(tComp);
tComp->src = cloneString(chain->tName);
tComp->srcSize = chain->tSize;
tComp->strand = '+';
tComp->start = chain->tStart;
tComp->size = chain->tEnd - chain->tStart;
tComp->text = dyStringCannibalize(&tSym);

/* Build up a maf alignment structure . */
struct mafAli *maf;
AllocVar(maf);
maf->score = chain->score;
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
    splatOutputMaf(ali, qSeqF, qSeqR, f);
}


static void splatOutputPsl(struct splatAlign *ali, 
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, FILE *f)
/* Output alignment to psl file. */
{
/* TODO - this is sufficiently refactored now that it can be a very thin
 * wrapper around chainToPsl/pslWrite/pslFree. */
struct chain *chain = ali->chain;
struct cBlock *bStart = ali->chain->blockList;

/* Point to DNA */
DNA *qDna = ali->qDna;
DNA *tDna = ali->tDna;

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
fprintf(f, "%c\t", chain->qStrand);
fprintf(f, "%s\t", chain->qName);
fprintf(f, "%d\t", chain->qSize);

/* Handle qStart/qEnd field, which requires some care on - strand. */
int qStart = chain->qStart;
int qEnd = chain->qEnd;
if (chain->qStrand == '-')
    reverseIntRange(&qStart, &qEnd, chain->qSize);
fprintf(f, "%d\t%d\t", qStart, qEnd);

/* Target is always on plus strand, so easier. */
fprintf(f, "%s\t", chain->tName);	// tName
fprintf(f, "%d\t", chain->tSize);	// tSize
fprintf(f, "%d\t", chain->tStart);	// tStart
fprintf(f, "%d\t", chain->tEnd);	// tEnd

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
    splatOutputPsl(ali, qSeqF, qSeqR, f);
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
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, FILE *f)
/* Output alignment  to splat format output file. (Tag-align plus query name) */
{
struct chain *chain = ali->chain;
struct cBlock *bStart = ali->chain->blockList;

/* Write chrom chromStart chromEnd */
fprintf(f, "%s\t", chain->tName);
fprintf(f, "%d\t%d\t", chain->tStart, chain->tEnd);

/* Write sequence including gaps (- for deletions, ^ for inserts) */
DNA *qDna = ali->qDna;
DNA *tDna = ali->tDna;
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
fprintf(f, "%c\t", chain->qStrand);
fprintf(f, "%s\n", chain->qName);
}

static void splatOutputSplats(struct splatAlign *aliList, 
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, struct splix *splix, FILE *f)
/* Output list of alignments to splat format output file. (Tag-align plus query name)*/
{
int mapCount = slCount(aliList);
struct splatAlign *ali;
for (ali = aliList; ali != NULL; ali = ali->next)
    splatOutputSplat(ali, mapCount, qSeqF, qSeqR, f);
}

void splatOutList(struct splatAlign *aliList, char *outType,
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, struct splix *splix, FILE *f)
/* Output list of alignments to file in format defined by outType. */
{
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
