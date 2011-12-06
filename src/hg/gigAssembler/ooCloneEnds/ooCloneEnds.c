/* ooCloneEnds - create cloneEnds files in each contig in ooDir. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "obscure.h"
#include "hCommon.h"
#include "ooUtils.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "ooCloneEnds - create cloneEnds files in each contig in ooDir\n"
  "usage:\n"
  "   ooCloneEnds ffaDir shiawPingEndsFile ooDir\n");
}

struct endInfo
/* Clone end info. */
    {
    struct endInfo *next;	/* Next in list. */
    char *contig;                 /* Contig number. */
    char *text;			/* Some sort of text describing end. */
    char lr;			/* L,R, or ? */
    };

struct clone
/* Info about one clone */
    {
    struct clone *next;	/* Next in list. */
    char *name;		/* Name - not allocated here. */
    char version[8];	/* Version - .something. */
    struct endInfo *gsList;     /* List of ends from Greg Schuler. */
    struct endInfo *spList;	/* List of ends from Shiaw-Pyng. */
    boolean isFin;		/* True if finished clone. */
    boolean isOrdered;		/* True if phase 2 clone. */
    int fragCount;		/* Number of fragments in clone. */
    struct frag *fragList;	/* List of fragments in clone. */
    int phase;			/* HTG PHASE (3 = finished, 2 = ordered, 1 = draft. */
    int size;			/* Clone size. */
    };

struct frag
/* Info about a piece of a clone. */
    {
    struct frag *next;
    char name[8];
    };

struct frag *newFrag(char *fullFinfName, struct lineFile *lf)
/* Return a frag based on full name in Finf file. */
{
char *s;
struct frag *frag;

AllocVar(frag);
s = strchr(fullFinfName, '~');
if (s == NULL || strlen(s) > sizeof(frag->name) || !isdigit(s[1]))
    errAbort("Badly formated name %s line %d of %s", fullFinfName, lf->lineIx, lf->fileName);
strcpy(frag->name, s+1);
subChar(frag->name, '.', '_');
return frag;
}

char *gsFiles[] = {"finished.finf", "extras.finf", "draft.finf", "predraft.finf"};

struct hash *cloneHash;
struct clone *cloneList = NULL;

void readFinfFiles(char *gsDir)
/* Read in .finf files and save info in cloneHash/cloneList. */
{
struct lineFile *lf;
struct clone *clone = NULL;
struct endInfo *end;
char fileName[512];
int i;
char *words[7];
char lastClone[64];
char cloneName[64];
int gsInfoCount = 0;
struct frag *frag;
boolean isFin;
char *s, *e;

strcpy(lastClone, "");
for (i=0; i<ArraySize(gsFiles); ++i)
    {
    isFin = (i <= 0);
    sprintf(fileName, "%s/%s", gsDir, gsFiles[i]);
    printf("Reading info from %s\n", fileName);
    lf = lineFileOpen(fileName, TRUE);
    while (lineFileRow(lf, words))
        {
	if (!sameString(words[1], lastClone))
	    {
	    struct clone *oldClone;
	    strcpy(lastClone, words[1]);
	    strcpy(cloneName, words[1]);
	    AllocVar(clone);
	    s = strchr(cloneName, '.');
	    if (s == NULL)
	        errAbort("Bad clone name format line %d of %s\n", lf->lineIx, lf->fileName);
	    if (strlen(s) >= sizeof(clone->version))
	        errAbort("Bad clone name format line %d of %s\n", lf->lineIx, lf->fileName);
	    strcpy(clone->version, s);
	    chopSuffix(cloneName);
	    clone->size = atoi(words[3]);
	    if ((oldClone = hashFindVal(cloneHash, cloneName)) != NULL)
		{
		if (isFin && clone->size == oldClone->size && sameString(clone->version, oldClone->version))
		    warn("Apparently benign duplication of %s line %d of %s", cloneName, lf->lineIx, lf->fileName);
		else
		    warn("%s duplicated line %d of %s (size %d oldSize %d)", cloneName, lf->lineIx, lf->fileName,
		    	clone->size, oldClone->size);
		}
	    hashAddSaveName(cloneHash, cloneName, clone, &clone->name);
	    clone->isFin = isFin;
	    slAddHead(&cloneList, clone);
	    }
	frag = newFrag(words[0], lf);
	slAddTail(&clone->fragList, frag);
	++clone->fragCount;
	if (!clone->isFin && !sameString(words[6], "?") && !sameString(words[6], "i") 
	   && !sameString(words[6], "w"))
	    {
	    char *s = strchr(words[0], '~');
	    char c;

	    if (s == NULL)
	        errAbort("Expecting ~ in fragment name line %d of %s\n", lf->lineIx, lf->fileName);
	    ++s;
	    AllocVar(end);
	    end->contig = cloneString(s);
	    subChar(s, '.', '_');
	    end->text = cloneString(words[6]);
	    c = lastChar(end->text);
	    if (!(c == 'L' || c == 'R'))
	        c = '?';
	    end->lr = c;
	    slAddHead(&clone->gsList, end);
	    ++gsInfoCount;
	    }
	}
    lineFileClose(&lf);
    }
printf("Found %d ends in %d clones\n", gsInfoCount, slCount(cloneList));
}

