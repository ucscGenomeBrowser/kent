/* hgGcPercent - Calculate GC Percentage in 20kb windows. */
#include "common.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "options.h"

static char const rcsid[] = "$Id: hgGcPercent.c,v 1.6 2004/02/03 00:44:56 hiram Exp $";

/* Command line switches. */
int winSize = 20000;               /* window size */
boolean noLoad = FALSE;		/* Suppress loading mysql table . */
char *file = (char *)NULL;	/* file name for output */
char *chr = (char *)NULL;	/* process only chromosome listed */
boolean noDots = FALSE;	/* TRUE == do not display ... progress */
boolean doGaps = FALSE;	/* TRUE == process gaps correctly */
boolean verbose = FALSE;	/* Explain what is happening */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"win", OPTION_INT},
    {"noLoad", OPTION_BOOLEAN},
    {"file", OPTION_STRING},
    {"chr", OPTION_STRING},
    {"noDots", OPTION_BOOLEAN},
    {"doGaps", OPTION_BOOLEAN},
    {"verbose", OPTION_BOOLEAN},
    {NULL, 0}
};


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGcPercent - Calculate GC Percentage in 20kb windows\n"
  "usage:\n"
  "   hgGcPercent [options] database nibDir\n"
  "options:\n"
  "   -win=<size> - change windows size (default 20000)\n"
  "   -noLoad - do not load mysql table - create bed file\n"
  "   -file=<filename> - output to <filename> (stdout OK) (implies -noLoad)\n"
  "   -chr=<chrN> - process only chrN from the nibDir\n"
  "   -noDots - do not display ... progress during processing\n"
  "   -doGaps - process gaps correctly (default: gaps are not counted as GC)\n"
  "   -verbose - display details to stderr during processing");
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

void makeGcTab(char *nibFile, char *chrom, FILE *f)
/* Scan through nib file and write out GC percentage info in
 * 20 kb windows. */
{
int chromSize, start, end, oneSize;
int minCount = winSize/4;
int i, count, gcCount, val, ppt, gapCount;
struct dnaSeq *seq = NULL;
FILE *nf = NULL;
DNA *dna;
int dotMod = 0;

printf("#	Calculating gcPercent with window size %d\n",winSize);
nibOpenVerify(nibFile, &nf, &chromSize);
for (start=0; start<chromSize; start = end)
    {
    if ((++dotMod&127) == 0)
	{
	if (!noDots)
	    {
	    printf(".");
	    fflush(stdout);
	    }
	}
    end = start + winSize;
    if (end > chromSize)
        end = chromSize;
    oneSize = end - start;
    seq = nibLdPart(nibFile, nf, chromSize, start, oneSize);
    dna = seq->dna;
    gapCount = count = gcCount = 0;
    for (i=0; i<oneSize; ++i)
        {
	if ((val = ntVal[dna[i]]) >= 0)
	    {
	    ++count;
	    if (val == G_BASE_VAL || val == C_BASE_VAL)
		{
	        ++gcCount;
		}
	    } else {
		++gapCount;
	    }
	}
    freeDnaSeq(&seq);
    if (count >= minCount)
	ppt = round(1000.0*(double)gcCount/(double)count);
    else
        ppt = 0;
    /*	Recogize windows full of N's, do no output.
     */
    if (doGaps)
	{
	if (gapCount < oneSize)
	    fprintf(f, "%s\t%d\t%d\t%s\t%d\n", chrom, start, end, "GC", ppt);
	}
    else
	fprintf(f, "%s\t%d\t%d\t%s\t%d\n", chrom, start, end, "GC", ppt);
    }
carefulClose(&nf);
printf("\n");
}

void hgGcPercent(char *database, char *nibDir)
/* hgGcPercent - Calculate GC Percentage in 20kb windows on all chromosomes. */
{
struct fileInfo *nibList = listDirX(nibDir, "*.nib", TRUE), *nibEl;
char dir[256], chrom[128], ext[64];
char *tabFileName = "gcPercent.bed";
FILE *tabFile = (FILE *) NULL;
struct sqlConnection *conn;
char query[256];

if (file)
    tabFileName = file;

tabFile = mustOpen(tabFileName, "w");

if (verbose) fprintf(stderr, "writing to file %s\n", tabFileName);

/* Create tab file with all GC percent data. */
for (nibEl = nibList; nibEl != NULL; nibEl = nibEl->next)
    {
    splitPath(nibEl->name, dir, chrom, ext);
    if (chr)
	{
	char chrNib[256];
	safef(chrNib, ArraySize(chrNib), "%s/%s.nib", nibDir, chr);
	if (verbose) fprintf( stderr, "checking name: chrNib %s =? %s nibEl->name\n", chrNib, nibEl->name);
	if (sameString(chrNib,nibEl->name))
	    {
	    printf("Processing %s\n", nibEl->name);
	    makeGcTab(nibEl->name, chrom, tabFile);
	    }
	}
    else
	{
    printf("Processing %s\n", nibEl->name);
    makeGcTab(nibEl->name, chrom, tabFile);
	}
    }
carefulClose(&tabFile);
printf("File %s created\n",tabFileName);

/* Load that file in database. */
if (!noLoad)
    {
    conn = sqlConnect(database);
    printf("Loading gcPercent table\n");
    sqlMaybeMakeTable(conn, "gcPercent", createTable);
    sqlUpdate(conn, "DELETE from gcPercent");
    sprintf(query, "LOAD data local infile '%s' into table gcPercent", tabFileName);
    sqlUpdate(conn, query);
    sqlDisconnect(&conn);
    }

slFreeList(&nibList);
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
verbose = optionExists("verbose");
noDots = optionExists("noDots");
doGaps = optionExists("doGaps");
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


if (verbose)
    {
    fprintf(stderr, "hgGcPercent -win=%d", winSize);
    if (file) fprintf(stderr, " -file=%s", file);
    if (noLoad) fprintf(stderr, " -noLoad");
    if (noDots) fprintf(stderr, " -noDots");
    if (doGaps) fprintf(stderr, " -doGaps");
    if (chr) fprintf(stderr, " -chr=%s", chr);
    fprintf(stderr, "\n");
    }

hgGcPercent(argv[1], argv[2]);
return 0;
}
