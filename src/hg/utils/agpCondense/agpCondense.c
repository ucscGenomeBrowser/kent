/* Copyright (C) 2005 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* agpCondense - Get rid of extra lines in the AGP file that don't add any info.
 * For example:
 * 
 * Group13 1697787 1702288 114     W       Contig4781      1       4502    +
 * Group13 1702289 1702752 115     N       464     fragment        yes
 * Group13 1702753 1703708 116     W       Contig4782      1       956     +
 * Group13 1703709 1715808 117     W       Contig4782      957     13056   +
 * Group13 1715809 1715858 118     N       50      fragment        yes
 * 
 * is changed to:
 *
 * Group13 1697787 1702288 114     W       Contig4781      1       4502    +
 * Group13 1702289 1702752 115     N       464     fragment        yes
 * Group13 1702753 1715808 116     W       Contig4782      1       13056     +
 * Group13 1715809 1715858 117     N       50      fragment        yes
 */

#include "common.h"
#include "hash.h"
#include "agp.h"
#include "agpFrag.h"
#include "agpGap.h"

#define connected(x,y) (abs(x - y) == 1)
/* Basic calculation. */

FILE *output = NULL;
/* The output file. */

static void usage()
/* Explain usage and exit. */
{
errAbort(
  "agp - Create a modified .agp file\n"
  "usage:\n"
  "   agpToFa in.agp out.agp");
}

void printList(struct agp *list)
{
/* Print out the AGP. */
struct agp *curr;
for (curr = list; curr != NULL; curr = curr->next)
    if (curr->isFrag)
	{
	agpFragTabOut(curr->entry, output);
	}
    else
	agpGapTabOut(curr->entry, output);
}

boolean lastWasAFrag(struct agp *prev, struct agp *curr)
{
/* Was the last record a fragment adjacent to the current fragment? */
if (!curr || !prev)
    return FALSE;
if (curr->isFrag && prev->isFrag) 
    {
    struct agpFrag *prevFrag = (struct agpFrag *)prev->entry;
    struct agpFrag *currFrag = (struct agpFrag *)curr->entry;
    return (sameString(currFrag->frag, prevFrag->frag) &&
	    connected(currFrag->chromStart, prevFrag->chromEnd) &&
	    (connected(currFrag->fragStart, prevFrag->fragEnd) || 
	     connected(currFrag->fragEnd, prevFrag->fragStart)));
    }
return FALSE;
}

void condenseRecord(struct agpFrag *prev, struct agpFrag *curr)
/* Change the previous record based on the current one. */
{
if (curr->fragStart < prev->fragEnd)
    prev->fragStart = curr->fragStart;
else
    prev->fragEnd = curr->fragEnd;
prev->chromEnd = curr->chromEnd;
}

void condenseOneList(struct agp *agpList)
/* Condense a single list of agps for a chrom. */
{
struct agp *agp, *prev = NULL;
int i;

for (agp = agpList; agp != NULL; prev = agp, agp = agp->next)
    {
    if (agp->isFrag)
	{
	struct agpFrag *frag = agp->entry;
	if (lastWasAFrag(prev, agp))
	    {
	    condenseRecord(prev->entry, agp->entry);
	    prev->next = agp->next;
	    agp->next = NULL;
	    agpFree(&agp);
	    agp = prev;
	    }
	}
    }
/* Go through the list again and fix things up. */
for (agp = agpList, i = 1; agp != NULL; agp = agp->next, i++)
    {
    if (agp->isFrag)
	{
	struct agpFrag *frag = agp->entry;
	frag->fragStart--;
	frag->chromStart--;
	frag->ix = i;
	}
    else 
	((struct agpGap *)agp->entry)->ix = i;
    }
}

void condenseAll(struct hash *agpLists, char *outFileName)
{
/* Go through the hash of agp lists and condense each one. */
struct hashEl *lists = hashElListHash(agpLists), *oneList;
output = mustOpen(outFileName, "w");
for (oneList = lists; oneList != NULL; oneList = oneList->next)
    {
    struct agp *list = oneList->val;
    condenseOneList(list);
    printList(list);
    oneList->val = NULL;
    }
hashElFreeList(&lists);
carefulClose(&output);
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct hash *agpLists = NULL;
if (argc != 3)
    usage();
agpLists = agpLoadAll(argv[1]);
condenseAll(agpLists, argv[2]);
agpHashFree(&agpLists);
return 0;
}
