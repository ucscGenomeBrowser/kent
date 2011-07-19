/* gfOut - stuff to manage output for genoFind system -
 * currently supports psl, axt, blast, and wu-blast. 
 *
 * Copyright 2001-2003 Jim Kent.  All rights reserved. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "dnautil.h"
#include "axt.h"
#include "maf.h"
#include "trans3.h"
#include "psl.h"
#include "genoFind.h"

static char const rcsid[] = "$Id: gfOut.c,v 1.16 2006/06/22 16:24:44 kent Exp $";

struct pslxData
/* This is the data structure put in gfOutput.data for psl/pslx output. */
    {
    FILE *f;			/* Output file. */
    boolean saveSeq;		/* Save sequence too? */
    };

struct axtData
/* This is the data structure put in gfOutput.data for axt/blast output. */
    {
    struct axtBundle *bundleList;	/* List of bundles. */
    char *databaseName;		/* Just used for blast. */
    int databaseSeqCount;	/* Just used for blast. */
    double databaseLetters; /* Just used for blast. */
    char *blastType;	/* 'blast' or 'wublast' or 'xml' or 'blast8' or 'blast9' */
    double minIdentity; /* Just used for blast. */
    };

static void savePslx(char *chromName, int chromSize, int chromOffset,
	struct ffAli *ali, struct dnaSeq *tSeq, struct dnaSeq *qSeq, 
	boolean isRc, enum ffStringency stringency, int minMatch, FILE *f,
	struct hash *t3Hash, boolean reportTargetStrand, boolean targetIsRc,
	struct hash *maskHash, int minIdentity, 
	boolean qIsProt, boolean tIsProt, boolean saveSeq)