void addPhaseInfo(char *gsDir)
/* Add in phase to clones from sequence.inf file. */
{
char fileName[512];
struct lineFile *lf;
char *words[8];
struct clone *clone;
char *cloneName;

sprintf(fileName, "%s/sequence.inf", gsDir);
printf("Scanning %s for phase info\n", fileName);
lf = lineFileOpen(fileName, TRUE);
while (lineFileRow(lf, words))
    {
    cloneName = words[0];
    chopSuffix(cloneName);
    if ((clone = hashFindVal(cloneHash, cloneName)) == NULL)
        {
	if (!sameString(words[3], "0"))
	    warn("%s is in %s but not .finf files", cloneName, fileName);
	continue;
	}
    clone->phase = atoi(words[3]);
    if (clone->phase <= 0 || clone->phase > 3)
	{
	warn("Bad phase %s line %d of %s", words[3], lf->lineIx, lf->fileName);
	continue;
	}
    if (clone->phase == 3)
	{
        if (!clone->isFin)
	    warn("Clone %s is finished in sequence.inf but not in finished.finf", cloneName);
	}
    else
        {
	if (clone->isFin)
	    warn("Clone %s is in finished.fin but is phase %s in sequence.inf", cloneName, words[3]);
	}
    }
lineFileClose(&lf);
}


void addBacEndInfo(char *spFile)
/* Add BAC end info from Shiaw-Pyng's file to clones in cloneHash. */
{
struct lineFile *lf = lineFileOpen(spFile, TRUE);
char *line;
int lineSize, wordCount;
int spCount = 0;
char *words[16];

while (lineFileNext(lf, &line, &lineSize))
    {
    char *s, *e, c;
    struct clone *clone;
    struct endInfo *end;
    char *firstWord;
    char *contig;

    if (line[0] == '#')
       continue;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;
    firstWord = words[0];
    s = strchr(firstWord, '.');
    if (s == NULL)
	errAbort("Expecting dot line %d of %s\n", lf->lineIx, lf->fileName);
    *s++ = 0;
    if ((clone = hashFindVal(cloneHash, firstWord)) == NULL)
	{
	warn("%s in %s but not .finf files", firstWord, spFile);
	continue;
	}
    if (!startsWith("Contig", s))
	errAbort("Expecting .Contig line %d of %s\n", lf->lineIx, lf->fileName);
    s += 6;
    contig = s;
    if (wordCount == 1)
	{
	/* Older style - just one word. */
	e = strrchr(contig, '.');
	if (e == NULL)
	    errAbort("Expecting last dot line %d of %s\n", lf->lineIx, lf->fileName);
	*e++ = 0;
	AllocVar(end);
	subChar(s, '.', '_');
	end->contig = cloneString(contig);
	end->text = cloneString(e);
	c = lastChar(end->text);
	if (!(c == 'L' || c == 'R'))
	    c = '?';
	end->lr = c;
	slAddHead(&clone->spList, end);
	++spCount;
	}
    else if (wordCount == 15)
        {
	/* Newer style - 15 words. */
	if (!sameWord(words[11], "total_repeats"))
	    {
	    AllocVar(end);
	    end->contig = cloneString(contig);
	    end->text = cloneString(words[2]);
	    c = words[3][0];
	    if (!(c == 'L' || c == 'R'))
		c = '?';
	    end->lr = c;
	    slAddHead(&clone->spList, end);
	    ++spCount;
	    }
	}
    else
        {
	lineFileExpectWords(lf, 15, wordCount);
	}
    }
lineFileClose(&lf);
printf("Info on %d ends in %s\n", spCount, spFile);
}

