/* agpMergeChromScaf - Combines chrom.agp and scaffold.agp 
 *  Galt Barber 2009-06-19
 *  Produces a combined agp file from the chrom.agp and 
 *  scaffold.agp, merging only scaffolds from scaffold.agp
 *  that are not already in chrom.agp.
 *  This is being developed for Zv8 (and the same structure was in Zv7).
 *  The mapping of scaffold to chrom is enabled through
 *  the sanger hack of renaming all the contigs in the 
 *  agp and contig.fa to names like scaffold99.1, scaffold99.2
 *  which is the only way we know which scaffolds are
 *  involved in which chromosomes.
 *  One of the major goals here is to preserve the detailed
 *  gap structure of contigs even inside the chrom.
*/
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "fa.h"
#include "agpFrag.h"
#include "agpGap.h"
#include "options.h"


static void usage()
/* Explain usage and exit. */
{
errAbort(
  "agpMergeChromScaf - Create a merged .agp file with all chroms and all scaffolds not in chroms\n"
  "usage:\n"
  "   agpMergeChromScaf chrom.agp scaffold.agp out.agp\n"
  "\n"
  "options:\n"
  "   -verbose=N - N=2 display extra information\n"
  );
}

/* global vars */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};


static void agpMergeChromScaf(char *agpFile, char *agpOut, boolean filtering)
/* Create a combined agp file from the chrom.agp and scaffold.agp, 
 *  merging in only scaffolds from scaffold.agp
 *  that are not already in chroms. */
{
struct lineFile *lf = lineFileOpen(agpFile, TRUE);
char *line, *words[16];
int lineSize, wordCount;
unsigned lastPos = 0;
struct agpFrag *agp;
struct agpGap *gap;
FILE *f;
char *lastObj = NULL;
f = mustOpen(agpOut, filtering ? "a" : "w");
char *newChrom = NULL;
static struct hash *hash = NULL;
boolean skipping = FALSE;

if (!hash)
    hash = hashNew(0);

verbose(2,"#\tprocessing AGP file: %s\n", agpFile);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == 0 || line[0] == '#' || line[0] == '\n')
        continue;
    //verbose(2,"#\tline: %d\n", lf->lineIx);
    wordCount = chopLine(line, words);
    if (wordCount < 5)
        errAbort("Bad line %d of %s: need at least 5 words, got %d\n",
		 lf->lineIx, lf->fileName, wordCount);

    if (!lastObj || !sameString(words[0],lastObj))
	{
	freez(&newChrom);
	newChrom = cloneString(words[0]);
	lastPos = 0;
	}

    	
    skipping = FALSE;
    if (filtering)
	{
	if (hashLookup(hash, words[0]))
	    skipping = TRUE;
	}
		 
    if (words[4][0] != 'N')
	{
	lineFileExpectAtLeast(lf, 9, wordCount);
	agp = agpFragLoad(words);
	/* agp is 1-based but agp loaders do not adjust for 0-based: */
    	agp->chromStart -= 1;
	agp->fragStart  -= 1;
	if (agp->chromEnd - agp->chromStart != agp->fragEnd - agp->fragStart)
	    errAbort("Sizes don't match in %s and %s line %d of %s\n",
		agp->chrom, agp->frag, lf->lineIx, lf->fileName);
        if (!filtering)
	    {
	    char *root = cloneString(agp->frag);
	    chopSuffixAt(root, '.');
	    hashStore(hash, root);
	    freeMem(root);
	    }
	}
    else
        {
	lineFileExpectAtLeast(lf, 8, wordCount);
	gap = agpGapLoad(words);
	/* to be consistent with agpFrag */
	gap->chromStart -= 1;
	agp = (struct agpFrag*)gap;
	}

    if (agp->chromStart != lastPos)
	errAbort("Start doesn't match previous end line %d of %s\n"
	    "agp->chromStart: %u\n" 
	    "agp->chromEnd: %u\n" 
	    "lastPos: %u\n" 
	    ,lf->lineIx, lf->fileName
	    ,agp->chromStart
	    ,agp->chromEnd
	    ,lastPos
	    );

    lastPos = agp->chromEnd;
    freez(&lastObj);
    lastObj = cloneString(words[0]); /* not agp->chrom which may be modified already */
	
    if (words[4][0] != 'N')
	{
	/* agpFragOutput assumes 0-based-half-open, but writes 1-based for agp */
	if (!skipping)
    	    agpFragOutput(agp, f, '\t', '\n');
	agpFragFree(&agp);
	}
    else
        {
	/* restore back to 1-based for agp 
	 * because agpGapOutput doesn't compensate */
	gap->chromStart += 1;
	if (!skipping)
	    agpGapOutput(gap, f, '\t', '\n');
	agpGapFree(&gap);
	}
	
    }

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);

if (argc != 4)
    usage();
//if (optionExists("agpSeq"))
  //  agpSeq = optionVal("agpSeq", NULL);


agpMergeChromScaf(argv[1], argv[3], FALSE);
agpMergeChromScaf(argv[2], argv[3], TRUE);

return 0;
}
