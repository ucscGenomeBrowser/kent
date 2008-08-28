/* baseMaskToBed - write baseMask file as bed file */

#include <limits.h>
#include "common.h"
#include "genomeRangeTree.h"
#include "options.h"

static struct optionSpec optionSpecs[] = {
    {"id", OPTION_BOOLEAN},
    {"merge", OPTION_BOOLEAN},
    {NULL, 0}
};

static char *gChrom = NULL;
static int gChromCount = 0;
static FILE *gF = NULL;
static boolean gId = FALSE;

void writeMergedList(char *chrom, struct range *rList)
/* Write bed file from list of ranges, merging together adjacent nodes in list */
{
struct range *r;
if (!rList) 
    return;
/* remember the first item */
int start = rList->start, end = rList->end, i = 0;
/* start on the second item */
for (r = rList->next; r ; r = r->next)
    {
    if (end > r->start)
	errAbort("overlapping bed records in chrom %s (start,end=%d) (start=%d,end=%d)\n", chrom, end, r->start, r->end);
    if (end == r->start) /* keep the previous start and dont print */
	{
	end = r->end; 
	}
    else /* print the previous range and update */
	{
	if (gId)
	    fprintf(gF, "%s\t%d\t%d\t%s.%d\n", chrom, start, end, chrom, ++i);
	else
	    fprintf(gF, "%s\t%d\t%d\n", chrom, start, end);
	start = r->start;
	end = r->end;
	}
    }
/* print the last item */
if (gId)
    fprintf(gF, "%s\t%d\t%d\t%s.%d\n", chrom, start, end, chrom, ++i);
else
    fprintf(gF, "%s\t%d\t%d\n", chrom, start, end);
}

void writeBed(void *node)
/* Write bed file from one rangeTree node at a time. Applied to every node 
 * by tree traversal function. */
{
struct range *r = node;
if (gId)
    fprintf(gF, "%s\t%d\t%d\t%s.%d\n", gChrom, r->start, r->end, gChrom, ++gChromCount);
else
    fprintf(gF, "%s\t%d\t%d\n", gChrom, r->start, r->end);
}


void baseMaskToBed(char *inBama, char *bedFile, boolean merge)
/* read first 3 columns of bed and write as baseMask */
{
struct rbTree *rt;
struct genomeRangeTree *tree = genomeRangeTreeRead(inBama);
struct hashEl *chrom, *chromList = hashElListHash(tree->hash);
slSort(&chromList, hashElCmp); /* alpha sort on chrom */
gF = mustOpen(bedFile,"w");
/* loop over sorted chroms */
for (chrom = chromList ; chrom ; chrom = chrom->next)
    {
    rt = genomeRangeTreeFindRangeTree(tree, chrom->name);
    if (merge)
	{
	writeMergedList(chrom->name, rangeTreeList(rt));
	}
    else
	{
	gChrom = chrom->name;
	gChromCount = 0;
	rbTreeTraverse(rt, writeBed); /* write out all nodes in the tree */
	}
    }
hashElFreeList(&chromList);
genomeRangeTreeFree(&tree);
}


void usage(char *msg)
/* usage message and abort */
{
static char *usageMsg =
#include "baseMaskToBedUsage.msg"
    ;
errAbort("%s\n%s", msg, usageMsg);
}

/* entry */
int main(int argc, char** argv)
{
optionInit(&argc, argv, optionSpecs);
--argc;
++argv;
if (argc == 0)
    usage("");
if (argc != 2)
    usage("wrong # args");
boolean merge = optionExists("merge");
gId = optionExists("id");
char *inBama = argv[0];
char *outBed = argv[1];

baseMaskToBed(inBama, outBed, merge);
return 0;
}
