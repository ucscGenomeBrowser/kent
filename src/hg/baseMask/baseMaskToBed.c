/* baseMaskToBed - write baseMask file as bed file */

#include "common.h"
#include "genomeRangeTree.h"
#include "options.h"

static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};


static char *tmp_chrom;
static FILE *tmp_f;

void writeBed(void *item)
{
struct range *r = item;
fprintf(tmp_f, "%s\t%d\t%d\n", tmp_chrom, r->start, r->end);
}


void baseMaskToBed(char *inBama, char *bedFile)
/* read first 3 columns of bed and write as baseMask */
{
struct rbTree *rt;
struct genomeRangeTree *tree = genomeRangeTreeRead(inBama);
struct hashEl *chrom, *chromList = hashElListHash(tree->hash);
slSort(&chromList, hashElCmp); /* alpha sort on chrom */
tmp_f = mustOpen(bedFile,"w");
/* loop over sorted chroms */
for (chrom = chromList ; chrom ; chrom = chrom->next)
    {
    tmp_chrom = chrom->name;
    rt = genomeRangeTreeFindRangeTree(tree, tmp_chrom);
    /* write out all nodes in the tree */
    rbTreeTraverse(rt, writeBed);
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
errAbort("%s:  %s", msg, usageMsg);
}

/* entry */
int main(int argc, char** argv)
{
char *inBama, *outBed;
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage("wrong # args");
inBama = argv[1];
outBed = argv[2];

baseMaskToBed(inBama, outBed);
return 0;
}
