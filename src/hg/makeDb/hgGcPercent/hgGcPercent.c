/* hgGcPercent - Calculate GC Percentage in 20kb windows. */
#include "common.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "options.h"
#include "twoBit.h"

static char const rcsid[] = "$Id: hgGcPercent.c,v 1.16 2004/11/18 22:56:11 hiram Exp $";

/* Command line switches. */
int winSize = 20000;            /* window size */
boolean noLoad = FALSE;		/* Suppress loading mysql table . */
char *file = (char *)NULL;	/* file name for output */
char *chr = (char *)NULL;	/* process only chromosome listed */
boolean noDots = FALSE;	        /* TRUE == do not display ... progress */
boolean doGaps = FALSE;	        /* TRUE == process gaps correctly */
boolean wigOut = FALSE;	        /* TRUE == output wiggle ascii data */
int overlap = 0;                /* overlap size */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"win", OPTION_INT},
    {"noLoad", OPTION_BOOLEAN},
    {"file", OPTION_STRING},
    {"chr", OPTION_STRING},
    {"noDots", OPTION_BOOLEAN},
    {"doGaps", OPTION_BOOLEAN},
    {"overlap", OPTION_INT},
    {"wigOut", OPTION_BOOLEAN},
    {NULL, 0}
};

static char * previousChrom = (char *) NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGcPercent - Calculate GC Percentage in 20kb windows\n"
  "usage:\n"
  "   hgGcPercent [options] database nibDir\n"
  "     If nibDir/database.2bit file exists, uses that; otherwise uses \n"
  "     nibDir/*.nib.  Loads gcPercent table with counts from sequence.\n"
  "options:\n"
  "   -win=<size> - change windows size (default %d)\n"
  "   -noLoad - do not load mysql table - create bed file\n"
  "   -file=<filename> - output to <filename> (stdout OK) (implies -noLoad)\n"
  "   -chr=<chrN> - process only chrN from the nibDir\n"
  "   -noDots - do not display ... progress during processing\n"
  "   -doGaps - process gaps correctly (default: gaps are not counted as GC)\n"
  "   -wigOut - output wiggle ascii data ready to pipe to wigBedToBinary\n"
  "   -overlap=N - overlap windows by N bases (default 0)\n"
  "   -verbose=N - display details to stderr during processing\n\n"
  "example:\n"
  "  calculate GC percent in 5 base windows using a 2bit nib assembly (dp2):\n"
  "  hgGcPercent -wigOut -doGaps -file=stdout -win=5 dp2 \\\n"
  "      /cluster/data/dp2 | wigBedToBinary stdin gc5Base.wig gc5Base.wib"
      , winSize);
}

char *createTable = 
"#Displays GC percentage in 20Kb blocks for genome\n"
"CREATE TABLE gcPercent (\n"
    "chrom varchar(255) not null,	# Human chromosome number\n"
    "chromStart int unsigned not null,	# Start position in genoSeq\n"
    "chromEnd int unsigned not null,	# End position in genoSeq\n"
    "name varchar(255) not null,	# Constant string GCpct\n"
    "gcPpt int unsigned not null,	# GC percentage for 20Kb block\n"
              "#Indices\n"
    "UNIQUE(chrom(12),chromStart),\n"
    "INDEX(chrom(12), chromEnd)\n"
");\n";

static void wigOutLine(FILE *f, char *chrom, int start, int end, int ppt)
{
/*	only full winSize spans are valid	*/
if ((end - start) < winSize)
    return;

/*	see if we are starting on a new chrom	*/
if (! (previousChrom && (sameWord(previousChrom, chrom))))
    {
    freeMem(previousChrom);
    previousChrom = cloneString(chrom);
    fprintf(f, "variableStep chrom=%s span=%d\n", chrom, winSize);
    }
fprintf(f, "%d\t%g\n", start+1, ppt/10.0);
}

void makeGcLineFromSeq(struct dnaSeq *seq, char *chrom, int start, int end,
		       FILE *f)
/* Given a window of sequence, print out a line of BED 5 with the GC 
 * parts per thousand (not percent) in the window. */
{
static int dotMod = 0;
DNA *dna = seq->dna;
int minCount = winSize/4;
int i, count, gcCount, val, ppt, gapCount;

if ((++dotMod&127) == 0)
    {
    if (!noDots)
	{
	verbose(1, ".");
	if ((dotMod & 8191) == 0)
	    verbose(1, "\n");
	fflush(stdout);
	}
    }

gapCount = count = gcCount = 0;
for (i=0; i < seq->size; ++i)
    {
    if ((val = ntVal[(int)dna[i]]) >= 0)
	{
	++count;
	if (val == G_BASE_VAL || val == C_BASE_VAL)
	    {
	    ++gcCount;
	    }
	}
    else
	{
	++gapCount;
	}
    }
if (count >= minCount)
    ppt = round(1000.0*(double)gcCount/(double)count);
else
    ppt = 0;


if (doGaps)
    {
    if (gapCount < seq->size)
	{
	if (wigOut)
	    wigOutLine(f, chrom, start, end, ppt);
	else
	    fprintf(f, "%s\t%d\t%d\t%s\t%d\n", chrom, start, end, "GC", ppt);
	}
    }
else
    {
    if (wigOut)
	wigOutLine(f, chrom, start, end, ppt);
    else
	fprintf(f, "%s\t%d\t%d\t%s\t%d\n", chrom, start, end, "GC", ppt);
    }
}


