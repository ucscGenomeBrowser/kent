/* liftUpBedFromTable - Convert coordinate systems using a positional table in the database.. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "liftUpBedFromTable - Convert coordinate systems using a positional table in the database.\n"
  "usage:\n"
  "   liftUpBedFromTable db table in.bed out.bed\n"
  "options:\n"
  "   -plus=N   File has first N fields which are bed, plus more after that.\n"
  "   -cds      Lift off positional table's CDS start (or end if using -end).\n"
  "   -end      Lift off the end of the position instead of the beginning.\n"
  "WARNING: This utility isn't yet mature.  in.bed should be BED 6 or less, and\n"
  "the output could be off so double-check stuff.\n"
  );
}

static struct optionSpec options[] = {
   {"plus", OPTION_INT},
   {"cds", OPTION_BOOLEAN},
   {"end", OPTION_BOOLEAN},
   {NULL, 0},
};

/* Option-related globals. */

static int numBedFields = 0;
static boolean usingCds = FALSE;
static boolean usingEnd = FALSE;

struct bedPlus 
/* Store bed plus the rest of the line. */
    {
    struct bedPlus *next;
    struct bed *bed;
    char *rest;
    }; 

void bedPlusFree(struct bedPlus **pBp)
/* freedom. */
{
struct bedPlus *bp;
if ((bp = *pBp) == NULL)
    return;
bedFree((struct bed **)&(bp->bed));
freeMem((char *)bp->rest);
freez(pBp);
}

void bedPlusFreeList(struct bedPlus **pList)
/* typical freeList function. */
{
struct bedPlus *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    bedPlusFree(&el);
    }
*pList = NULL;
}

struct bedPlus *bedPlusNLoad(char *row[], int wordCount, int bedWordCount)
/* Load a single row array like bedLoadN would... but handle extra info. */
{
struct bedPlus *newBp;
AllocVar(newBp);
if (bedWordCount > 0)
    {
    struct dyString *ds = newDyString(0);
    int i;
    newBp->bed = bedLoadN(row, bedWordCount);
    for (i = bedWordCount; i < wordCount; i++)
	{
	dyStringAppend(ds, row[i]);
	if (i < (wordCount-1))
	    dyStringAppendC(ds, '\t');
	}
    newBp->rest = dyStringCannibalize(&ds);
    }
else
    {
    newBp->bed = bedLoadN(row, wordCount);
    }
return newBp;
}

struct bedPlus *bedPlusFileLoad(char *fileName)
/* Load all the beds from a file using either the */
/* N number of fields, or whatever we find. */
{
struct bedPlus *list = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[128];
int wordCount;
int bedWordCount = optionInt("plus", 0);
numBedFields = bedWordCount;
while ((wordCount = lineFileChopTab(lf, words)) > 0)
    {
    struct bedPlus *newBp;
    if (numBedFields == 0)
	numBedFields = wordCount;
    if (numBedFields < 4)
	errAbort("Too few bed fields.");
    newBp = bedPlusNLoad(words, wordCount, bedWordCount);
    slAddHead(&list, newBp);
    }
slReverse(&list);
lineFileClose(&lf);
return list;
}

void bedPlusTabOut(FILE *f, struct bedPlus *bp)
/* Output and handle plus info. */
{
if (bp->rest == NULL)
    bedOutputN(bp->bed, numBedFields, f, '\t', '\n');
else
    {
    bedOutputN(bp->bed, numBedFields, f, '\t', '\t');
    fprintf(f, "%s\n", bp->rest);
    }
}

struct hash *hashUpPositions(struct bed **pBedList)
/* Add beds to a hash of lists. */
{
struct bed *popped;
struct hash *theHash = hashNew(22);
while ((popped = slPopHead(pBedList)) != NULL)
    {
    struct slRef *ref = slRefNew(popped);
    struct slRef *list = (struct slRef *)hashFindVal(theHash, popped->name);
    ref->next = list;
    hashReplace(theHash, popped->name, ref);
    }
return theHash;
}

void slRefAndBedFree(struct slRef **pRef)
/* designed to be called by slRefListAndBedFree. */
{
struct slRef *ref;
if ((ref = *pRef) == NULL) 
    return;
bedFree((struct bed **)&(ref->val));
freez(pRef);
}

void slRefListAndBedFree(void **pList)
/* designed to be called by bedFreeWithVals. */
{
struct slRef *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    slRefAndBedFree(&el);
    }
*pList = NULL;
}

