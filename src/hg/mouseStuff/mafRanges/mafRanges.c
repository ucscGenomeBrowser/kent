/* mafRanges - Extract ranges of target (or query) coverage from maf and 
 * output as BED 3 (intended for further processing by featureBits). */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "maf.h"
#include "string.h"

/* Command line switches */

/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
"mafRanges - Extract ranges of target (or query) coverage from maf and \n"
"            output as BED 3 (e.g. for processing by featureBits).\n"
"usage:\n"
"   mafRanges in.maf db out.bed\n"
"            db should appear in in.maf alignments as the first part of \n"
"            \"db.seqName\"-style sequence names.  The seqName part will \n"
"            be used as the chrom field in the range printed to out.bed.\n"
/*
"options:\n"
"   -blah    blah\n"
*/
);
}


void mafRanges(char *in, char *db, char *out)
/* mafRanges - Extract ranges of target (or query) coverage from maf and 
 * output as BED 3 (intended for further processing by featureBits). */
{
struct mafFile *mf = mafOpen(in);
FILE *f = mustOpen(out, "w");
struct mafAli *ma = NULL;
char *dbDot = (char *)cloneMem(db, strlen(db)+2);
int dbDotLen = 0;

strcat(dbDot, ".");
dbDotLen = strlen(dbDot);
while ((ma = mafNext(mf)) != NULL)
    {
    struct mafComp *mc = NULL;
    for (mc = ma->components;  mc != NULL;  mc = mc->next)
	{
	if (mc->src != NULL && startsWith(dbDot, mc->src))
	    {
	    int start = mc->start;
	    int end = start + mc->size;
	    if (mc->strand == '-')
		reverseIntRange(&start, &end, mc->srcSize);
	    fprintf(f, "%s\t%d\t%d\n", mc->src+dbDotLen, start, end);
	    }
	}
    mafAliFree(&ma);
    }

mafFileFree(&mf);
fclose(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage();
mafRanges(argv[1], argv[2], argv[3]);
return 0;
}