/* Analyse one alignment and if it looks good enough write it out to file in
 * psl format (or pslX format - if saveSeq is TRUE).  */
{
/* This function was stolen from psLayout and slightly extensively to cope
 * with protein as well as DNA aligments. */
struct ffAli *ff, *nextFf;
struct ffAli *right = ffRightmost(ali);
DNA *needle = qSeq->dna;
DNA *hay = tSeq->dna;
int nStart = ali->nStart - needle;
int nEnd = right->nEnd - needle;
int hStart, hEnd; 
int nInsertBaseCount = 0;
int nInsertCount = 0;
int hInsertBaseCount = 0;
int hInsertCount = 0;
int matchCount = 0;
int mismatchCount = 0;
int repMatch = 0;
int countNs = 0;
DNA *np, *hp, n, h;
int blockSize;
int i;
struct trans3 *t3List = NULL;
Bits *maskBits = NULL;

if (maskHash != NULL)
    maskBits = hashMustFindVal(maskHash, tSeq->name);
if (t3Hash != NULL)
    t3List = hashMustFindVal(t3Hash, tSeq->name);
hStart = trans3GenoPos(ali->hStart, tSeq, t3List, FALSE) + chromOffset;
hEnd = trans3GenoPos(right->hEnd, tSeq, t3List, TRUE) + chromOffset;

/* Count up matches, mismatches, inserts, etc. */
for (ff = ali; ff != NULL; ff = nextFf)
    {
    nextFf = ff->right;
    blockSize = ff->nEnd - ff->nStart;
    np = ff->nStart;
    hp = ff->hStart;
    for (i=0; i<blockSize; ++i)
	{
	n = np[i];
	h = hp[i];
	if (n == 'n' || h == 'n')
	    ++countNs;
	else
	    {
	    if (n == h)
		{
		if (maskBits != NULL)
		    {
		    int seqOff = hp + i - hay;
		    if (bitReadOne(maskBits, seqOff))
		        ++repMatch;
		    else
		        ++matchCount;
		    }
		else
		    ++matchCount;
		}
	    else
		++mismatchCount;
	    }
	}
    if (nextFf != NULL)
	{
	int nhStart = trans3GenoPos(nextFf->hStart, tSeq, t3List, FALSE) + chromOffset;
	int ohEnd = trans3GenoPos(ff->hEnd, tSeq, t3List, TRUE) + chromOffset;
	int hGap = nhStart - ohEnd;
	int nGap = nextFf->nStart - ff->nEnd;

	if (nGap != 0)
	    {
	    ++nInsertCount;
	    nInsertBaseCount += nGap;
	    }
	if (hGap != 0)
	    {
	    ++hInsertCount;
	    hInsertBaseCount += hGap;
	    }
	}
    }


/* See if it looks good enough to output, and output. */
/* if (score >= minMatch) Moved to higher level */
    {
    int gaps = nInsertCount + (stringency == ffCdna ? 0: hInsertCount);
    if ((matchCount + repMatch + mismatchCount) > 0)
	{
	int id = roundingScale(1000, matchCount + repMatch - 2*gaps, matchCount + repMatch + mismatchCount);
	if (id >= minIdentity)
	    {
	    if (isRc)
		{
		int temp;
		int oSize = qSeq->size;
		temp = nStart;
		nStart = oSize - nEnd;
		nEnd = oSize - temp;
		}
	    if (targetIsRc)
		{
		int temp;
		temp = hStart;
		hStart = chromSize - hEnd;
		hEnd = chromSize - temp;
		}
	    fprintf(f, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%c",
		matchCount, mismatchCount, repMatch, countNs, nInsertCount, nInsertBaseCount, hInsertCount, hInsertBaseCount,
		(isRc ? '-' : '+'));
	    if (reportTargetStrand)
		fprintf(f, "%c", (targetIsRc ? '-' : '+') );
	    fprintf(f, "\t%s\t%d\t%d\t%d\t"
			 "%s\t%d\t%d\t%d\t%d\t",
		qSeq->name, qSeq->size, nStart, nEnd,
		chromName, chromSize, hStart, hEnd,
		ffAliCount(ali));
	    for (ff = ali; ff != NULL; ff = ff->right)
		fprintf(f, "%ld,", (long)(ff->nEnd - ff->nStart));
	    fprintf(f, "\t");
	    for (ff = ali; ff != NULL; ff = ff->right)
		fprintf(f, "%ld,", (long)(ff->nStart - needle));
	    fprintf(f, "\t");
	    for (ff = ali; ff != NULL; ff = ff->right)
		fprintf(f, "%d,", trans3GenoPos(ff->hStart, tSeq, t3List, FALSE) + chromOffset);
	    if (saveSeq)
		{
		fputc('\t', f);
		for (ff = ali; ff != NULL; ff = ff->right)
		    {
		    mustWrite(f, ff->nStart, ff->nEnd - ff->nStart);
		    fputc(',', f);
		    }
		fputc('\t', f);
		for (ff = ali; ff != NULL; ff = ff->right)
		    {
		    mustWrite(f, ff->hStart, ff->hEnd - ff->hStart);
		    fputc(',', f);
		    }
		}
	    fprintf(f, "\n");
	    if (ferror(f))
		{
		perror("");
		errAbort("Write error to .psl");
		}
	    }
	}
    }
}

static void pslOut(char *chromName, int chromSize, int chromOffset, struct ffAli *ali, 
	struct dnaSeq *tSeq, struct hash *t3Hash, struct dnaSeq *qSeq, 
	boolean qIsRc, boolean tIsRc, 
	enum ffStringency stringency, int minMatch, struct gfOutput *out)
/* Save psl for more complex alignments. */
{
struct pslxData *outForm = out->data;

savePslx(chromName, chromSize, chromOffset, ali, tSeq, qSeq,
    qIsRc, stringency, minMatch, outForm->f, t3Hash, 
    out->reportTargetStrand, tIsRc,
    out->maskHash, out->minGood, 
    out->qIsProt, out->tIsProt, outForm->saveSeq);
}

static struct ffAli *ffNextBreak(struct ffAli *ff, int maxInsert, 
	bioSeq *tSeq, struct trans3 *t3List)
/* Return ffAli after first gap in either sequence longer than maxInsert,
 * or after first gap in both sequences.  Return may legitimately
 * be NULL. */
{
struct ffAli *rt = ff->right;
int hGap, nGap;
int nhStart, ohEnd;
for (;;)
    {
    if (rt == NULL)
        break;
    nhStart = trans3GenoPos(rt->hStart, tSeq, t3List, FALSE);
    ohEnd = trans3GenoPos(ff->hEnd, tSeq, t3List, TRUE);
    hGap = nhStart - ohEnd;
    nGap = rt->nStart - ff->nEnd;
    if (hGap != 0 && nGap != 0)
        break;
    if (hGap < 0 || nGap < 0)
        break;
    if (hGap > maxInsert || nGap > maxInsert)
        break;
    ff = rt;
    rt = ff->right;
    }
return rt;
}


