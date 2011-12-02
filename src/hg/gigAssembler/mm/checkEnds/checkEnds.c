/* checkEnds - Check that ends output for a set is reasonable. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dlist.h"


boolean anyProblem = FALSE;

void report(char *format, ...)
/* Print error message to stdout and note there was a problem. */
{
va_list args;
va_start(args, format);
vfprintf(stdout, format, args);
va_end(args);
anyProblem = TRUE;
}


void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkEnds - Check that ends output for a set is reasonable\n"
  "usage:\n"
  "   checkEnds testSet\n");
}

struct barge
/* Info on a barge. */
    {
    struct barge *next;
    struct clone *cloneList;
    struct dlList *endList;
    int cloneCount;
    };

struct clone
/* Info on a clone. */
    {
    struct clone *next;
    char *name;
    int start;
    int end;
    int orientation;
    struct cloneEnd *endList;
    struct barge *barge;
    };

struct cloneEnd
/* Info on a clone end. */
    {
    struct cloneEnd *next;
    boolean isStart;
    struct clone *clone;
    };

void dumpBarge(struct barge *barge, FILE *f)
/* Dump out (some) info on barge to file. */
{
struct clone *clone;
for (clone = barge->cloneList; clone != NULL; clone = clone->next)
    fprintf(f, "%s ", clone->name);
fprintf(f, "\n");
}

int bargeCmpCloneCount(const void *va, const void *vb)
/* Compare to sort based on score. */
{
const struct barge *a = *((struct barge **)va);
const struct barge *b = *((struct barge **)vb);
return b->cloneCount - a->cloneCount;
}

struct barge *sortBargeList(struct barge *bargeList)
/* Sort barge list by biggest first. */
{
struct barge *barge;
for (barge = bargeList; barge != NULL; barge = barge->next)
    barge->cloneCount = slCount(barge->cloneList);
slSort(&bargeList, bargeCmpCloneCount);
return bargeList;
}

void parseCloneName(char *in, char **retClone, int *retStart, int *retEnd)
/* Parse out string of form name@number_number. */
{
static char name[64];
char dupe[64], *parts[4];
int start, end, partCount;

strcpy(dupe, in);
partCount = chopString(dupe, "@_)", parts, ArraySize(parts));
if (partCount != 3)
    errAbort("Unparsable clone name %s", in);
strcpy(name, parts[0]);

if (!isdigit(parts[1][0]) || !isdigit(parts[2][0]))
    errAbort("Expecting integer in clone name %s", in);
start = atoi(parts[1]);
end = atoi(parts[2]);

*retClone = name;
*retStart = start;
*retEnd = end;
}

void parseCloneEnd(char *in, char **retClone, int *retStart, int *retEnd,
	int *retOrientation, boolean *retIsStart)
/* Parse out a clone end string into fields. */
{
int orientation = 0;
boolean isStart = FALSE;
char *s = in;
char c;

c = *s;
if (c == '+')
    {
    isStart = TRUE;
    orientation = +1;
    s += 2;
    }
else if (c == '-')
    {
    isStart = TRUE;
    orientation = -1;
    s += 2;
    }
else if (c == '?')
    {
    isStart = TRUE;
    orientation = 0;
    s += 2;
    }
parseCloneName(s, retClone, retStart, retEnd);
*retOrientation = orientation;
*retIsStart = isStart;
}

