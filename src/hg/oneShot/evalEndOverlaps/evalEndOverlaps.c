/* evalEndOverlaps - Look at end overlap info and see how much seems reasonable.. */
#include "common.h"
#include "linefile.h"
#include "portable.h"
#include "hash.h"
#include "ooUtils.h"
#include "hCommon.h"
#include "ocp.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "evalEndOverlaps - Look at end overlap info and see how much seems reasonable.\n"
  "usage:\n"
  "   evalEndOverlaps ooDir\n");
}

struct clone
/* Information on one clone. */
    {
    struct clone *next;	/* Next in list. */
    char *name;		/* Clone name (accession, no version). Allocated in hash */
    char *version;	/* Version part of name. */
    int size;           /* Clone size. */
    int phase;		/* HTG phase.  3 - finished, 2 - ordered, 1 - draft, 0 - survey. */
    char *firstFrag;	/* Name of first fragment in fa file. */
    char *lastFrag;	/* Name of last fragment in fa file. */
    char *faFileName;   /* File with clone sequence. */
    char *startFrag;	/* Known start fragment name or NULL if none. */
    char startSide;	/* L or R depending which side of frag this is on. */
    char *endFrag;	/* Known end frag name or NULL if none. */
    char endSide;	/* L or R depending which side of frag this is on. */
    };

void cloneFree(struct clone **pClone)
/* Free a clone. */
{
struct clone *clone = *pClone;
if (clone != NULL)
    {
    freeMem(clone->version);
    freeMem(clone->firstFrag);
    freeMem(clone->lastFrag);
    freeMem(clone->faFileName);
    freeMem(clone->startFrag);
    freeMem(clone->endFrag);
    freez(pClone);
    }
}

void cloneFreeList(struct clone **pList)
/* Free a list of dynamically allocated clones */
{
struct clone *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    cloneFree(&el);
    }
*pList = NULL;
}


boolean loadCloneInfo(char *fileName,
	struct clone **retList, struct hash **retHash)
/* Load list of clones from fileName.  Return FALSE if can't
 * because file doesn't exist or is zero length. */
{
struct lineFile *lf = lineFileMayOpen(fileName, TRUE);
struct clone *cloneList = NULL, *clone;
struct hash *hash = NULL;
char *row[7];

if (lf == NULL)
    return FALSE;
hash = newHash(10);
while (lineFileRow(lf, row))
    {
    AllocVar(clone);
    slAddHead(&cloneList, clone);
    hashAddSaveName(hash, row[0], clone, &clone->name);
    clone->version = cloneString(row[1]);
    clone->size = lineFileNeedNum(lf, row, 2);
    clone->phase = lineFileNeedNum(lf, row, 3);
    clone->firstFrag = cloneString(row[4]);
    clone->lastFrag = cloneString(row[5]);
    clone->faFileName = cloneString(row[6]);
    }
lineFileClose(&lf);
slReverse(&cloneList);
*retHash = hash;
*retList = cloneList;
return TRUE;
}

char getLR(struct lineFile *lf, char *words[], int wordIx)
/* Make sure that words[wordIx] is L or R, and return it. */
{
char c = words[wordIx][0];
if (c == 'R' || c == 'L' || c == '?')
    return c;
else
    {
    errAbort("Expecting L or R in field %d line %d of %s",
       wordIx+1, lf->lineIx, lf->fileName);
    return '?';
    }
}


void loadCloneEndInfo(char *fileName, struct hash *cloneHash)
/* Load up cloneEnds file into list/hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct clone *clone;
char *words[16], *line;
int wordCount;
char fragName[128];
int singles = 0, doubles = 0;


while ((wordCount = lineFileChop(lf, words)) > 0)
    {
    if (wordCount != 7 && wordCount != 9)
        errAbort("Expecting 7 or 9 words line %d of %s", lf->lineIx, lf->fileName);
    clone = hashMustFindVal(cloneHash, words[0]);
    if (!sameString(clone->version, words[1]))
        errAbort("Version mismatch %s.%s and %s.s", clone->name, clone->version, clone->name, words[1]);
    sprintf(fragName, "%s%s_%s", clone->name, clone->version, words[5]);
    clone->startFrag = cloneString(fragName);
    clone->startSide = getLR(lf, words, 6);
    if (wordCount == 9)
        {
	sprintf(fragName, "%s%s_%s", clone->name, clone->version, words[7]);
	clone->endFrag = cloneString(fragName);
	clone->endSide = getLR(lf, words, 8);
	++doubles;
	}
    else
        {
	++singles;
	}
    }
lineFileClose(&lf);
}

struct overlapStats
/* Stats on a family of overlaps. */
    {
    int overlaps;	/* Total number of overlaps in this family. */
    int allKnown;       /* Number where all ends are known. */
    int anyKnown;       /* Total number where at least one end known. */
    int niceOverlap;	/* YN/YN type overlaps. */
    int niceEnclosed;   /* YY/NN with sizes ok. */
    int badEnclosed;    /* YY/NN with sizes bad. */
    int allN;		/* NN/NN. */
    int identical;      /* YY/YY same size, full overlap. */
    int nearIdentical;  /* YY/YY nearly same size, nearly full overlap. */
    int singleY;	/* NN/YN or a variant. */
    int badOther;       /* Others. */
    };