void makeGcTabFromNib(char *nibFile, char *chrom, FILE *f)
/* Scan through nib file and write out GC percentage info. */
{
int start = 0, end = 0;
int chromSize;
FILE *nf = NULL;

nibOpenVerify(nibFile, &nf, &chromSize);

for (start=0, end=0;  start < chromSize && end < chromSize;  
     start = end - overlap)
    {
    struct dnaSeq *seq = NULL;
    end = start + winSize;
    if (end > chromSize)
        end = chromSize;
    seq = nibLdPart(nibFile, nf, chromSize, start, end - start);
    makeGcLineFromSeq(seq, chrom, start, end, f);
    freeDnaSeq(&seq);
    }
carefulClose(&nf);
if (!noDots)
    verbose(1, "\n");
}


void makeGcTabFromTwoBit(char *twoBitFileName, FILE *f)
/* Scan through all sequenes in .2bit file and write out GC percentage info. */
{
struct slName *twoBitNames = twoBitSeqNames(twoBitFileName);
struct slName *el = NULL;
struct twoBitFile *tbf = twoBitOpen(twoBitFileName);

for (el = twoBitNames; el != NULL; el = el->next)
    {
    int start = 0, end = 0;
    int chromSize = twoBitSeqSize(tbf, el->name);
    char *chrom = el->name;
    if (chr)
	{
	verbose(2, "#\tchecking name: %s =? %s\n", chrom, chr);
	if (! sameString(chrom, chr))
	    continue;
	}
    verbose(2, "#\tProcessing twoBit sequence %s\n", chrom);
    for (start=0, end=0;  start < chromSize && end < chromSize;  
	 start = end - overlap)
	{
	struct dnaSeq *seq = NULL;
	end = start + winSize;
	if (end > chromSize)
	    end = chromSize;
	seq = twoBitReadSeqFrag(tbf, chrom, start, end);
	makeGcLineFromSeq(seq, chrom, start, end, f);
	freeDnaSeq(&seq);
	}
    }
if (!noDots)
    verbose(1, "\n");
}


void hgGcPercent(char *database, char *nibDir)
/* hgGcPercent - Calculate GC Percentage on all chromosomes, load db table. */
{
char *tabFileName = file ? file : "gcPercent.bed";
FILE *tabFile = mustOpen(tabFileName, "w");
char twoBitFile[512];

verbose(1, "#\tCalculating gcPercent with window size %d\n", winSize);
verbose(2, "#\tWriting to tab file %s\n", tabFileName);

sprintf(twoBitFile, "%s/%s.2bit", nibDir, database);
if (fileExists(twoBitFile))
    {
    verbose(1, "#\tUsing twoBit: %s\n", twoBitFile);
    makeGcTabFromTwoBit(twoBitFile, tabFile);
    }  
else
    {
    struct fileInfo *nibList, *nibEl;
    nibList = listDirX(nibDir, "*.nib", TRUE);
    for (nibEl = nibList; nibEl != NULL; nibEl = nibEl->next)
        {
	char dir[256], chrom[128], ext[64];
        splitPath(nibEl->name, dir, chrom, ext);
        if (chr)
	    {
	    verbose(2, "#\tchecking name: %s =? %s\n", chrom, chr);
	    if (! sameString(chrom, chr))
		continue;
	    }
	verbose(1, "#\tProcessing %s\n", nibEl->name);
	makeGcTabFromNib(nibEl->name, chrom, tabFile);
        }
    slFreeList(&nibList);
    }
carefulClose(&tabFile);
verbose(1, "#\tFile %s created\n", tabFileName);

/* Load that file in database. */
if (!noLoad)
    {
    struct sqlConnection *conn = sqlConnect(database);
    char query[1024];
    verbose(1, "#\tLoading gcPercent table\n");
    sqlMaybeMakeTable(conn, "gcPercent", createTable);
    sqlUpdate(conn, "DELETE from gcPercent");
    safef(query, sizeof(query),
	  "LOAD data local infile '%s' into table gcPercent", tabFileName);
    sqlUpdate(conn, query);
    sqlDisconnect(&conn);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);

if (argc <3)
    usage();

dnaUtilOpen();
winSize = optionInt("win", 20000);
noLoad = optionExists("noLoad");
noDots = optionExists("noDots");
doGaps = optionExists("doGaps");
wigOut = optionExists("wigOut");
overlap = optionInt("overlap", 0);
file = optionVal("file", NULL);
chr = optionVal("chr", NULL);

/*	sending output to file implies no loading of DB, and also if the
 *	stated file is "stdout" we should not do ... progress reports
 */
if (file)
    {
    noLoad = TRUE;
    if (sameString(file,"stdout")) noDots = TRUE;
    }


if (verboseLevel() >= 2)
    {
    fprintf(stderr, "#\thgGcPercent -win=%d", winSize);
    if (file) fprintf(stderr, " -file=%s", file);
    if (noLoad) fprintf(stderr, " -noLoad");
    if (noDots) fprintf(stderr, " -noDots");
    if (doGaps) fprintf(stderr, " -doGaps");
    if (wigOut) fprintf(stderr, " -wigOut");
    if (chr) fprintf(stderr, " -chr=%s", chr);
    fprintf(stderr, "\n");
    }

hgGcPercent(argv[1], argv[2]);
return 0;
}
