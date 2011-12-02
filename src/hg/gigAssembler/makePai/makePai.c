/* makePai - make pair file for est's or bac ends. */
#include "common.h"
#include "hash.h"
#include "localmem.h"
#include "linefile.h"


void usage()
/* Print usage and exit. */
{
errAbort(
  "makePai - make est.pai or bacEnds.pai file from .ra file or .info file\n"
  "usage:\n"
  "    makePai type inFile out.pai\n"
  "where 'type' is either 'est' or 'bacEnds'\n"
  "and inFile must be either .ra or .info\n");
}

char *skipWord(char *fw)
/* skips over current word to start of next. 
 * Error for this not to exist. */
{
char *s;
s = skipToSpaces(fw);
if (s == NULL)
    errAbort("Expecting two words in .ra file line %s\n", fw);
s = skipLeadingSpaces(s);
if (s == NULL)
    errAbort("Expecting two words in .ra file line %s\n", fw);
return s;
}

void trimEnd(char *string, char *end)
/* If string ends with end, trim off end. */
{
int sLen = strlen(string);
int endLen = strlen(end);
int startEnd = sLen - endLen;

if (startEnd < 0)
    return;
if (sameString(string+startEnd, end))
    string[startEnd] = 0;
}


struct pair
/* Pair of bacs or ests. */
    {
    struct pair *next;
    char *a;  /* 5' if EST. */
    char *b;  /* 3' if EST. */
    char *clone;  /* Name of clone. */
    };

struct hash *cloneHash;
struct pair *pairList;

struct lm *lm;  /* Local memory pool */

char *lCloneString(char *s)
/* Clone string into local memory. */
{
int len = strlen(s) + 1;
char *d = lmAlloc(lm, len);
memcpy(d, s, len);
return d;
}

struct pair *newPair()
/* Allocate new pair. */
{
return lmAlloc(lm, sizeof(struct pair));
}

void savePairs(char *fileName)
/* Save pair list to fileName. */
{
FILE *f = mustOpen(fileName, "w");
struct pair *pair;
for (pair = pairList; pair != NULL; pair = pair->next)
    {
    if (pair->a != NULL && pair->b != NULL)
	{
	fprintf(f, "%s\t%s\t%s\n", pair->a, pair->b, pair->clone);
	if (ferror(f))
	    {
	    perror("");
	    errAbort("Write error to %s, aborting\n", fileName);
	    }
	}
    }
fclose(f);
}

void addEst(struct lineFile *lf, char *acc, char *clo, char *dir)
/* Add est to paairlist. */
{
struct hashEl *hel;
struct pair *pair;

if (acc[0] == 0 || clo[0] == 0 || dir[0] == 0)
    {
    // warn("Incomplete EST line %d of %s\n", lf->lineIx, lf->fileName);
    return;
    }
if ((hel = hashLookup(cloneHash, clo)) == NULL)
    {
    pair = newPair();
    hel = hashAdd(cloneHash, clo, pair);
    slAddHead(&pairList, pair);
    pair->clone = hel->name;
    }
else
    pair = hel->val;
if (dir[0] == '5')
    pair->a = lCloneString(acc);
else if (dir[0] == '3')
    pair->b = lCloneString(acc);
else
    errAbort("Wierd direction %s for EST %s\n", dir, acc);
}

void addBac(struct lineFile *lf, char *acc, char *clo)
/* Add bacEnds to pair list. */
{
struct hashEl *hel;
struct pair *pair;
static char *sp6End = " SP6 end";
static char *t7End = " T7 end";


if (acc[0] == 0 || clo[0] == 0 )
    {
    // warn("Incomplete BAC line %d of %s\n", lf->lineIx, lf->fileName);
    return;
    }
trimEnd(clo, sp6End);
trimEnd(clo, t7End);
if ((hel = hashLookup(cloneHash, clo)) == NULL)
    {
    pair = newPair();
    hel = hashAdd(cloneHash, clo, pair);
    slAddHead(&pairList, pair);
    pair->a = lCloneString(acc);
    pair->clone = hel->name;
    }
else
    {
    pair = hel->val;
    pair->b = lCloneString(acc);
    }
}

void makeRaPai(boolean isEst, char *raName, char *outName)
/* Make pair file. */
{
struct lineFile *lf = lineFileOpen(raName, TRUE);
static char acc[256];
static char clo[512];
static char dir[16];
int lineSize;
char *line;

printf("processing %s", raName);
fflush(stdout);
while (lineFileNext(lf, &line, &lineSize))
    {
    if ((lf->lineIx & 0xffff) == 0)
	{
	printf(".");
	fflush(stdout);
	}
    if (lineSize <=  2)  /* Blank line */
	{
	if (isEst)
	    addEst(lf, acc, clo, dir);
	else
	    addBac(lf, acc, clo);
	acc[0] = clo[0] = dir[0] = 0;
	}
    else if (startsWith("acc", line))
	strcpy(acc, skipWord(line));
    else if (startsWith("clo", line))
	strcpy(clo, skipWord(line));
    else if (startsWith("dir", line))
	strcpy(dir, skipWord(line));
    }
slReverse(&pairList);
printf("\n");
lineFileClose(&lf);
}

void makeInfoPai(boolean isEst, char *inName, char *outName)
/* Make a pair based on .info file - which has two fields:
 *    accession clone */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
char *row[2];
char acc[128];

if (isEst)
    errAbort("Currently only handle BAC .info files.");
while (lineFileRow(lf, row))
    {
    strcpy(acc, row[0]);
    chopSuffix(acc);
    addBac(lf, acc, row[1]);
    }
lineFileClose(&lf);
}


void makePai(boolean isEst, char *inName, char *outName)
/* Decide what the input type is, and call appropriate reader.
 * Then write out list. */
{
cloneHash = newHash(20);
if (endsWith(inName, ".ra"))
    makeRaPai(isEst, inName, outName);
else if (endsWith(inName, ".info"))
    makeInfoPai(isEst, inName, outName);
else
    usage();
printf("Saving %s\n", outName);
savePairs(outName);
}


int main(int argc, char *argv[])
/* Process command line and exit. */
{
boolean isEst;
char *type;

if (argc != 4 )
    usage();
type = argv[1];
if (sameWord(type, "est"))
    isEst = TRUE;
else if (sameWord(type, "bacEnds"))
    isEst = FALSE;
else
    usage();
lm = lmInit(0);
makePai(isEst, argv[2], argv[3]);
}
