/* bedToClosestPoint - Given a bed file, and a list of unique points (often promoters) in genome, 
 * assign each bed to the closest point. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "genomeRangeTree.h"
#include "basicBed.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedToClosestPoint - Given a bed file, and a list of unique points (often promoters) in genome,\n"
  "assign each bed to the closest point.\n"
  "usage:\n"
  "   bedToClosestPoint inRanges.bed inPoints.bed out.tab\n"
  "Where:\n"
  "   inRanges.bed is a bed file without exons or strand\n"
  "   inPoints.bed is a bed file (promoters), where the chromStart on the + strand and\n"
  "       the chromEnd on the - strand will determine the point\n"
  "   out.tab has the following fields\n"
  "      <pointName> - the name field from inPoints.bed\n"
  "      <rangeName> - the name field from inRanges.bed, or '.' if the field is missing\n"
  "      <midDistance> - distance between point and middle of in.bed record. May be - if upstream\n"
  "      <rangeScore> - the score field if any from inRanges.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct range *rangeTreeClosestPoint(struct rbTree *t, int start, int end)
/* Find the closest range in tree to given range.  Closeness is measured by
 * closeness of the midpoint.   I'm not quite sure if this is general purpose
 * enough to go into the range tree library. */
{
struct rbTreeNode *p, *nextP = NULL;
struct rbTreeNode *closestNode = NULL;
int closestDistance = 0;
int mid = (start+end)/2;
    
/* Repeatedly explore either the left or right branch, depending on the
 * value of the key, keeping track of the closest point we've seen. */
for (p = t->root; p != NULL; p = nextP)
    {
    struct range *r = p->item;
    int rMid = (r->start + r->end)/2;
    int distance = mid - rMid;
    int absDistance;

    if (distance < 0)
        {
	nextP = p->left;
	absDistance = -distance;
        }
    else if (distance > 0)
        {
	nextP = p->right;
	absDistance = distance;
        }
    else
        {
	absDistance = 0;
        }
    if (closestNode == NULL || absDistance < closestDistance)
        {
	closestDistance = absDistance;
	closestNode = p;
	if (closestDistance == 0)
	   break;
	}
    }
return (closestNode == NULL ? NULL : closestNode->item); 
}

struct range *genomeRangeTreeClosest(struct genomeRangeTree *tree, char *chrom, int start, int end)
/* Return closest range in genome tree to given segment.  Returns NULL if chromosome is empty. */
{
struct rbTree *t;
return (t=genomeRangeTreeFindRangeTree(tree,chrom)) ? rangeTreeClosestPoint(t, start, end) : NULL;
}

void bedToClosestPoint(char *inRangesFile, char *inPointsFile, char *outFile)
/* bedToClosestPoint - Given a bed file, and a list of unique points (often promoters) in genome, 
 * assign each bed to the closest point. */
{
/* Load up inPoints file and put it into a range tree, insuring that each point is unique. */
struct bed *inPointsList = bedLoadNAll(inPointsFile, 6);
struct genomeRangeTree *pointTree = genomeRangeTreeNew();
struct bed *bed, *next;
for (bed = inPointsList; bed != NULL; bed = next)
    {
    next = bed->next;
    bed->next = NULL;
    int start;
    if (bed->strand[0] == '-')
	start = bed->chromEnd-1;
    else
	start = bed->chromStart;
    struct range *range = genomeRangeTreeAddValList(pointTree, bed->chrom, start, start+1, bed);
    if (slCount(range->val) > 1)
        errAbort("Multiple points at %s %d in %s", bed->chrom, start, inPointsFile);
    }

/* Go through ranges file. */
FILE *f = mustOpen(outFile, "w");
struct lineFile *lf = lineFileOpen(inRangesFile, TRUE);
char *row[6];
int wordCount;
while ((wordCount = lineFileChop(lf, row)) != 0)
    {
    /* Parse out bed fields. */
    lineFileExpectAtLeast(lf, 4, wordCount);
    char *chrom = row[0];
    unsigned chromStart = sqlUnsigned(row[1]);
    unsigned chromEnd = sqlUnsigned(row[2]);
    char *name = (wordCount > 3 ? row[3] : ".");
    int score = (wordCount > 4 ? sqlSigned(row[4]) : 0);

    struct range *range = genomeRangeTreeClosest(pointTree, chrom, chromStart, chromEnd);
    if (range != NULL)
	 {
	 struct bed *bed = range->val;
         fprintf(f, "%s\t", bed->name);
	 fprintf(f, "%s\t", name);
	 int midRange = (chromStart + chromEnd)/2;
	 int distance = midRange - range->start;
	 if (bed->strand[0] == '-')
	     distance = -distance;
	 fprintf(f, "%d\t", distance);
	 fprintf(f, "%d\n", score);
	 }
    }

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
bedToClosestPoint(argv[1], argv[2], argv[3]);
return 0;
}
