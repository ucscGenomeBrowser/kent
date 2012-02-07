/* evalEnds - Evaluate extent of end pair info. */
#include "common.h"
#include "hash.h"
#include "linefile.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "evalEnds - Evaluate extent of end pair info\n"
  "usage:\n"
  "   evalEnds ffaDir spFile\n");
}

struct endInfo
/* Clone end info. */
    {
    struct endInfo *next;	/* Next in list. */
    char *contig;                 /* Contig number. */
    char *text;			/* Some sort of text describing end. */
    };

struct clone
/* Info about one clone */
    {
    struct clone *next;	/* Next in list. */
    char *name;		/* Name - not allocated here. */
    struct endInfo *gsList;     /* List of ends from Greg Schuler. */
    struct endInfo *spList;	/* List of ends from Shiaw-Pyng. */
    boolean isFin;		/* True if finished clone. */
    boolean isOrdered;		/* Treu if phase 2 clone. */
    };

char *gsFiles[] = {"finished.finf", "draft.finf", "predraft.finf"};

void evalEnds(char *gsDir, char *spFile)
/* evalEnds - Evaluate extent of end pair info. */
{
struct hash *cloneHash = newHash(15);
struct hashEl *hel;
struct lineFile *lf;
struct clone *cloneList = NULL, *clone = NULL;
struct endInfo *end;
char fileName[512];
int i, wordCount, lineSize;
char *words[16], *line;
char lastClone[64];
char cloneName[64];
int gsInfoCount = 0;


strcpy(lastClone, "");
for (i=0; i<ArraySize(gsFiles); ++i)
    {
    sprintf(fileName, "%s/%s", gsDir, gsFiles[i]);
    lf = lineFileOpen(fileName, TRUE);
    while ((wordCount = lineFileChop(lf, words)) != 0)
        {
	lineFileExpectWords(lf, 7, wordCount);

	if (!sameString(words[1], lastClone))
	    {
	    strcpy(lastClone, words[1]);
	    strcpy(cloneName, words[1]);
	    chopSuffix(cloneName);
	    AllocVar(clone);
	    if (hashLookup(cloneHash, cloneName))
	        warn("%s duplicated\n", cloneName);
	    hel = hashAdd(cloneHash, cloneName, clone);
	    clone->name = hel->name;
	    clone->isFin = (i == 0);
	    slAddHead(&cloneList, clone);
	    }
	if (!clone->isFin && !sameString(words[6], "?") && !sameString(words[6], "i") 
	   && !sameString(words[6], "w"))
	    {
	    char *s = strchr(words[0], '~');
	    if (s == NULL)
	        errAbort("Expecting ~ in fragment name line %d of %s\n", lf->lineIx, lf->fileName);
	    ++s;
	    AllocVar(end);
	    end->contig = cloneString(s);
	    end->text = cloneString(words[6]);
	    slAddHead(&clone->gsList, end);
	    ++gsInfoCount;
	    }
	}
    lineFileClose(&lf);
    }

lf = lineFileOpen(spFile, TRUE);
while (lineFileNext(lf, &line, &lineSize))
    {
    char *s, *e;
    struct clone *clone;

    line = trimSpaces(line);
    s = strchr(line, '.');
    if (s == NULL)
        errAbort("Expecting dot line %d of %s\n", lf->lineIx, lf->fileName);
    *s++ = 0;
    if ((clone = hashFindVal(cloneHash, line)) == NULL)
        {
	warn("%s in %s but not %s", line, spFile, gsDir);
	continue;
	}
    if (!startsWith("Contig", s))
        errAbort("Expecting .Contig line %d of %s\n", lf->lineIx, lf->fileName);
    s += 6;
    e = strrchr(s, '.');
    if (e == NULL)
        errAbort("Expecting last dot line %d of %s\n", lf->lineIx, lf->fileName);
    *e++ = 0;
    AllocVar(end);
    end->contig = cloneString(s);
    end->text = cloneString(e);
    slAddHead(&clone->spList, end);
    }
lineFileClose(&lf);

/* Evaluate just stats on info. */
    {
    int cloneCount = 0, finCount = 0, draftCount = 0, oneEndCount = 0, twoEndCount = 0,
    	moreEndCount = 0;
    int finWithEndCount = 0;
    int gsCount, spCount;
    int spNewOne = 0, spNewTwo = 0, spMulti = 0;

    for (clone = cloneList; clone != NULL; clone = clone->next)
        {
	if (clone->isFin)
	    {
	    ++finCount;
	    if (clone->gsList)
	        ++finWithEndCount;
	    }
	else
	    ++draftCount;
	gsCount = slCount(clone->gsList);
	spCount = slCount(clone->spList);
	if (gsCount == 1 && spCount <= 1 || spCount == 1 && gsCount <= 1)
	    ++oneEndCount;
	else if (gsCount == 2 || spCount == 2)
	    ++twoEndCount;
	else if (gsCount >= 3 || spCount == 3)
	    {
	    ++moreEndCount;
	        {
		static int count = 5;
		if (--count >= 0)
		    printf("Multiple %s\n", clone->name);
		}
	    }
	++cloneCount;
	}
    printf("Info from .finf  and %s files:\n", spFile);
    printf("  %d draft, %d finished, %d total clones\n", draftCount, finCount, cloneCount);
    printf("  %d with one end, %d with two ends\n", oneEndCount, twoEndCount);
    printf("  %d finished with ends, %d more than two ends\n", finWithEndCount, moreEndCount);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
evalEnds(argv[1], argv[2]);
return 0;
}