struct overlapStats byPhase[4][4];	/* Overlaps between various phases. */
struct overlapStats all;		/* All overlaps. */

void categorizeOcp(struct ocp *ocp, struct clone *a, struct clone *b, struct overlapStats *stats)
/* Add info on ocp to stats. */
{
boolean bothFin = (stats == &byPhase[3][3]);
boolean inAll = (stats == &all);

stats->overlaps += 1;
if (ocp->aHitS != '?' || ocp->aHitT != '?' || ocp->bHitS != '?' || ocp->bHitT != '?')
    stats->anyKnown += 1;

if (ocp->aHitS == '?' || ocp->aHitT == '?' || ocp->bHitS == '?' || ocp->bHitT == '?')
    {
    if (bothFin)
	{
        printf("odd - finished/finished overlap with unknown ends\n");
	ocpTabOut(ocp, stdout);
	}
    return;
    }

stats->allKnown += 1;
if (ocp->aHitS == 'N' && ocp->aHitT == 'N' && ocp->bHitS == 'N' && ocp->bHitT == 'N')
    {
    stats->allN += 1;
    if (bothFin)
	{
        printf("odd - NN/NN finished/finished overlap\n");
	ocpTabOut(ocp, stdout);
	}
    }
else if (ocp->aHitS == 'Y' && ocp->aHitT == 'N' && ocp->bHitS == 'Y' && ocp->bHitT == 'N')
    stats->niceOverlap += 1;
else if (ocp->aHitS == 'Y' && ocp->aHitT == 'N' && ocp->bHitS == 'N' && ocp->bHitT == 'Y')
    stats->niceOverlap += 1;
else if (ocp->aHitS == 'N' && ocp->aHitT == 'Y' && ocp->bHitS == 'Y' && ocp->bHitT == 'N')
    stats->niceOverlap += 1;
else if (ocp->aHitS == 'N' && ocp->aHitT == 'Y' && ocp->bHitS == 'N' && ocp->bHitT == 'Y')
    stats->niceOverlap += 1;
else if (ocp->aHitS == 'Y' && ocp->aHitT == 'Y' && ocp->bHitS == 'N' && ocp->bHitT == 'N')
    {
    if (a->size < b->size)
        stats->niceEnclosed += 1;
    else
        stats->badEnclosed += 1;
    }
else if (ocp->aHitS == 'N' && ocp->aHitT == 'N' && ocp->bHitS == 'Y' && ocp->bHitT == 'Y')
    {
    if (a->size > b->size)
        stats->niceEnclosed += 1;
    else
        stats->badEnclosed += 1;
    }
else if (ocp->aHitS == 'Y' && ocp->aHitT == 'Y' && ocp->bHitS == 'Y' && ocp->bHitT == 'Y')
    {
    if (ocp->aSize == ocp->bSize && ocp->aSize == ocp->overlap)
	{
	stats->identical += 1;
	if (inAll)
	    {
	    printf("odd - identical clones!\n");
	    ocpTabOut(ocp, stdout);
	    }
	}
    else if (intAbs(ocp->aSize - ocp->bSize) < 1.1*ocp->aSize && ocp->overlap*1.1 >= min(ocp->aSize, ocp->bSize))
        {
	stats->nearIdentical += 1;
	if (inAll)
	    {
	    printf("odd - near identical clones!\n");
	    ocpTabOut(ocp, stdout);
	    }
	}
    else
        goto BADOTHER;
    }
else if (ocp->aHitS == 'Y' && ocp->aHitT == 'Y' && ocp->bHitS == 'N' && ocp->bHitT == 'Y' 
	&& ocp->overlap*1.1+1000 >= ocp->aSize && ocp->aSize < ocp->bSize)
    {
    stats->niceEnclosed += 1;
    }
else if (ocp->aHitS == 'Y' && ocp->aHitT == 'Y' && ocp->bHitS == 'Y' && ocp->bHitT == 'N' 
	&& ocp->overlap*1.1+1000 >= ocp->aSize && ocp->aSize < ocp->bSize)
    {
    stats->niceEnclosed += 1;
    }
else if (ocp->aHitS == 'Y' && ocp->aHitT == 'N' && ocp->bHitS == 'Y' && ocp->bHitT == 'Y' 
	&& ocp->overlap*1.1+1000 >= ocp->bSize && ocp->bSize < ocp->aSize)
    {
    stats->niceEnclosed += 1;
    }
else if (ocp->aHitS == 'N' && ocp->aHitT == 'Y' && ocp->bHitS == 'Y' && ocp->bHitT == 'Y' 
	&& ocp->overlap*1.1+1000 >= ocp->bSize && ocp->bSize < ocp->aSize)
    {
    stats->niceEnclosed += 1;
    }
else if (ocp->aHitS == 'Y' && ocp->aHitT == 'N' && ocp->bHitS == 'N' && ocp->bHitT == 'N')
    {
    stats->singleY += 1;
    if (bothFin)
	{
	printf("oddFin - single Y!\n");
	ocpTabOut(ocp, stdout);
	}
    else
        {
	printf("oddDraft - single Y!\n");
	ocpTabOut(ocp, stdout);
	}
    }
else if (ocp->aHitS == 'N' && ocp->aHitT == 'Y' && ocp->bHitS == 'N' && ocp->bHitT == 'N')
    {
    stats->singleY += 1;
    if (bothFin)
	{
	printf("oddFin - single Y!\n");
	ocpTabOut(ocp, stdout);
	}
    else
        {
	printf("oddDraft - single Y!\n");
	ocpTabOut(ocp, stdout);
	}
    }
else if (ocp->aHitS == 'N' && ocp->aHitT == 'N' && ocp->bHitS == 'Y' && ocp->bHitT == 'N')
    {
    stats->singleY += 1;
    if (bothFin)
	{
	printf("oddFin - single Y!\n");
	ocpTabOut(ocp, stdout);
	}
    else
        {
	printf("oddDraft - single Y!\n");
	ocpTabOut(ocp, stdout);
	}
    }
else if (ocp->aHitS == 'N' && ocp->aHitT == 'N' && ocp->bHitS == 'N' && ocp->bHitT == 'Y')
    {
    stats->singleY += 1;
    if (bothFin)
	{
	printf("oddFin - single Y!\n");
	ocpTabOut(ocp, stdout);
	}
    else
        {
	printf("oddDraft - single Y!\n");
	ocpTabOut(ocp, stdout);
	}
    }
else
    {
BADOTHER:
    if (inAll)
	{
        printf("odd%s - badOther \n", ((a->phase == 3 && b->phase == 3) ? "Fin" : "Draft"));
	ocpTabOut(ocp, stdout);
	}
    stats->badOther += 1;
    }
}

