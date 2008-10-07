
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"
#include "maf.h"
#include "obscure.h"
#include "genePred.h"
#include "mafGene.h"

static char const rcsid[] = "$Id: mafGene.c,v 1.11 2008/10/07 19:19:25 braney Exp $";

struct exonInfo
{
    struct exonInfo *next;
    struct mafAli *ali;
    int exonStart;
    int exonSize;
    int chromStart;
    int chromEnd;
    int frame;
    char strand;
    char *name;
};

struct speciesInfo
{
    struct speciesInfo *next;
    char *name;
    int size;
    char *nucSequence;
    char *aaSequence;
    int aaSize;
    struct slName *posStrings;
    struct slName *curPosString;
    char *posString;
    char *chrom;
    int start, end;
    char strand;
    char frameStrand;
};

/* rather than allocate space for these every time,
 * just re-use a global resource.  It's not 
 * re-entrant safe, but....
 */
#define MAX_EXON_SIZE 100 * 1024
static char exonBuffer[MAX_EXON_SIZE];

static char bigBuffer[100 * 1024];

#define MAX_COMPS  	5000 
char *compText[MAX_COMPS]; 
char *siText[MAX_COMPS]; 

/* is the sequence all dashes ? */
static boolean allDashes(char *seq)
{
while (*seq)
    if (*seq++ != '-')
	return FALSE;

return TRUE;
}

/* translate a nuc sequence into amino acids. If there
 * are any dashes in any of the three nuc positions
 * make the AA a dash.
 */
static aaSeq *doTranslate(struct dnaSeq *inSeq, unsigned offset, 
    unsigned inSize, boolean stop)
{
aaSeq *seq;
DNA *dna = inSeq->dna;
AA *pep, aa;
int i, lastCodon;
int actualSize = 0;

assert(offset <= inSeq->size);
if ((inSize == 0) || (inSize > (inSeq->size - offset)))
    inSize = inSeq->size - offset;
lastCodon = offset + inSize - 3;

AllocVar(seq);
seq->dna = pep = needLargeMem(inSize/3+1);
for (i=offset; i <= lastCodon; i += 3)
    {
    aa = lookupCodon(dna+i);
    if (aa == 'X')
	{
	if ((dna[i] == '-') ||
	    (dna[i+1] == '-') ||
	    (dna[i+2] == '-'))
	    aa = '-';
	}
    if (aa == 0)
	{
        if (stop)
	    break;
	else
	    aa = 'Z';
	}
    *pep++ = aa;
    ++actualSize;
    }
*pep = 0;
assert(actualSize <= inSize/3+1);
seq->size = actualSize;
return seq;
}

static struct mafAli *getRefAli(char *database, char *chrom, int start, int end)
{
struct mafAli *ali;
struct mafComp *comp;
char buffer[1024];

AllocVar(ali);
AllocVar(comp);
ali->components = comp;
ali->textSize = end - start;

safef(buffer, sizeof buffer, "%s.%s", database, chrom);
comp->src = cloneString(buffer);
comp->start = start;
comp->strand = '+';
comp->size = end - start;
struct dnaSeq *seq = hChromSeqMixed(database, chrom, start , end);
comp->text = cloneString(seq->dna);
freeDnaSeq(&seq);

return ali;
}

/* make sure we have the whole range even if
 * there isn't a maf loaded in this region
 */
static struct mafAli *padOutAli(struct mafAli *list, char *database, 
    char *chrom, int start, int end)
{
if (list == NULL)
    {
    struct mafAli *ali = getRefAli(database, chrom, start, end);
    return ali;
    }

int aliStart = list->components->start;
if (start != aliStart)
    {
    struct mafAli *ali = getRefAli(database, chrom, start, aliStart);
    slAddHead(&list, ali);
    }

struct mafAli *last = list;
for(; last->next; last = last->next)
    ;

int aliEnd = last->components->start + last->components->size;
if (end != aliEnd)
    {
    struct mafAli *ali = getRefAli(database, chrom, aliEnd, end);
    slAddTail(&list, ali);
    }

return list;
}