static void saveAxtBundle(char *chromName, int chromSize, int chromOffset,
	struct ffAli *ali, 
	struct dnaSeq *tSeq, struct hash *t3Hash, struct dnaSeq *qSeq, 
	boolean qIsRc, boolean tIsRc, 
	enum ffStringency stringency, int minMatch, struct gfOutput *out)
/* Save alignment to axtBundle. */
{
struct axtData *ad = out->data;
struct ffAli *sAli, *eAli, *ff, *rt, *eFf = NULL;
struct axt *axt;
struct dyString *q = newDyString(1024), *t = newDyString(1024);
struct axtBundle *gab;
struct trans3 *t3List = NULL;

if (t3Hash != NULL)
    t3List = hashMustFindVal(t3Hash, tSeq->name);
AllocVar(gab);
gab->tSize = chromSize;
gab->qSize = qSeq->size;
for (sAli = ali; sAli != NULL; sAli = eAli)
    {
    eAli = ffNextBreak(sAli, 8, tSeq, t3List);
    dyStringClear(q);
    dyStringClear(t);
    for (ff = sAli; ff != eAli; ff = ff->right)
        {
	dyStringAppendN(q, ff->nStart, ff->nEnd - ff->nStart);
	dyStringAppendN(t, ff->hStart, ff->hEnd - ff->hStart);
	rt = ff->right;
	if (rt != eAli)
	    {
	    int nGap = rt->nStart - ff->nEnd;
	    int nhStart = trans3GenoPos(rt->hStart, tSeq, t3List, FALSE) 
	    	+ chromOffset;
	    int ohEnd = trans3GenoPos(ff->hEnd, tSeq, t3List, TRUE) 
	    	+ chromOffset;
	    int hGap = nhStart - ohEnd;
	    int gap = max(nGap, hGap);
	    if (nGap < 0 || hGap < 0)
		{
	        errAbort("Negative gap size in %s vs %s", tSeq->name, qSeq->name);
		}
	    if (nGap == gap)
	        {
		dyStringAppendN(q, ff->nEnd, gap);
		dyStringAppendMultiC(t, '-', gap);
		}
	    else
	        {
		dyStringAppendN(t, ff->hEnd, gap);
		dyStringAppendMultiC(q, '-', gap);
		}
	    }
	eFf = ff;	/* Keep track of last block in bunch */
	}
    assert(t->stringSize == q->stringSize);
    AllocVar(axt);
    axt->qName = cloneString(qSeq->name);
    axt->qStart = sAli->nStart - qSeq->dna;
    axt->qEnd = eFf->nEnd - qSeq->dna;
    axt->qStrand = (qIsRc ? '-' : '+');
    axt->tName = cloneString(chromName);
    axt->tStart = trans3GenoPos(sAli->hStart, tSeq, t3List, FALSE) + chromOffset;
    axt->tEnd = trans3GenoPos(eFf->hEnd, tSeq, t3List, TRUE) + chromOffset;
    axt->tStrand = (tIsRc ? '-' : '+');
    axt->symCount = t->stringSize;
    axt->qSym = cloneString(q->string);
    axt->tSym = cloneString(t->string);
    axt->frame = trans3Frame(sAli->hStart, t3List);
    if (out->qIsProt)
	axt->score = axtScoreProteinDefault(axt);
    else 
	axt->score = axtScoreDnaDefault(axt);
    slAddHead(&gab->axtList, axt);
    }
slReverse(&gab->axtList);
dyStringFree(&q);
dyStringFree(&t);
slAddHead(&ad->bundleList, gab);
}

static void axtQueryOut(struct gfOutput *out, FILE *f)
/* Do axt oriented output - at end of processing query. */
{
struct axtData *aod = out->data;
struct axtBundle *gab;
for (gab = aod->bundleList; gab != NULL; gab = gab->next)
    {
    struct axt *axt;
    for (axt = gab->axtList; axt != NULL; axt = axt->next)
	axtWrite(axt, f);
    }
axtBundleFreeList(&aod->bundleList);
}

static void mafHead(struct gfOutput *out, FILE *f)
/* Write maf header. */
{
mafWriteStart(f, "blastz");
}