struct hash *positionHash(char *db, char *table)
/* Load up table from database and return a hash from it. */
{
struct bed *list = hGetFullBedDb(db, table);
struct hash *theHash = hashUpPositions(&list);
return theHash;
}

void liftAndOutput(struct hash *posHash, struct bedPlus *bpList, char *outfile)
/* Combine the two sources of data, do the coordinate math, and output. */
{
FILE *f = mustOpen(outfile, "w");
struct bedPlus *bp;
for (bp = bpList; bp != NULL; bp = bp->next)
    {
    struct slRef *refList = hashFindVal(posHash, bp->bed->chrom);
    /* Skips lifting ones that don't have chroms with names among those in */
    /* the table. */
    if (refList)
	{
	struct slRef *ref;
	for (ref = refList; ref != NULL; ref = ref->next)
	    {
	    struct bedPlus *liftedBp;
	    struct bed *liftedBed;
	    struct bed *pos = ref->val;
	    char newName[64];
	    int size = bp->bed->chromEnd - bp->bed->chromStart;
	    boolean minusStrand = FALSE;
	    if (((pos->strand[0] == '+') && (bp->bed->strand[0] == '-')) || 
		((pos->strand[0] == '-') && (bp->bed->strand[0] == '+')))
		minusStrand = TRUE;
	    safef(newName, sizeof(newName), "%s.%s", pos->name, bp->bed->name);	   
	    AllocVar(liftedBp);
	    AllocVar(liftedBed);
	    liftedBed->name = cloneString(newName);
	    liftedBed->chrom = cloneString(pos->chrom);
	    liftedBed->score = bp->bed->score;
	    liftedBed->strand[0] = (minusStrand) ? '-' : '+';
	    liftedBed->strand[1] = '\0';
	    /* coordinate hell... obnoxious. */
	    if (usingEnd)
		{
		if (usingCds)
		    {
		    if (minusStrand)
			{
			liftedBed->chromEnd = pos->thickStart - bp->bed->chromStart - 1;
			liftedBed->chromStart = liftedBed->chromEnd - size;
			}
		    else 
			{
			liftedBed->chromStart = pos->thickEnd + bp->bed->chromStart;
			liftedBed->chromEnd = liftedBed->chromStart + size;
			}
		    }		
		else
		    {
		    if (minusStrand)
			{
			liftedBed->chromEnd = pos->chromStart - bp->bed->chromStart - 1;
			liftedBed->chromStart = liftedBed->chromEnd - size;
			}
		    else 
			{
			liftedBed->chromStart = pos->chromEnd + bp->bed->chromStart;
			liftedBed->chromEnd = liftedBed->chromStart + size;
			}
		    }
		}
	    else
		{
		if (usingCds)
		    {
		    if (minusStrand)
			{
			liftedBed->chromEnd = pos->thickEnd - bp->bed->chromStart - 1;
			liftedBed->chromStart = liftedBed->chromEnd - size;
			}
		    else 
			{
			liftedBed->chromStart = pos->thickStart + bp->bed->chromStart;
			liftedBed->chromEnd = liftedBed->chromStart + size;
			}
		    }
		else
		    {
		    if (minusStrand)
			{
			liftedBed->chromEnd = pos->chromEnd - bp->bed->chromStart - 1;
			liftedBed->chromStart = liftedBed->chromEnd - size;
			}
		    else 
			{
			liftedBed->chromStart = pos->chromStart + bp->bed->chromStart;
			liftedBed->chromEnd = liftedBed->chromStart + size;
			}
		    }
		}
	    liftedBp->bed = liftedBed;
	    liftedBp->rest = cloneString(bp->rest);
	    /* Output! */
	    bedPlusTabOut(f, liftedBp);
	    bedPlusFree(&liftedBp);
	    }
	}
    else
	fprintf(stderr, "warning: %s not found in table\n", bp->bed->chrom);
    }
carefulClose(&f);
}

void liftUpBedFromTable(char *db, char *table, char *inFile, char *outFile)
/* liftUpBedFromTable - Convert coordinate systems using a positional table in the database.. */
{
struct hash *posHash = positionHash(db, table);
struct bedPlus *bpList = bedPlusFileLoad(inFile);
if (optionExists("cds"))
    usingCds = TRUE;
if (optionExists("end"))
    usingEnd = TRUE;
liftAndOutput(posHash, bpList, outFile);
bedPlusFreeList(&bpList);
hashFreeWithVals(&posHash, slRefListAndBedFree);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
liftUpBedFromTable(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