static struct mafAli *getAliForRange(char *database, char *mafTable, 
    char *chrom, int start, int end)
{
struct sqlConnection *conn = hAllocConn(database);
struct mafAli *aliAll = mafLoadInRegion(conn, mafTable,
	chrom, start, end);
struct mafAli *ali;
struct mafAli *list = NULL;
struct mafAli *nextAli;

hFreeConn(&conn);

for(ali = aliAll; ali; ali = nextAli)
    {
    nextAli = ali->next;
    ali->next = NULL;

    char *masterSrc = ali->components->src;
    struct mafAli *subAli = NULL;

    if (mafNeedSubset(ali, masterSrc, start, end))
	{
	subAli = mafSubset( ali, masterSrc, start, end);
	if (subAli == NULL)
	    continue;
	}

    if (subAli)
	{
	slAddHead(&list, subAli);
	mafAliFree(&ali);
	}
    else
	slAddHead(&list, ali);
    }
slReverse(&list);

list = padOutAli(list, database, chrom, start, end);

return list;
}


/* allocate space for the nuc and aa sequence for each species */
static struct speciesInfo *getSpeciesInfo(struct exonInfo *giList, 
    struct slName *speciesNames, struct hash *siHash)
{
struct exonInfo *gi;
int size = 0;
struct speciesInfo *siList = NULL;

for(gi = giList ; gi ; gi = gi->next)
    size += gi->exonSize;

struct slName *name = speciesNames;

for(; name ; name = name->next)
    {
    struct speciesInfo *si;

    AllocVar(si);
    si->frameStrand = giList->strand;
    si->name = name->name;
    verbose(2, "size %d\n", size);
    si->size = size;
    si->nucSequence = needMem(size + 1);
    memset(si->nucSequence, '-', size);
    si->aaSequence = needMem(size/3 + 1);
    hashAdd(siHash, si->name, si);
    slAddHead(&siList, si);
    }
slReverse(&siList);

return siList;
}


static void outSpeciesExons(FILE *f, char *dbName, struct speciesInfo *si, 
    struct exonInfo *giList, boolean doBlank, boolean doTable, int numCols)
{
int exonNum = 1;
struct dnaSeq thisSeq;
aaSeq *outSeq;
int exonCount = 0;
struct exonInfo *gi = giList;

for(; gi; gi = gi->next)
    {
    if (gi->exonSize > 1)
	exonCount++;
    }

for(gi = giList; gi; gi = gi->next, exonNum++)
    {
    struct speciesInfo *siTemp = si;
    
    if (gi->exonSize == 1)
	continue;

    for(; siTemp ; siTemp = siTemp->next)
	{
	char *ptr = exonBuffer;

	switch(gi->frame)
	    {
	    case 0: /* just copy the sequence over */
		memcpy(ptr, 
		    &siTemp->nucSequence[gi->exonStart], gi->exonSize);
		ptr += gi->exonSize;
		break;
	    case 1: /* we need to grab one nuc from the end 
	             * of the previous exon */
		*ptr++ = siTemp->nucSequence[gi->exonStart - 1];
		memcpy(ptr, 
		    &siTemp->nucSequence[gi->exonStart], gi->exonSize);
		ptr += gi->exonSize;
		break;

	    case 2: /* we need to delete the first nuc from this exon
	             * because we included it on the last exon */
		memcpy(ptr, 
		    &siTemp->nucSequence[gi->exonStart+1], gi->exonSize - 1);
		ptr += gi->exonSize - 1;
		break;
	    }

	int lastFrame = (gi->frame + gi->exonSize) % 3;
	if (lastFrame == 1) /* delete the last nucleotide */
	    --ptr;
	else if (lastFrame == 2) /* add one more nucleotide from
	                          * the next exon */
	    *ptr++ = siTemp->nucSequence[gi->exonStart + gi->exonSize];
	*ptr++ = 0;   /* null terminate */

	thisSeq.dna = exonBuffer;
	thisSeq.size = ptr - exonBuffer;
	outSeq =  doTranslate(&thisSeq, 0,  0, FALSE);
	char buffer[10 * 1024];

	safef(buffer, sizeof buffer,  "%s_%s_%d_%d %d %d %d %s",
	    gi->name, 
	    siTemp->name, exonNum, exonCount, 
	    outSeq->size,
	    gi->frame, lastFrame,
	    siTemp->curPosString->name);

	if (doBlank || !allDashes(outSeq->dna))
	    {
	    if (doTable)
		{
		if (numCols == -1)
		    fprintf(f, "%s ", buffer);
		else
		    {
		    if (strlen(buffer) > numCols)
			buffer[numCols] = 0;
		    fprintf(f, "%-*s ", numCols, buffer);
		    }
		}
	    else
		fprintf(f, ">%s\n", buffer);

	    fprintf(f, "%s\n", outSeq->dna);
	    }
	siTemp->curPosString = siTemp->curPosString->next;
	}
    fprintf(f, "\n");
    }
fprintf(f, "\n");
}

