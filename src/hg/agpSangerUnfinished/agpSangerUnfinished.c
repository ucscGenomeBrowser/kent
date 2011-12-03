/* agpSangerUnfinished - Fixes chrom.agp or scaffold.agp 
 *  Galt Barber 2009-06-22
 *  Produces an agp file that matches the unfinished contig names 
 *  that are found in contigs.fa.
 *  Apparently Sanger wants to use these unfinished ones
 *  but they are not allowed to submit them to Genbank.
 *  So the agp matches the genbank record, but the fasta
 *  file does not.  
 *  The needed information is actually in the files,
 *  but it is stored in a completely non-standard way.
 *  So, we change the frag names in the agp to match
 *  the fasta, and we use the frag start and end 
 *  also in the matching.  The final agp frag start and end
 *  for these must be modified to be 1, size-of-frag
 *  This is being developed for Zv8 (and the same structure was in Zv7).
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
  "agpSangerUnfinished - Make agp file match the given contigs fasta file\n"
  "usage:\n"
  "   agpSangerUnfinished in.agp contigs.fa out.agp\n"
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


struct unfinishedContig
{
    char *frag;      /* unfinished contig frag name */
    int fragStart;   /* unfinished contig frag start 1-based */
    int fragEnd;     /* unfinished contig frag end */
};


struct hash *hashFasta(char *fileName)
/* Read names in fasta and put them in a hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(0);
verbose(1, "reading contig fasta file %s ...", fileName);
verbose(2, "\n");
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '>')
        {
        char *sp = strchr(line, ' ');
        if (sp)
	    {
	    ++sp;
	    char *under = strchr(line, '_');
	    if (under && (under < sp))
		{
		sp = strchr(sp, ' ');
		if (sp)
		    {
		    ++sp;
		    ++line;
		    struct unfinishedContig *u;
		    AllocVar(u);
		    u->frag = cloneString(nextWord(&line));
		    nextWord(&line);
		    char *key = nextWord(&line);
		    u->fragStart = atoi(nextWord(&line)) - 1; // -1 to adjust to our internal coordintes
		    u->fragEnd = atoi(nextWord(&line));
		    hashAdd(hash, key, u);
		    verbose(2, "key=[%s] %s %d %d\n", key, u->frag, u->fragStart, u->fragEnd);
		    }
		}
	    }
        }
    }
lineFileClose(&lf);
verbose(1, " done.\n");
return hash;
}


static void agpSangerUnfinished(char *agpFile, char *contigFasta, char *agpOut)
/* Fix agp to match unfinished contigs in fasta */
{
struct lineFile *lf = lineFileOpen(agpFile, TRUE);
char *line, *words[16];
int lineSize, wordCount;
unsigned lastPos = 0;
struct agpFrag *agp;
struct agpGap *gap;
FILE *f;
char *lastObj = NULL;
f = mustOpen(agpOut, "w");
char *newChrom = NULL;
struct hash *hash = hashFasta(contigFasta);

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

	char *root = cloneString(agp->frag);
	chopSuffixAt(root, '.');

	struct hashEl *e, *elist = hashLookup(hash, root);
	for (e = elist; e; e = hashLookupNext(e))
	    {
	    struct unfinishedContig *u = e->val;
            if ((u->fragStart <= agp->fragStart) && (u->fragEnd >= agp->fragEnd))
		{
		agp->frag = cloneString(u->frag);
		agp->fragEnd -= u->fragStart;
		agp->fragStart -= u->fragStart;
		}
	    }
	freeMem(root);
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
	agpFragOutput(agp, f, '\t', '\n');
	agpFragFree(&agp);
	}
    else
        {
	/* restore back to 1-based for agp 
	 * because agpGapOutput doesn't compensate */
	gap->chromStart += 1;
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


agpSangerUnfinished(argv[1], argv[2], argv[3]);

return 0;
}
