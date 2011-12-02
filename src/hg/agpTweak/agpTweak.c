/* agpToFa - Tweak an .agp file. 
 *  Galt Barber 2005-08-20
 *  This is handy for reducing the size of an agp element,
 *  i.e. chrUn.  You can squeeze some wasted space by changing
 *  the size of the clone gap padding, or simply split the chrom
 *  creating chrUn, chrUn2, etc.  You can also split chrUn into 
 *  scaffolds.
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
  "agpTweak - Create a modified .agp file\n"
  "usage:\n"
  "   agpToFa in.agp out.agp\n"
  "\n"
  "options:\n"
  "   -splitIntoScaffolds - will break obj into scaffolds at 'clone no' boundaries\n"
  "   -splitSize=N - will keep the agp object at about size N in agp file,\n"
  "                split names will be obj.1, obj.2, etc.\n"
  "                and the split point is chosen to start a next object on a non-gap;\n"
  "                note that you can use splitIntoScaffolds or splitSize but not both\n"
  "   -cloneGapSize=N - force clone gaps to size N in agp file\n"
  "   -agpSeq=xxxx limits changes to where agp object=xxxx matches a sequence name in in.agp\n"
  "   -verbose=N - N=2 display extra information\n"
  );
}

char *agpSeq = NULL;
int cloneGapSize = 0;
int splitSize= 0;
boolean splitIntoScaffolds = FALSE;

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"splitIntoScaffolds", OPTION_BOOLEAN},
    {"splitSize", OPTION_INT},
    {"cloneGapSize", OPTION_INT},
    {"agpSeq", OPTION_STRING},
    {NULL, 0}
};


static void agpTweak(char *agpFile, char *agpOut)
/* agpToFa - Convert a .agp file to a .fa file. */
{
struct lineFile *lf = lineFileOpen(agpFile, TRUE);
char *line, *words[16];
int lineSize, wordCount;
unsigned lastPos = 0;
struct agpFrag *agp;
struct agpGap *gap;
FILE *f;
int accumDif = 0;
char *lastObj = NULL;
f = mustOpen(agpOut, "w");
int suffixCount = 1;
int lineNo = 1;
char buf[256];
char *newChrom = NULL;
boolean suppressGap = FALSE;

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

    suppressGap = FALSE;
    if (!lastObj || !sameString(words[0],lastObj))
	{
	freez(&newChrom);
	if (splitIntoScaffolds)
	    newChrom = cloneString("scaffold1");
	else
	    newChrom = cloneString(words[0]);
	}
	
    if (lastObj && !sameString(words[0],lastObj))
	{
	accumDif = 0;
	lastPos = 0;
	suffixCount = 1;
	lineNo = 1;
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
	}
    else
        {
	lineFileExpectAtLeast(lf, 8, wordCount);
	gap = agpGapLoad(words);
	/* to be consistent with agpFrag */
	gap->chromStart -= 1;
	agp = (struct agp*)gap;
	}

    /* adjust for accumulated difference if any */
    agp->chromStart += accumDif;
    agp->chromEnd += accumDif;
    
    if (agp->chromStart != lastPos)
	errAbort("Start doesn't match previous end line %d of %s\n"
	    "agp->chromStart: %u\n" 
	    "agp->chromEnd: %u\n" 
	    "lastPos: %u\n" 
	    "accumDif: %d\n"
	    ,lf->lineIx, lf->fileName
	    ,agp->chromStart
	    ,agp->chromEnd
	    ,lastPos
	    ,accumDif
	    );

    if ((agpSeq == NULL) || sameWord(agp->chrom, agpSeq))
	{
	if ((splitIntoScaffolds && words[4][0] == 'N' && sameWord(gap->type, "clone")) 
	 || (splitSize > 0 && words[4][0] != 'N' && agp->chromEnd > splitSize))
	    {
	    char *base;
	    if (splitIntoScaffolds)
		base = "scaffold";
	    else
		base = words[0];
	    freez(&newChrom);
	    safef(buf,sizeof(buf),"%s%d",base,++suffixCount);
    	    newChrom = cloneString(buf);
	    accumDif -= agp->chromStart;
	    agp->chromEnd = agp->chromEnd - agp->chromStart;
	    agp->chromStart = 0;
	    if (splitIntoScaffolds)
		{
    		suppressGap = TRUE;
		/* suppress the gap itself */
		accumDif -= agp->chromEnd - agp->chromStart;
		agp->chromEnd = 0;
		lineNo = 0;
		}
	    }
	if (splitSize > 0 || splitIntoScaffolds)
	    {
	    char *temp = agp->chrom;
	    agp->chrom = cloneString(newChrom);
	    freez(&temp);
	    agp->ix = lineNo++;
	    }
	if (cloneGapSize > 0 && words[4][0] == 'N' && sameWord(gap->type, "clone"))
	    {
	    int dif = cloneGapSize - gap->size;
	    gap->size = cloneGapSize;
	    agp->chromEnd += dif;
	    accumDif += dif;
	    }
	}

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
	if (!suppressGap)
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

if (argc != 3)
    usage();
if (optionExists("agpSeq"))
    agpSeq = optionVal("agpSeq", NULL);
if (optionExists("cloneGapSize"))
    cloneGapSize = optionInt("cloneGapSize", cloneGapSize);
if (optionExists("splitSize"))
    splitSize = optionInt("splitSize", splitSize);
if (optionExists("splitIntoScaffolds"))
    splitIntoScaffolds = TRUE;

if (splitSize > 0 && splitIntoScaffolds)
    usage();

if (verboseLevel() > 1)
    {
    verbose(2,
    "#\t"
    "agpSeq: %s\n"
    "cloneGapSize: %d\n"
    "splitSize: %d\n"
    , agpSeq
    , cloneGapSize
    , splitSize
    );
    }
agpTweak(argv[1], argv[2]);

return 0;
}