void printCloneStart(struct clone *clone, FILE *f)
/* Print the start of the output line with the clone info. */
{
fprintf(f, "%s\t%s\tH%d\tG%d\tS%d\t", clone->name, clone->version, clone->phase,
	slCount(clone->gsList), slCount(clone->spList));
}

void oneSideOut(struct endInfo *end, struct clone *clone, FILE *f)
/* Print out info on a clone that has just one end. */
{
printCloneStart(clone, f);
fprintf(f, "%s\t%c\t\t\n", end->contig, end->lr);
}

void twoSidesOut(struct endInfo *endList, struct clone *clone, FILE *f)
/* Print out info on clone that has two ends. */
{
struct endInfo *a = endList;
struct endInfo *b = a->next;
printCloneStart(clone, f);
fprintf(f, "%s\t%c\t%s\t%c\n", a->contig, a->lr, b->contig, b->lr);
}

void printCloneEndInfo(struct clone *clone, FILE *f)
/* Print out end information on one clone. */
{
struct frag *frag, *endFrag;

if (clone->fragCount == 1)
    {
    frag = clone->fragList;
    printCloneStart(clone, f);
    fprintf(f, "%s\tL\t%s\tR\n", frag->name, frag->name);
    }
else if (clone->isFin || clone->phase >= 2)
    {
    frag = clone->fragList;
    endFrag = slLastEl(clone->fragList);
    printCloneStart(clone, f);
    fprintf(f, "%s\tL\t%s\tR\n", frag->name, endFrag->name);
    }
else 
    {
    int gsCount = slCount(clone->gsList);
    int spCount = slCount(clone->spList);
    if (gsCount == 1 && spCount <= 1 || spCount == 1 && gsCount <= 1)
	{
	if (gsCount == 1)
	    oneSideOut(clone->gsList, clone, f);
	else
	    oneSideOut(clone->spList, clone, f);
	}
    else if (gsCount == 2 || spCount == 2)
	{
	if (gsCount == 2)
	    twoSidesOut(clone->gsList, clone, f);
	else
	    twoSidesOut(clone->spList, clone, f);
	}
    else if (gsCount >= 3 || spCount >= 3)
	{
	warn("Three or more ends (%d %d) in %s, skipping for now",
		gsCount, spCount, clone->name);
	}
    }
}

void oneContig(char *dir, char *chrom, char *contig)
/* Write out clone ends information to one contig. */
{
char *wordsBuf, **faFiles, *faFile;
int i, faCount;
char path[512], sDir[256], sFile[128], sExt[64];
FILE *f;
struct clone *clone;

sprintf(path, "%s/geno.lst", dir);
readAllWords(path, &faFiles, &faCount, &wordsBuf);
sprintf(path, "%s/cloneEnds", dir);
f = mustOpen(path, "w");
fprintf(f, "#accession\tversion\tphase\tGS ends\tSP ends\taFrag\taSide\tzFrag\tzSide\n");

for (i=0; i<faCount; ++i)
    {
    faFile = faFiles[i];
    splitPath(faFile, sDir, sFile, sExt);
    if ((clone = hashFindVal(cloneHash, sFile)) != NULL)
        {
	printCloneEndInfo(clone, f);
	}
    }
freez(&wordsBuf);
fclose(f);
}


void ooCloneEnds(char *gsDir, char *spFile, char *ooDir)
/* ooCloneEnds - create cloneEnds files in each contig in ooDir. */
{
cloneHash = newHash(15);
readFinfFiles(gsDir);
addPhaseInfo(gsDir);
addBacEndInfo(spFile);
printf("Writing cloneEnds to contigs in %s\n", ooDir);

#ifdef SOMETIMES
    {
    struct clone *clone;
    uglyf("Zeroing out gs info\n");
    for (clone = cloneList; clone != NULL; clone = clone->next)
        {
	clone->gsList = NULL;
	}
    }
#endif /* SOMETIMES */

ooToAllContigs(ooDir, oneContig);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
ooCloneEnds(argv[1], argv[2], argv[3]);
return 0;
}