static void outSpeciesExonsNoTrans(FILE *f, char *dbName, 
    struct speciesInfo *si, struct exonInfo *giList, boolean doBlank,
    boolean doTable, int numCols)
{
int exonNum = 1;
int exonCount = 0;
struct exonInfo *gi;

for(gi = giList; gi; gi = gi->next)
    exonCount++;

for(gi = giList; gi; gi = gi->next, exonNum++)
    {
    struct speciesInfo *siTemp = si;

    int lastFrameNum = (gi->frame + gi->exonSize) % 3;

    for(; siTemp ; siTemp = siTemp->next)
	{
	int start = gi->exonStart;
	int end = start + gi->exonSize;
	char *ptr = &siTemp->nucSequence[gi->exonStart];

	for (; start < end; start++, ptr++)
	    if (*ptr != '-')
		break;

	if (!doBlank && (start == end))
	    {
	    siTemp->curPosString = siTemp->curPosString->next;
	    continue;
	    }

	start = gi->exonStart;
	ptr = &siTemp->nucSequence[gi->exonStart];
	char buffer[10 * 1024];

	safef(buffer, sizeof buffer, "%s_%s_%d_%d %d %d %d %s",
	    gi->name, 
	    siTemp->name, exonNum, exonCount, 
	    gi->exonSize,
	    gi->frame, lastFrameNum,
	    siTemp->curPosString->name);

	if (doTable)
	    {
	    if (numCols == -1)
		fprintf(f, "%s ", buffer);
	    else
		{
		if (strlen(buffer) > numCols)
		    buffer[numCols] = 0;
		fprintf(f, "%-*s ", numCols, buffer);
		}
	    }
	else
	    fprintf(f, ">%s\n", buffer);

	siTemp->curPosString = siTemp->curPosString->next;

	for (; start < end; start++)
	    fprintf(f, "%c", *ptr++);
	fprintf(f, "\n");
	}
    fprintf(f, "\n");
    }
fprintf(f, "\n");
}

/* translate nuc sequence into an sequence of amino acids */
static void translateProtein(struct speciesInfo *si)
{
struct dnaSeq thisSeq;
aaSeq *outSeq;

thisSeq.dna = si->nucSequence;
thisSeq.size = si->size;
outSeq =  doTranslate(&thisSeq, 0,  0, FALSE);
si->aaSequence  = outSeq->dna;
si->aaSize = outSeq->size;
}

static char *allPos(struct speciesInfo *si)
{
char *ptr = bigBuffer;
struct slName *names = si->posStrings;
int size = sizeof bigBuffer;

for(; names ; names = names->next)
    {
    int sz = safef(ptr, size, "%s", names->name);
    ptr += sz;
    size -= sz;
    if (names->next)
	{
	safef(ptr, size, ";");
	ptr++;
	size--;
	}
    }

return bigBuffer;
}

/* output a particular species sequence to the file stream */
static void writeOutSpecies(FILE *f, char *dbName, struct speciesInfo *si, 
    struct exonInfo *giList, unsigned options, int numCols)
{
boolean inExons = options & MAFGENE_EXONS;
boolean noTrans = options & MAFGENE_NOTRANS;
boolean doBlank = options & MAFGENE_OUTBLANK;
boolean doTable = options & MAFGENE_OUTTABLE;

if (inExons)
    {
    if (noTrans)
	outSpeciesExonsNoTrans(f, dbName, si, giList, doBlank, 
	    doTable, numCols);
    else
	outSpeciesExons(f, dbName, si, giList, doBlank, doTable, numCols);
    return;
    }

struct exonInfo *lastGi;

for(lastGi = giList; lastGi->next ; lastGi = lastGi->next)
    ;

if (noTrans)
    {
    for(; si ; si = si->next)
	{
	if (doBlank || !allDashes(si->nucSequence))
	    {
	    char buffer[10 * 1024];

	    safef(buffer, sizeof buffer, "%s_%s %d %s\n",
		giList->name, si->name, si->size, allPos(si));

	    if (doTable)
		{
		if (numCols == -1)
		    fprintf(f, "%s ", buffer);
		else
		    {
		    if (strlen(buffer) > numCols)
			buffer[numCols] = 0;
		    fprintf(f, "%-*s ", numCols, buffer);
		    }
		}
	    else
		fprintf(f, ">%s\n", buffer);

	    fprintf(f, "%s\n", si->nucSequence);
	    }
	}
    fprintf(f, "\n\n");
    }
else
    {
    for(; si ; si = si->next)
	{
	translateProtein(si);

	char buffer[10 * 1024];

	safef(buffer, sizeof buffer, "%s_%s %d %s",
	    giList->name, si->name, si->aaSize, allPos(si));

	if (doBlank || !allDashes(si->aaSequence))
	    {
	    if (doTable)
		{
		if (numCols == -1)
		    fprintf(f, "%s ", buffer);
		else
		    {
		    if (strlen(buffer) > numCols)
			buffer[numCols] = 0;
		    fprintf(f, "%-*s ", numCols, buffer);
		    }
		}
	    else
		fprintf(f, ">%s\n", buffer);

	    fprintf(f, "%s\n", si->aaSequence);
	    }
	}
    fprintf(f, "\n\n");
    }
}