struct barge *loadAnswerFile(char *fileName)
/* Load barges from answer file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[8];
struct clone *lastClone = NULL, *clone;
struct barge *barge, *bargeList = NULL;
struct cloneEnd *cloneStart, *cloneEnd;
int start,end;
char *cloneName;
int farthestEnd = 0;

while (lineFileRow(lf, words))
    {
    parseCloneName(words[0], &cloneName, &start, &end);
    AllocVar(clone);
    clone->name = cloneString(cloneName);
    clone->start = start;
    clone->end = end;
    if (words[7][0] == '1')
        clone->orientation = 1;
    else
        clone->orientation = -1;

    AllocVar(cloneStart);
    cloneStart->clone = clone;
    cloneStart->isStart = TRUE;
    slAddTail(&clone->endList, cloneStart);
    AllocVar(cloneEnd);
    cloneEnd->clone = clone;
    cloneEnd->isStart = FALSE;
    slAddTail(&clone->endList, cloneEnd);

    if (lastClone == NULL || farthestEnd <= clone->start)
	{
	AllocVar(barge);
	slAddHead(&bargeList, barge);
	}

    if (clone->end > farthestEnd)
        farthestEnd = clone->end;
    slAddTail(&barge->cloneList, clone);
    clone->barge = barge;
    lastClone = clone;
    }
lineFileClose(&lf);
slReverse(&bargeList);
return bargeList;
}

int countForwardEnds(struct lineFile *lf, struct dlList *endList, boolean squawk)
/* Count up number of ends in a row that could be forward. */
{
struct dlNode *node;
int lastPos = -1, pos;
struct cloneEnd *ce;
struct clone *clone;
int count = 0;
int totalRev = 0;

for (node = endList->head; !dlEnd(node); node = node->next)
    {
    ce = node->val;
    clone = ce->clone;
    if (ce->isStart)
        pos = clone->start;
    else
        pos = clone->end;
    if (lastPos <= pos)
	++count;
    else 
	{
        if (squawk)
	    {
	    if (totalRev == 0)
		report("Reversal at clone %s line %d of %s\n", 
		    clone->name, lf->lineIx, lf->fileName);
	    totalRev += (lastPos - pos);
	    }
	else
	    break;
	}
    lastPos = pos;
    }
if (squawk)
   report("  Total reversal %d\n", totalRev);
return count;
}


int countReverseEnds(struct lineFile *lf, struct dlList *endList, boolean squawk)
/* Count up number of ends in a row that could be reverse. */
{
struct dlNode *node;
int lastPos = -1, pos;
struct cloneEnd *ce;
struct clone *clone;
int count = 0;
int totalRev = 0;

for (node = endList->tail; !dlStart(node); node = node->prev)
    {
    ce = node->val;
    clone = ce->clone;
    if (ce->isStart)
        pos = clone->end;
    else
        pos = clone->start;
    if (lastPos <= pos)
	++count;
    else 
	{
        if (squawk)
	    {
	    if (totalRev == 0)
		report("Backwards reversal at clone %s line %d of %s.\n", 
		    clone->name, lf->lineIx, lf->fileName);
	    totalRev += (lastPos - pos);
	    }
	else
	    break;
	}
    lastPos = pos;
    }
if (squawk)
   report("  Total reversal %d\n", totalRev);
return count;
}


struct barge *checkBargeEnds(struct lineFile *lf, int endCount, char *endStrings[])
/* Read barge line and make sure that barge is internally consistent. */
{
struct hash *cloneHash = newHash(8);
char *cloneName;
int i,start,end,orientation;
boolean isStart;
char *endString;
struct clone *clone;
struct cloneEnd *ce, *lastCe = NULL;
int lastStart, lastEnd;
int lastPos, lastRevPos, pos, revPos;
int direction = 0;
int forCount, revCount;
struct barge *barge;

AllocVar(barge);
barge->endList = newDlList();
if (endCount&1)
    {
    report("Odd clone end count line %d of %s\n", lf->lineIx, lf->fileName);
    return barge;
    }

for (i=0; i<endCount; ++i)
    {
    endString = endStrings[i];
    parseCloneEnd(endString, &cloneName, &start, &end, &orientation, &isStart);
    clone = hashFindVal(cloneHash, cloneName);
    if (clone == NULL)
       {
       if (!isStart)
	   {
	   report("First appearance of clone %s is not a start line %d of %s\n",
		endString, lf->lineIx, lf->fileName);
	   }
       AllocVar(clone);
       hashAdd(cloneHash, cloneName, clone);
       clone->name = cloneString(cloneName);
       clone->start = start;
       clone->end = end;
       clone->orientation = orientation;
       clone->barge = barge;
       slAddHead(&barge->cloneList, clone);
       }
    else
       {
       if (isStart)
	   {
           report("Second appearance of clone %s is not end line %d of %s\n",
		endString, lf->lineIx, lf->fileName);
	   }
	       
       }
    AllocVar(ce);
    ce->isStart = isStart;
    ce->clone = clone;
    slAddTail(&clone->endList, ce);
    dlAddValTail(barge->endList, ce);
    }

forCount = countForwardEnds(lf, barge->endList, FALSE);
revCount = countReverseEnds(lf, barge->endList, FALSE);
if (forCount != endCount && revCount != endCount)
    {
    if (forCount >= revCount)
        countForwardEnds(lf, barge->endList, TRUE);
    else
        countReverseEnds(lf, barge->endList, TRUE);
    }

slReverse(&barge->cloneList);
for (clone = barge->cloneList; clone != NULL; clone = clone->next)
    {
    int endCount = slCount(clone->endList);
    if (endCount != 2)
        report("Got %d ends on clone %s line %d of %s\n", endCount, clone->name,
	    lf->lineIx, lf->fileName);
    }
freeHash(&cloneHash);
return barge;
}