static void mafQueryOut(struct gfOutput *out, FILE *f)
/* Do axt oriented output - at end of processing query. */
{
struct axtData *aod = out->data;
struct axtBundle *gab;
for (gab = aod->bundleList; gab != NULL; gab = gab->next)
    {
    struct axt *axt;
    for (axt = gab->axtList; axt != NULL; axt = axt->next)
	{
	struct mafAli temp;
	mafFromAxtTemp(axt, gab->tSize, gab->qSize, &temp);
	mafWrite(f, &temp);
	}
    }
axtBundleFreeList(&aod->bundleList);
}

static double axtIdRatio(struct axt *axt)
/* Return matches/total. */
{
int matchCount = 0;
int i;
for (i=0; i<axt->symCount; ++i)
    {
    if (axt->qSym[i] == axt->tSym[i])
        ++matchCount;
    }
return (double)matchCount/(double)axt->symCount;
}

static void sim4QueryOut(struct gfOutput *out, FILE *f)
/* Do sim4-like output - at end of processing query. */
{
struct axtData *aod = out->data;
struct axtBundle *gab;

for (gab = aod->bundleList; gab != NULL; gab = gab->next)
    {
    struct axt *axt = gab->axtList;
    fprintf(f, "\n");
    fprintf(f, "seq1 = %s, %d bp\n", axt->qName, gab->qSize);
    fprintf(f, "seq2 = %s, %d bp\n", axt->tName, gab->tSize);
    fprintf(f, "\n");
    if (axt->qStrand == '-')
	fprintf(f, "(complement)\n");
    for (axt = gab->axtList; axt != NULL; axt = axt->next)
	{
	fprintf(f, "%d-%d  ", axt->qStart+1, axt->qEnd);
	fprintf(f, "(%d-%d)   ", axt->tStart+1, axt->tEnd);
	fprintf(f, "%1.0f%% ", 100.0 * axtIdRatio(axt));
	if (axt->qStrand == '-')
	     fprintf(f, "<-\n");
	else
	     fprintf(f, "->\n");
	}
    }
axtBundleFreeList(&aod->bundleList);
}

static void blastQueryOut(struct gfOutput *out, FILE *f)
/* Output blast on query. */
{
struct axtData *aod = out->data;
axtBlastOut(aod->bundleList, out->queryIx, out->qIsProt, f,
	aod->databaseName, aod->databaseSeqCount, aod->databaseLetters,
	aod->blastType, "blat", aod->minIdentity);
axtBundleFreeList(&aod->bundleList);
}

static struct gfOutput *gfOutputInit(int goodPpt, boolean qIsProt, boolean tIsProt)
/* Allocate and initialize gfOutput.   You'll need to fill in 
 * gfOutput.out at a minimum, and likely gfOutput.data before
 * using though. */
{
struct gfOutput *out;
AllocVar(out);
out->minGood = goodPpt;
out->qIsProt = qIsProt;
out->tIsProt = tIsProt;
return out;
}

static void pslHead(struct gfOutput *out, FILE *f)
/* Write out psl head */
{
pslWriteHead(f);
}

struct gfOutput *gfOutputPsl(int goodPpt, 
	boolean qIsProt, boolean tIsProt, FILE *f, 
	boolean saveSeq, boolean noHead)
/* Set up psl/pslx output */
{
struct gfOutput *out = gfOutputInit(goodPpt, qIsProt, tIsProt);
struct pslxData *pslData;

AllocVar(pslData);
pslData->saveSeq = saveSeq;
pslData->f = f;
out->out = pslOut;
out->data = pslData;
if (!noHead)
    out->fileHead = pslHead;
return out;
}

struct gfOutput *gfOutputAxtMem(int goodPpt, boolean qIsProt, 
	boolean tIsProt)
/* Setup output for in memory axt output. */
{
struct gfOutput *out = gfOutputInit(goodPpt, qIsProt, tIsProt);
struct axtData *ad;
AllocVar(ad);
out->out = saveAxtBundle;
out->data = ad;
return out;
}

struct gfOutput *gfOutputAxt(int goodPpt, boolean qIsProt, 
	boolean tIsProt, FILE *f)
/* Setup output for axt format. */
{
struct gfOutput *out = gfOutputAxtMem(goodPpt, qIsProt, tIsProt);
out->queryOut = axtQueryOut;
return out;
}