void gatherStats(struct ocp *ocpList, struct hash *cloneHash)
/* Gather stats on all ocp's in list. */
{
struct ocp *ocp;
struct clone *a, *b;
for (ocp = ocpList; ocp != NULL; ocp = ocp->next)
    {
    a = hashMustFindVal(cloneHash, ocp->aName);
    b = hashMustFindVal(cloneHash, ocp->bName);
    categorizeOcp(ocp, a, b, &all);
    categorizeOcp(ocp, a, b, &byPhase[a->phase][b->phase]);
    }
}

double percent(double p, double q)
/* Return p/q as a percent. */
{
return 100.0 * p / q;
}

double infoLine(FILE *f, char *title, int p, int q)
/* Print line of info to file. */
{
fprintf(f, "%s %d (%4.2f%%)\n", title, p, percent(p,q));
}

void printStats(struct overlapStats *stats, FILE *f)
/* Print stats to file. */
{
fprintf(f,  "Total overlaps:  %d\n", stats->overlaps);
infoLine(f, "Any ends known: ", stats->anyKnown, stats->overlaps);
infoLine(f, "Both ends known:", stats->allKnown, stats->overlaps);
infoLine(f, "Nice end to end:", stats->niceOverlap, stats->allKnown);
infoLine(f, "Nicely enclosed:", stats->niceEnclosed, stats->allKnown);
infoLine(f, "Identical clone:", stats->identical, stats->allKnown);
infoLine(f, "Near identical: ", stats->nearIdentical, stats->allKnown);
infoLine(f, "No ends overlap:", stats->allN, stats->allKnown);
infoLine(f, "Badly enclosed: ", stats->badEnclosed, stats->allKnown);
infoLine(f, "Single end hits:", stats->singleY, stats->allKnown);
infoLine(f, "Other bad:      ", stats->badOther, stats->allKnown);
}

void doContig(char *contigDir, char *chrom, char *contig)
/* Eval ends on one contig. */
{
struct clone *cloneList = NULL;
struct hash *cloneHash = NULL;
struct ocp *ocpList = NULL;
char fileName[512];

sprintf(fileName, "%s/cloneInfo", contigDir);
if (!loadCloneInfo(fileName, &cloneList, &cloneHash))
    return;
printf("Processing %d clones in %s\n", slCount(cloneList), fileName);

sprintf(fileName, "%s/cloneEnds", contigDir);
loadCloneEndInfo(fileName, cloneHash);

sprintf(fileName, "%s/cloneOverlap", contigDir);
if (fileExists(fileName))
    {
    ocpList = ocpLoadFile(fileName);
    gatherStats(ocpList, cloneHash);
    }

/* Clean up time. */
cloneFreeList(&cloneList);
freeHash(&cloneHash);
ocpFreeList(&ocpList);
}

void evalEndOverlaps(char *ooDir)
/* evalEndOverlaps - Look at end overlap info and see how much seems reasonable.. */
{
ooToAllContigs(ooDir, doContig);
printf("Info on all overlaps\n");
printStats(&all, stdout);
printf("\n");
printf("Info on finished/finished overlaps\n");
printStats(&byPhase[3][3], stdout);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
evalEndOverlaps(argv[1]);
return 0;
}