static void flushPosString(struct speciesInfo *si)
{
if (si->chrom != NULL)
    {
    char buffer[10*1024];
    char strand = '+';

    if (si->strand != si->frameStrand)
	strand = '-';

    if (si->posString == NULL)
	{
	safef(buffer, sizeof buffer, "%s:%d-%d%c", si->chrom,
	    si->start+1, si->end, strand);
	}
    else
	{
	safef(buffer, sizeof buffer, "%s;%s:%d-%d%c", si->posString,
	    si->chrom, si->start+1, si->end, strand);
	freez(&si->posString);
	}
    si->posString = cloneString(buffer);
    }

si->chrom = NULL;
}

static void pushPosString(struct speciesInfo *si)
{
flushPosString(si);

struct slName *newName = newSlName(si->posString);
slAddTail(&si->posStrings, newName);

freez(&si->posString);
}

static void updatePosString(struct speciesInfo *si, char *chrom, 
		char strand, int start, int end)
{
if (start == end)
    return;

if ((si->chrom == NULL) || 
    !sameString(si->chrom, chrom) ||
    si->strand != strand)
    {
    flushPosString(si);

    si->chrom = chrom;
    si->strand = strand;
    si->start = start;
    si->end = end;
    }

if (strand == '+')
    {
    si->end = end;
    }
else
    {
    si->start = start;
    }
}

/* copy the maf alignments into the species sequence buffers.
 * remove all the dashes from the reference sequence, and collapse
 * all the separate maf blocks into one sequence
 */
static int copyAli(struct hash *siHash, struct mafAli *ali, int start)
{
struct mafComp *comp = ali->components;
int jj;

for(; comp; comp = comp->next)
    {
    char *chrom = strchr(comp->src, '.');

    if (chrom == NULL)
	errAbort("all components must have a '.'");

    *chrom++ = 0;

    struct speciesInfo *si = hashFindVal(siHash, comp->src);
    if (si == NULL)
	continue;

    if (comp->strand == '+')
	updatePosString(si, chrom, comp->strand, 
	    comp->start, comp->start + comp->size);
    else
	updatePosString(si, chrom, comp->strand, 
	    comp->srcSize - (comp->start + comp->size), 
	    comp->srcSize - comp->start);


    char *tptr = ali->components->text;
    int size = 0;
    for(jj = 0 ; jj < ali->textSize; jj++)
	if (*tptr++ != '-')
	    size++;

    /* check to make sure maf is sane (no overlaps) */
    if (start + size >= si->size + 1)
	errAbort("bad maf, nucSequence buffer overflow %d %d %d\n", 
	    start,size, si->size);

    char *cptr = comp->text;
    char *sptr = &si->nucSequence[start];
    char *mptr = ali->components->text;
    if (cptr != NULL)
	{
	for(jj = 0 ; jj < ali->textSize; jj++)
	    {
	    if (*mptr++ != '-')
		{
		if (cptr != NULL)
		    *sptr++ = *cptr++;
		}
	    else 
		cptr++;
	    }
	}
    }

char *mptr = ali->components->text;
int count = 0;

for(jj = 0 ; jj < ali->textSize; jj++)
    if (*mptr++ != '-')
	count++;

return start + count;
}

/* copyMafs - copy all the maf alignments into 
 * one sequence for each species
 */
