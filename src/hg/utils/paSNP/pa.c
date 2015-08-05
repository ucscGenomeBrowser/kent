#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "axt.h"
#include "math.h"
#include "pa.h"

struct  hashContent
{
char *seqBuffer;
char *position;
};


#define MAX_POSITION_SIZE	100*1024
#define SEQBUFFER_SIZE		30*1024

struct slName *getOrderList(char *file)
/* Read in the species list. */
{
struct lineFile *lf = lineFileOpen(file, TRUE);
char *row[1];
struct slName *nameList = NULL;

while (lineFileRow(lf, row))
    {
    struct slName *name = newSlName(row[0]);

    slAddHead(&nameList, name);
    }

slReverse(&nameList);
lineFileClose(&lf);

return nameList;
}

int fillSeqHash(struct slName *list, struct hash *hash)
/* Put sequences into sequence hash. */
{
struct slName *name;
int count = 0;

for(name = list; name; name = name->next)
    {
    struct hashContent *hc;

    AllocVar(hc);
    char *buffer = needMem( SEQBUFFER_SIZE);

    hc->seqBuffer = buffer;
    hc->position = needMem(MAX_POSITION_SIZE);

    hashAdd(hash, name->name, hc);
    count++;
    }

return count;
}

static void clearSpecies(struct hash *seqHash, int exonSize)
/* Empty the sequence buffers. */
{
struct hashCookie cook = hashFirst(seqHash);
struct hashEl *hel;

while((hel = hashNext(&cook)) != NULL)
    {
    struct hashContent *hc = hel->val;
    char *seqBuffer = hc->seqBuffer;

    memset(seqBuffer, '-', exonSize);
    }
}

boolean isDashXorZ(char aa)
/* Is this aa a dash, X, or Z? */
{
if ( (aa == '-') || (aa == 'X') || (aa == 'Z') )
    return TRUE;

return FALSE;
}


int globalCount = 0;

void analyzeAlign(struct alignDetail *detail, alignFunc aFunc, 
    columnFunc cfunc,  void *closure)
/* Analyze this one alignment. */
{
aFunc(detail, cfunc, closure);
}

void parseFasta(struct lineFile *lf, int numSpecies, struct slName *list,
    struct hash *seqHash, alignFunc afunc, columnFunc cfunc, void *closure)
/* Parse an AA fasta, calling the alignment and column functions where appropriate. */
{
char *words[5000];
int wordsRead;
boolean expectGreat = TRUE;
char *seqBuffer = NULL;
struct alignDetail detail;
struct slName *name;

memset(&detail, 0, sizeof(detail));
detail.numSpecies = numSpecies;
detail.seqBuffers = needMem(numSpecies * sizeof(struct seqBuffer));
int ii = 0;

for(name = list; name; name = name->next)
    {
    struct hashContent *hc = hashMustFindVal(seqHash, name->name);

    detail.seqBuffers[ii].position = hc->position;
    detail.seqBuffers[ii].buffer = hc->seqBuffer;
    detail.seqBuffers[ii].species = name->name;
    detail.seqBuffers[ii].lastTwo[0] = '-';
    detail.seqBuffers[ii].lastTwo[1] = '-';
    ii++;
    }

while( (wordsRead = lineFileChopNext(lf, words, sizeof(words)/sizeof(char *)) ))
    {
    if (expectGreat)
	{
	if (*words[0] != '>')
	    errAbort("expect '>' as first char on line %d",lf->lineIx);

	char *pName = words[0];
	char *exonCountStr = strrchr(pName, '_');
	if (exonCountStr == NULL)
	    errAbort("expected to find underbar on line %d",lf->lineIx);
	int newExonCount = atoi(exonCountStr + 1);
	if (newExonCount == 0)
	    errAbort("bad exon count on line %d",lf->lineIx);
	*exonCountStr = 0;
	char *exonNumStr = strrchr(pName, '_');
	if (exonNumStr == NULL)
	    errAbort("expected to find underbar on line %d",lf->lineIx);
	int newExonNum = atoi(exonNumStr + 1);
	*exonNumStr = 0;
	char *species = strrchr(pName, '_');
	*species++ = 0;
	if (species == NULL)
	    errAbort("expected to find species underbar on line %d",lf->lineIx);
	int newExonSize = atoi(words[1]);

	pName++;

	if (newExonSize > SEQBUFFER_SIZE)
	    errAbort("exon is too big (%d) for constant SEQBUFFER (%d)",
		newExonSize, SEQBUFFER_SIZE);
	
	if (newExonSize <= 0)
	    errAbort("expected size > 0 (2nd arg)  on line %d",lf->lineIx);


	if ((detail.proName == NULL) || 
	    !sameString(pName, detail.proName)||
	    (detail.exonNum != newExonNum) ||
	    (detail.exonSize != newExonSize))
	    {
	    if (detail.proName != NULL)
		analyzeAlign(&detail, afunc, cfunc, closure);
	    else
		detail.exonSize = SEQBUFFER_SIZE; // initialize whole buffer

	    struct seqBuffer *sb = detail.seqBuffers;
	    struct seqBuffer *lastSb = &detail.seqBuffers[detail.numSpecies];

	    for(; sb < lastSb; sb++)
		{
		if (detail.exonSize == 1)
		    {
		    sb->lastTwo[0] = '-';
		    sb->lastTwo[1] = sb->buffer[detail.exonSize-1];
		    }
		else
		    memcpy(sb->lastTwo, &sb->buffer[detail.exonSize-2], 2); 
		}
	    clearSpecies(seqHash, detail.exonSize);

	    freez(&detail.proName);
	    detail.proName = cloneString(pName);

	    detail.exonNum = newExonNum;
	    detail.exonCount = newExonCount;
	    detail.exonSize = newExonSize;
	    detail.startFrame = atoi(words[2]);
	    detail.endFrame = atoi(words[3]);
	    freez(&detail.geneName);
	    if (wordsRead == 6)
		detail.geneName = cloneString(words[5]);
	    }

	struct hashContent *hc = hashMustFindVal(seqHash, species);
	seqBuffer = hc->seqBuffer;
	if (strlen(words[4]) >  MAX_POSITION_SIZE)
	    errAbort("overflowed position buffer have %d need %d",
		MAX_POSITION_SIZE, (int)strlen(words[4]));

	strcpy(hc->position, words[4]);

	expectGreat = FALSE;
	}
    else
	{
	expectGreat = TRUE;
	if (wordsRead != 1)
	    errAbort("expect only one word with sequence on line %d\n",lf->lineIx);
	if (strlen(words[0]) != detail.exonSize)
	    errAbort("expect exonSize to be %d on line %d\n",detail.exonSize,lf->lineIx);
	memcpy(seqBuffer, words[0], detail.exonSize);
	}
    }
if (detail.proName != NULL)
    analyzeAlign(&detail, afunc, cfunc, closure);
}


