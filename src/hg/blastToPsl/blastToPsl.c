#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "dlist.h"
#include "fa.h"
#include "nib.h"
#include "blastTab.h"
#include "psl.h"

static char const rcsid[] = "$Id: blastToPsl.c,v 1.1 2003/07/29 18:19:18 braney Exp $";

static int lifted;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "blastToPsl - chain together blast patches into psl\n"
  "usage:\n"
  "   blastToPsl [-lifted] liftSpec queryFa  in.tab out.psl \n"
  "liftSpec is to get target sizes\n"
  "queryFa is to get query sizes\n"
  "in.tab is tblastn tab output\n"
  "out.psl is generated psl file\n"
  "optional -lifted signifies tab file has been lifted.\n"
  );
}
struct sz {
    int size;
    char *name;
};

struct liftSpec
/* How to lift coordinates. */
    {
    struct liftSpec *next;	/* Next in list. */
    int offset;			/* Offset to add. */
    char *oldName;		/* Name in source file. */
    int oldSize;                /* Size of old sequence. */
    char *newName;		/* Name in dest file. */
    int newSize;                   /* Size of new sequence. */
    char strand;                /* Strand of contig relative to chromosome. */
    };

struct liftSpec *readLifts(char *fileName)
/* Read in lift file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordCount;
char *words[16];
struct liftSpec *list = NULL, *el;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    char *offs;
    if (wordCount < 5)
        errAbort("Need at least 5 words line %d of %s", lf->lineIx, lf->fileName);
    offs = words[0];
    if (!isdigit(offs[0]) && !(offs[0] == '-' && isdigit(offs[1])))
	errAbort("Expecting number in first field line %d of %s", lf->lineIx, lf->fileName);
    if (!isdigit(words[4][0]))
	errAbort("Expecting number in fifth field line %d of %s", lf->lineIx, lf->fileName);
    AllocVar(el);
    el->offset = atoi(offs);
    el->oldName = strchr(cloneString(words[1]), '/') + 1;
    el->oldSize = atoi(words[2]);
    el->newName = cloneString(words[3]);
    el->newSize = atoi(words[4]);
    if (wordCount >= 6)
        {
	char c = words[5][0];
	if (c == '+' || c == '-')
	    el->strand = c;
	else
	    errAbort("Expecting + or - field 6, line %d of %s", lf->lineIx, lf->fileName);
	}
    else
        el->strand = '+';
    slAddHead(&list, el);
    }
slReverse(&list);
lineFileClose(&lf);
printf("Got %d lifts in %s\n", slCount(list), fileName);
if (list == NULL)
    errAbort("Empty liftSpec file %s", fileName);
return list;
}

struct hash *hashLift(struct liftSpec *list, boolean revOk)
/* Return a hash of the lift spec. */
{
struct hash *hash = newHash(0);
struct liftSpec *el;
for (el = list; el != NULL; el = el->next)
    {
    if (!revOk && el->strand != '+')
        errAbort("Can't lift from minus strand contigs (like %s) on this file type", el->oldName);
    if (lifted)
	hashAdd(hash, el->newName, el);
    else
	hashAdd(hash, el->oldName, el);
    }
return hash;
}

int pslSortFunc(const void *va, const void *vb)
{       
    const struct psl *a = *((struct psl **)va);
    const struct psl *b = *((struct psl **)vb);
    int dif;
    int tmp;
    dif = strcmp(a->qName, b->qName);

    if (dif == 0)
	{
	return b->repMatch - a->repMatch;
	}
    return dif;
}

int sortFunc(const void *va, const void *vb)
{       
    const struct blastTab *a = *((struct blastTab **)va);
    const struct blastTab *b = *((struct blastTab **)vb);
    int dif;
    int tmp;
    dif = strcmp(a->target, b->target);
    if (dif == 0)
	{
	dif = strcmp(a->query, b->query);
	if (dif == 0)
	    {
		/*
		dif = a->qStart - b->qStart;
		*/
	    if (a->tStart > a->tEnd) 
		{
		if (b->tStart > b->tEnd) 
		    dif = b->tEnd - a->tEnd;
	    	else
		    dif = 1;
		}
	    else
		{
		if (b->tStart <= b->tEnd) 
		    dif = b->tStart - a->tStart;
		else
		    dif = -1;
		}
	    }
	}
    return dif; 
}       