struct gfOutput *gfOutputMaf(int goodPpt, boolean qIsProt, 
	boolean tIsProt, FILE *f)
/* Setup output for maf format. */
{
struct gfOutput *out = gfOutputAxtMem(goodPpt, qIsProt, tIsProt);
out->queryOut = mafQueryOut;
out->fileHead = mafHead;
return out;
}

struct gfOutput *gfOutputSim4(int goodPpt, boolean qIsProt, boolean tIsProt, 
	char *databaseName)
/* Set up to output in sim4 format. */
{
struct gfOutput *out = gfOutputAxtMem(goodPpt, qIsProt, tIsProt);
struct axtData *ad = out->data;
if (qIsProt || tIsProt)
    errAbort("sim4 output is not available for protein query sequences.");
ad->databaseName = databaseName;
out->queryOut = sim4QueryOut;
return out;
}

struct gfOutput *gfOutputBlast(int goodPpt, 
	boolean qIsProt, boolean tIsProt, 
	char *databaseName, int databaseSeqCount, double databaseLetters,
	char *blastType, double minIdentity, FILE *f)
/* Setup output for blast format. */
{
struct gfOutput *out = gfOutputAxtMem(goodPpt, qIsProt, tIsProt);
struct axtData *ad = out->data;
ad->databaseName = databaseName;
ad->databaseSeqCount = databaseSeqCount;
ad->databaseLetters = databaseLetters;
ad->blastType = blastType;
ad->minIdentity = minIdentity;
out->queryOut = blastQueryOut;
return out;
}

struct gfOutput *gfOutputAny(char *format, 
	int goodPpt, boolean qIsProt, boolean tIsProt, 
	boolean noHead, char *databaseName,
	int databaseSeqCount, double databaseLetters,
	double minIdentity,
	FILE *f)
/* Initialize output in a variety of formats in file or memory. 
 * Parameters:
 *    format - either 'psl', 'pslx', 'sim4', 'blast', 'wublast', 'axt', 'xml'
 *    goodPpt - minimum identity of alignments to output in parts per thousand
 *    qIsProt - true if query side is a protein.
 *    tIsProt - true if target (database) side is a protein.
 *    noHead - if true suppress header in psl/pslx output.
 *    databaseName - name of database.  Only used for blast output
 *    databaseSeq - number of sequences in database - only for blast
 *    databaseLetters - number of bases/aas in database - only blast
 *    minIdentity - minimum identity - only blast
 *    FILE *f - file.  
 */
{
struct gfOutput *out = NULL;
static char *blastTypes[] = {"blast", "wublast", "blast8", "blast9", "xml"};

if (format == NULL)
    format = "psl";
if (sameWord(format, "psl"))
    out = gfOutputPsl(goodPpt, qIsProt, tIsProt, f, FALSE, noHead);
else if (sameWord(format, "pslx"))
    out = gfOutputPsl(goodPpt, qIsProt, tIsProt, f, TRUE, noHead);
else if (sameWord(format, "sim4"))
    out = gfOutputSim4(goodPpt, qIsProt, tIsProt, databaseName);
else if (stringArrayIx(format, blastTypes, ArraySize(blastTypes)) >= 0)
    out = gfOutputBlast(goodPpt, qIsProt, tIsProt, 
	    databaseName, databaseSeqCount, databaseLetters, format, 
	    minIdentity, f);
else if (sameWord(format, "axt"))
    out = gfOutputAxt(goodPpt, qIsProt, tIsProt, f);
else if (sameWord(format, "maf"))
    out = gfOutputMaf(goodPpt, qIsProt, tIsProt, f);
else
    errAbort("Unrecognized output format '%s'", format);
return out;
}

void gfOutputQuery(struct gfOutput *out, FILE *f)
/* Finish writing out results for a query to file. */
{
++out->queryIx;
if (out->queryOut != NULL)
    out->queryOut(out, f);
}

void gfOutputHead(struct gfOutput *out, FILE *f)
/* Write out header if any. */
{
if (out->fileHead != NULL)
    out->fileHead(out, f);
}

void gfOutputFree(struct gfOutput **pOut)
/* Free up output */
{
struct gfOutput *out = *pOut;
if (out != NULL)
    {
    freeMem(out->data);
    freez(pOut);
    }
}