struct barge *checkEndsFile(char *fileName)
/* Check consistency of ends in one file.  Return list of
 * barges from file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
static char *words[4096], *line;
int wordCount, lineSize;
struct barge *list = NULL, *barge;

while (lineFileNext(lf, &line, &lineSize))
    {
    /* Skip blank and commented lines. */
    if (line[0] == '#')
        continue;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;
    barge = checkBargeEnds(lf, wordCount, words);
    slAddHead(&list, barge);
    }
lineFileClose(&lf);
slReverse(list);
return list;
}

struct hash *hashClonesInBargeList(struct barge *bargeList, char *fileName)
/* Add all clones in barge to hash keyed on clone name. */
{
struct hash *hash = newHash(0);
struct barge *barge;
struct clone *clone;
for (barge = bargeList; barge != NULL; barge = barge->next)
    {
    for (clone = barge->cloneList; clone != NULL; clone = clone->next)
	{
	if (hashLookup(hash, clone->name))
	    report("%s duplicated in %s\n", clone->name, fileName);
	else
	    hashAdd(hash, clone->name, clone);
	}
    }
return hash;
}

void crossCheck(struct barge *bargeList, struct hash *cloneHash, char *listName,
	char *hashName)
/* Make sure that all clones in barges in list exist in hash, and are in same barge
 * in hash if they are in same barge on list. */
{
struct barge *listBarge, *hashBarge;
struct clone *listClone, *hashClone;

for (listBarge = bargeList; listBarge != NULL; listBarge = listBarge->next)
    {
    hashBarge = NULL;
    for (listClone = listBarge->cloneList; listClone != NULL; listClone = listClone->next)
        {
	hashClone = hashFindVal(cloneHash, listClone->name);
	if (hashClone == NULL)
	    report("clone %s is in .end but not .answer\n", listClone->name);
	if (hashBarge == NULL)
	    hashBarge = hashClone->barge;
	else
	    {
	    if (hashBarge != hashClone->barge)
	         {
		 report("%s and %s are in same barge in %s but different barges in %s\n", 
		 	listBarge->cloneList->name, listClone->name, listName, hashName);
		 break;
		 }
	    }
	}
    }
}

void checkAgainstAnswer(struct barge *mmList, struct barge *answerList)
/* Check barges in mmList are consistent with those in answer list. */
{
int mmCount = slCount(mmList);
int answerCount = slCount(answerList);
struct barge *aBarge, *mBarge;
struct clone *aClone, *mClone;
struct hash *answerHash = hashClonesInBargeList(answerList, ".answer");
struct hash *mmHash = hashClonesInBargeList(mmList, ".end");

if (mmCount != answerCount)
    report("%d barges in .answer, %d in .ends\n", answerCount, mmCount);
crossCheck(mmList, answerHash, ".end", ".answer");
crossCheck(answerList, mmHash, ".answer", ".end");
freeHash(&answerHash);
freeHash(&mmHash);
}

void checkEnds(char *testSet)
/* checkEnds - Check that ends output for a set is reasonable. */
{
char fileName[512];
struct barge *mmBargeList = NULL, *answerBargeList = NULL, *barge;

sprintf(fileName, "%s.answer", testSet);
answerBargeList = loadAnswerFile(fileName);
sprintf(fileName, "%s.ends", testSet);
mmBargeList = checkEndsFile(fileName);
answerBargeList = sortBargeList(answerBargeList);
mmBargeList = sortBargeList(mmBargeList);
checkAgainstAnswer(mmBargeList, answerBargeList);
if (!anyProblem)
    printf("No problems detected in %s\n", fileName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
checkEnds(argv[1]);
return 0;
}