/*
unsigned blockSizes[20000];
unsigned qStarts[20000];
unsigned tStarts[20000];
unsigned revtStarts[20000];
*/
unsigned temp[20000];
struct blastTab Patches[20000];

void reverse(unsigned *array, int count)
{
int ii;

memcpy(temp, array, count * sizeof(unsigned));
for(ii=0; ii < count; ii++)
    array[ii] = temp[count - 1 - ii];
}

void makeChains(struct blastTab *patches, struct hash *tHash,struct hash *qHash,  struct psl **pslList) 
{
struct psl *psl = needMem(sizeof(struct psl));
struct blastTab *el;
boolean negStrand;
int lastStart;
double score = 0.0;
int tSize, qSize;
struct hashEl *hel;
struct sz *sz;

if ((hel = hashLookup(tHash, patches->target)) == NULL)
    {
    printf("couldn't find targe %s in liftSpec\n", patches->target);
    exit(1);
    }
if (lifted)
    tSize = ((struct liftSpec *)hel->val)->newSize;
else
    tSize = ((struct liftSpec *)hel->val)->oldSize;
if ((hel = hashLookup(qHash, patches->query)) == NULL)
    {
    printf("couldn't find query %s in liftSpec\n", patches->query);
    exit(1);
    }
qSize = ((struct sz *)hel->val)->size;
psl->qStart = patches->qStart;
psl->qName = (patches->query);
psl->tName = (patches->target);
psl->strand[0] = '+';
psl->blockSizes = needMem(200*sizeof(int));
psl->qStarts = needMem(200*sizeof(int));
psl->tStarts =needMem(200*sizeof(int));
psl->qSize = qSize;
psl->tSize = tSize;

if (patches->tStart > patches->tEnd)
    {
    psl->tStart = patches->tEnd - 1;
    psl->strand[1] = '-';
    negStrand = TRUE;
    }
else
    {
    psl->tStart = patches->tStart - 1;
    psl->strand[1] = '+';
    negStrand = FALSE;
    }
/*
if (negStrand && (strand == '-'))
    abort();
    */

lastStart = (negStrand)? 10000000 : 0;
for(el = patches ;el; el=el->next)
    {
    if (negStrand)
	{
	    if (el->tStart < el->tEnd)
		abort();
	if (el->qStart >= lastStart)
	    continue;
	}
    else
	{
	    if (el->tStart > el->tEnd)
		abort();
	if (el->qStart <= lastStart)
	    {
	    continue;
	    }
	}
    score += el->bitScore;
    lastStart = el->qStart;
    psl->match += el->aliLength;
    psl->misMatch += el->mismatch;
    psl->qNumInsert += el->gapOpen;

    if (el->qEnd > psl->qEnd)
	psl->qEnd = el->qEnd;
    if (el->qStart < psl->qStart)
	psl->qStart = el->qStart;
    psl->qStarts[psl->blockCount] = el->qStart - 1;
    psl->blockSizes[psl->blockCount] =el->qEnd - (el->qStart - 1);

    /*
    if (negStrand)
	blockSizes[psl->blockCount] =el->qEnd - (el->qStart - 1);
    else
	blockSizes[psl->blockCount] =el->qEnd - (el->qStart - 1);
	*/
    if (negStrand)
	{
//	psl->blockSizes[psl->blockCount] = (el->tStart - el->tEnd)/3;
	psl->tStarts[psl->blockCount] =  tSize - el->tStart;
	psl->tEnd = el->tStart;
	}
    else
	{
	psl->tEnd = el->tEnd;
//	psl->blockSizes[psl->blockCount] = (el->tEnd - el->tStart)/3;
	psl->tStarts[psl->blockCount] = el->tStart;
	}
    psl->tStarts[psl->blockCount]--;
    psl->blockCount++;
    }

    //blockSizes[psl->blockCount - 1] = qStarts[psl->blockCount] - qStarts[psl->blockCount - 1];
if (negStrand)
    {
    reverse(psl->qStarts, psl->blockCount);
    reverse(psl->tStarts, psl->blockCount);
    reverse(psl->blockSizes, psl->blockCount);
    }
//fprintf(pslFile, "%g ", score);
psl->repMatch = (psl->qEnd - psl->qStart)* score * 1000 / qSize ;

if (psl->blockCount > 200)
    abort();
slAddHead(pslList, psl);
// pslTabOut(&psl, pslFile);
}
void chainBlast(char *lift, char *seq, char *in, char *outPsl)
{
struct lineFile *seqlf = lineFileOpen(seq, TRUE);
struct lineFile *lf = lineFileOpen(in, TRUE);
FILE *pslFile = mustOpen(outPsl, "w");
struct blastTab *blastTab, *el, *patch, *patches;
struct psl *psl;
char *oldQuery = "";
boolean negStrand;
int ii;
int count;
struct psl *savePsl;
struct lm *lm, *seqlm;
char strand, oldStrand;
struct liftSpec *list;
char *lastName;
struct hash *hash;
struct hashEl *hel;
int tSize = 0;
int qSize = 0;
struct dnaSeq dnaseq;
struct hash *seqHash = newHash(8);
struct psl *pslList = 0;
struct sz *sz;

list = readLifts(lift);
hash = hashLift(list, TRUE);

seqlm = lmInit(1024*4);
while (faSpeedReadNext(seqlf, &dnaseq.dna, &dnaseq.size, &dnaseq.name))
    {
    lmAllocVar(seqlm, sz);
    sz->size = dnaseq.size;
    sz->name = cloneString(dnaseq.name);
    hashAdd(seqHash, dnaseq.name, sz);
    }
printf("Converting %s\n", in);
blastTab = blastTabLoadAll(in);
slSort(&blastTab, sortFunc);

oldStrand = (blastTab->tStart > blastTab->tEnd) ? '-' : '+';
patches = NULL;
lm = lmInit(1024*4);
pslxWriteHead( pslFile, gftProt, gftDna);
for (el = blastTab; el != NULL; el = el->next)
    {
    strand = (el->tStart > el->tEnd) ? '-' : '+';
    if (patches)
	{
	if ((strand != oldStrand) || (strcmp(el->query, oldQuery) != 0))
	    {
	//    slReverse(&patches);
	    makeChains( patches, hash, seqHash, &pslList );

	    lmCleanup(&lm);
	    lm = lmInit(1024*4);
	    patches = NULL;
	    }
	}

    oldStrand = strand;
    oldQuery = el->query;

    lmAllocVar(lm, patch);
    *patch = *el;
    slAddHead(&patches, patch);
    }
if (patches)
    makeChains( patches, hash, seqHash, &pslList );

slSort(&pslList, pslSortFunc);
lastName = 0;
savePsl = 0;
count = 0;
for(psl = pslList; psl; psl=psl->next)
    {
    if ( (lastName == NULL) || (strcmp(psl->qName, lastName) != 0))
	{
	if (savePsl)
	    {
	    savePsl->nCount = count;
	    pslTabOut(savePsl, pslFile);
	    }
	savePsl = psl;
	count = 0;
	}
    count++;
    lastName = psl->qName;
    }
if (savePsl)
    {
    savePsl->nCount = count;
    pslTabOut(savePsl, pslFile);
    }
lineFileClose(&lf);
lmCleanup(&lm);
}

int main(int argc, char *argv[])
/* Process command line. */
{
static boolean axt;
optionHash(&argc, argv);
if (argc != 5)
    usage();
lifted = optionExists("lifted");
chainBlast(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