static void copyMafs(struct hash *siHash, struct exonInfo **giList, 
    boolean inExons)
{
int start = 0;
struct exonInfo *gi = *giList;

for(; gi; gi = gi->next)
    {
    int thisSize = 0;
    struct mafAli *ali = gi->ali;

    for(; ali; ali = ali->next)
	{
	int newStart = copyAli(siHash, ali, start);
	thisSize += newStart - start;
	start = newStart;
	}
    struct hashCookie cookie =  hashFirst(siHash);
    struct hashEl *hel;

    if (inExons || (gi->next == NULL))
	while ((hel = hashNext(&cookie)) != NULL)
	    {
	    struct speciesInfo *si = hel->val;

	    pushPosString(si);
	    }
    }

boolean frameNeg = ((*giList)->strand == '-');

if (frameNeg)
    {
    int size = 0;
    struct hashCookie cookie =  hashFirst(siHash);
    struct hashEl *hel;
    
    verbose(3, "flipping exons\n");
    while ((hel = hashNext(&cookie)) != NULL)
	{
	struct speciesInfo *si = hel->val;

	if (si == NULL)
	    continue;

	if (size != 0)
	    assert(size == si->size);
	else
	    size = si->size;
	reverseComplement(si->nucSequence, si->size);

	slReverse(&si->posStrings);
	}

    gi = *giList;
    for(; gi; gi = gi->next)
	{
	verbose(3, "old start %d ",gi->exonStart);
	gi->exonStart = size - (gi->exonStart + gi->exonSize);
	verbose(3, "new start %d size %d \n",gi->exonStart, gi->exonSize);
	if (gi->next == NULL)
	    assert(gi->exonStart == 0);
	}

    slReverse(giList);
    }
}

/* free species info list */
static void freeSpeciesInfo(struct speciesInfo *list)
{
struct speciesInfo *siNext;

for(; list ; list = siNext)
    {
    siNext = list->next;

    freez(&list->nucSequence);
    freez(&list->aaSequence);
    slNameFreeList(&list->posStrings);
    }
}

/* free exonInfo list */
static void freeGIList(struct exonInfo *list)
{
struct exonInfo *giNext;

for(; list ; list = giNext)
    {
    giNext = list->next;

    mafAliFreeList(&list->ali);
    }
}
 
static struct exonInfo *buildGIList(char *database, struct genePred *pred, 
    char *mafTable)
{
struct exonInfo *giList = NULL;
unsigned *exonStart = pred->exonStarts;
unsigned *lastStart = &exonStart[pred->exonCount];
unsigned *exonEnd = pred->exonEnds;
int *frames = pred->exonFrames;

if (frames == NULL)
    {
    genePredAddExonFrames(pred);
    frames = pred->exonFrames;
    }

assert(frames != NULL);

int start = 0;


/* first skip 5' UTR */
for(; exonStart < lastStart; exonStart++, exonEnd++, frames++)
    {
    int size = *exonEnd - *exonStart;

    if (*exonStart + size > pred->cdsStart)
	break;
    }

for(; exonStart < lastStart; exonStart++, exonEnd++, frames++)
    {
    struct exonInfo *gi;
    int thisStart = *exonStart;

    if (thisStart < pred->cdsStart)
	thisStart = pred->cdsStart;

    int thisEnd = *exonEnd;

    if (thisEnd > pred->cdsEnd)
	thisEnd = pred->cdsEnd;

    int thisSize = thisEnd - thisStart;
    verbose(3, "in %d %d cds %d %d\n",*exonStart,*exonEnd, thisStart, thisEnd);

    AllocVar(gi);
    gi->frame = *frames;
    gi->name = pred->name;
    gi->ali = getAliForRange(database, mafTable, pred->chrom, 
	thisStart, thisEnd);
    gi->chromStart = thisStart;
    gi->chromEnd = thisEnd;
    gi->exonStart = start;
    gi->exonSize = thisSize;
    verbose(3, "exon size %d\n", thisSize);
    gi->strand = pred->strand[0];
    start += gi->exonSize;
    slAddHead(&giList, gi);

    if (thisEnd == pred->cdsEnd)
	break;
    }
slReverse(&giList);

return giList;
}

void mafGeneOutPred(FILE *f, struct genePred *pred, char *dbName, 
    char *mafTable,  struct slName *speciesNameList, unsigned options,
    int numCols) 
{
boolean inExons = options & MAFGENE_EXONS;

if (pred->cdsStart == pred->cdsEnd)
    return;

if (numCols < -1)
    errAbort("Number of columns must be zero or greater.");

struct exonInfo *giList = buildGIList(dbName, pred, mafTable);

if (giList == NULL)
    return;

struct hash *speciesInfoHash = newHash(5);
struct speciesInfo *speciesList = getSpeciesInfo(giList, speciesNameList, 
    speciesInfoHash);

copyMafs(speciesInfoHash, &giList, inExons);

struct speciesInfo *si = speciesList;
for(; si ; si = si->next)
    si->curPosString = si->posStrings;

writeOutSpecies(f, dbName, speciesList, giList, options, numCols);

freeSpeciesInfo(speciesList);
freeGIList(giList);
}