void fullColumns( struct alignDetail *detail, columnFunc cfunc, void *closure)
/* Call column function on columns that are fully populated. */
{
int ii;

for(ii = 0; ii < detail->exonSize; ii++)
    {
    int jj;

    for(jj= 0; jj < detail->numSpecies; jj++)
	{
	struct seqBuffer *sb = &detail->seqBuffers[jj];

	if (isDashXorZ( sb->buffer[ii]))
	    break;
	}

    if (jj == detail->numSpecies)
	(*cfunc)(detail, ii, closure);
    }
}

void binColumns( struct alignDetail *detail, columnFunc cfunc, void *closure)
/* Call column function on columns that have exactly two values. */
{
int ii;

for(ii = 0; ii < detail->exonSize; ii++)
    {
    int jj;
    char firstVal = 0;
    char anotherVal = 0;

    for(jj= 0; jj < detail->numSpecies; jj++)
	{
	struct seqBuffer *sb = &detail->seqBuffers[jj];
	char ch = sb->buffer[ii];

	if (isDashXorZ( ch ))
	    break;

	if (firstVal == 0)
	    {
	    firstVal = ch;
	    }
	else if (firstVal != ch)
	    {
	    if (anotherVal == 0)
		anotherVal = ch;
	    else
		{
		if (anotherVal != ch)
		    break;
		}
	    }
	}

    if ((anotherVal != 0) && (jj == detail->numSpecies))
	(*cfunc)(detail, ii, closure);
    }
}

void allColumns( struct alignDetail *detail, columnFunc cfunc, void *closure)
/* Call column function on all columns. */
{
int ii;

for(ii = 0; ii < detail->exonSize; ii++)
    (*cfunc)(detail, ii, closure);
}

void parseAli(char *orderFileName, char *fastaFile, 
    alignFunc afunc, columnFunc cfunc, void *closure)
/* Parse a file of alignments. */
{
struct hash *seqHash = newHash(5);
struct slName *list = getOrderList(orderFileName);
int numSpecies = fillSeqHash(list, seqHash);
struct lineFile *lf = lineFileOpen(fastaFile, TRUE);

parseFasta(lf, numSpecies, list, seqHash, afunc, cfunc,  closure);
slNameFreeList(&list);
}

struct hash *getOrderHash(char *file)
/* Parse species order file into a hash. */
{
struct lineFile *lf = lineFileOpen(file, TRUE);
char *row[1];
int count = 0;
struct hash *hash = newHash(0);
while (lineFileRow(lf, row))
    {
    hashAddInt(hash, row[0], count);
    count++;
    }
lineFileClose(&lf);
return hash;
}

struct aaMap *allocAAMap()
/* Allocate space for a amino acid counts. */
{
struct aaMap *ret;

AllocVar(ret);

ret->empty = needMem(sizeof(int) * 256);
ret->map = needMem(sizeof(double) * 256);

return ret;
}

struct aaMap *readAAMap(char *fileName)
/* Read in a amino acid counts from a file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[1024];
int wordsRead;
int ii;
struct aaMap *am;

am = allocAAMap();

for(ii=0; ii < 256; ii++)
    am->empty[ii] = TRUE;

while( (wordsRead = lineFileChopNext(lf, words, sizeof(words)/sizeof(char *))) > 0)
    {
    if (wordsRead != 2)
	errAbort("expect two words on line %d of %s\n", lf->lineIx, fileName);

    if (strlen(words[0]) > 1)
	errAbort("expect first word to be one char on line %d of %s\n", lf->lineIx, fileName);

    int aa = *words[0];
    am->map[aa] = atof(words[1]);
    am->empty[aa] = FALSE;
    }
lineFileClose(&lf);

return am;
}

char *getPosString(char *oldPos, char strand, int resNum, int sfn, int efn)
/* Generated a position sequence in UCSC standard format. */
{
static char buffer[10*1024];

char *chrom = oldPos;
char *startPos = strchr(oldPos,':');
*startPos = 0;
startPos++;

int start = atoi(startPos);

char *endPos = strchr(startPos,'-');
endPos++;

int end = atoi(endPos);
resNum *= 3;
if (strand == '+')
    {
    start += resNum;
    if (sfn == 1)
	start--;
    if (sfn == 2)
	start++;
    end = start + 2;
    }
else
    {
    end -= resNum;
    if (sfn == 2)
	end--;
    if (sfn == 1)
	end++;
    start = end - 2;
    }


safef(buffer, sizeof buffer, "%s:%d-%d", chrom, start, end);
startPos--;
*startPos = ':';

return buffer;
}
