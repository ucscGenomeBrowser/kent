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

static char const rcsid[] = "$Id: hgGcPercent.c,v 1.13 2004/10/15 23:02:00 heather Exp $";

/* Command line switches. */
int winSize = 20000;               /* window size */
boolean noLoad = FALSE;		/* Suppress loading mysql table . */
char *file = (char *)NULL;	/* file name for output */
char *chr = (char *)NULL;	/* process only chromosome listed */
boolean noDots = FALSE;	/* TRUE == do not display ... progress */
boolean doGaps = FALSE;	/* TRUE == process gaps correctly */
int overlap = 0;                /* overlap size */
boolean useTwoBit = TRUE;  /* TRUE == use 2bit file; if not found use 4bit nibs */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"win", OPTION_INT},
    {"noLoad", OPTION_BOOLEAN},
    {"file", OPTION_STRING},
    {"chr", OPTION_STRING},
    {"noDots", OPTION_BOOLEAN},
    {"doGaps", OPTION_BOOLEAN},
    {"overlap", OPTION_INT},
    {"useTwoBit", OPTION_BOOLEAN},
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
  "   -overlap=N - overlap windows by N bases (default 0)\n"
  "   -useTwoBit - use 2bit sequence (if not found, use 4bit sequence)\n"
  "   -verbose=N - display details to stderr during processing");
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

void makeGcTab(char *nibFile, char *chrom, int chromSize, FILE *f)
/* Scan through nib file and write out GC percentage info in
 * 20 kb windows. */
{
int start = 0, end, oneSize;
int minCount = winSize/4;
int i, count, gcCount, val, ppt, gapCount;
struct dnaSeq *seq = NULL;
FILE *nf = NULL;
struct twoBitFile *tbf;
DNA *dna;
int dotMod = 0;
boolean twoBit = FALSE;

printf("#	Calculating gcPercent with window size %d\n",winSize);

if (twoBitIsFile(nibFile))
    {
    twoBit = TRUE;
    }

if (twoBit)
    {
    tbf = twoBitOpen(nibFile);
    }
else
    {
    nibOpenVerify(nibFile, &nf, &chromSize);
    }

end = start + winSize;
for (start=0; start<chromSize && end < chromSize; start =  end - overlap)
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
    if (twoBit) 
        {
	seq = twoBitReadSeqFrag(tbf, chrom, start, end);
	}
    else 
        {
        seq = nibLdPart(nibFile, nf, chromSize, start, oneSize);
	}
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
        {
	fprintf(f, "%s\t%d\t%d\t%s\t%d\n", chrom, start, end, "GC", ppt);
	}
    }
if (twoBit)
    {
    twoBitClose(&tbf);
    }
else
    {
    carefulClose(&nf);
    }
printf("\n");
}

void hgGcPercent(char *database, char *nibDir)
/* hgGcPercent - Calculate GC Percentage in 20kb windows on all chromosomes. */
{
struct fileInfo *nibList, *nibEl;
char dir[256], chrom[128], ext[64];
char *tabFileName = "gcPercent.bed";
FILE *tabFile = (FILE *) NULL;
struct sqlConnection *conn;
char query[256];
struct sqlResult *sr;
char **row;
char twoBitFile[256];
struct twoBitFile *tbf;
struct slName *twoBitNames, *el;
int size = 0;

// output file
if (file)
    tabFileName = file;

tabFile = mustOpen(tabFileName, "w");

verbose(2, "writing to file %s\n", tabFileName);

sprintf(twoBitFile, "%s/%s.2bit", nibDir, database);
if (fileExists(twoBitFile) && useTwoBit)
    {
    printf("Using twoBit: %s\n", twoBitFile);
    tbf = twoBitOpen(twoBitFile);
    twoBitNames = twoBitSeqNames(twoBitFile);
    for (el = twoBitNames; el != NULL; el = el->next)
        {
            printf("Processing twoBit %s\n", el->name);
	    size = twoBitSeqSize(tbf, el->name);
            makeGcTab(twoBitFile, el->name, size, tabFile);
	}
    }
  
else
    {
    nibList = listDirX(nibDir, "*.nib", TRUE);

    /* Create tab file with all GC percent data. */
    for (nibEl = nibList; nibEl != NULL; nibEl = nibEl->next)
        {
        splitPath(nibEl->name, dir, chrom, ext);
        if (chr)
	    {
	    char chrNib[256];
	    safef(chrNib, ArraySize(chrNib), "%s/%s.nib", nibDir, chr);
	    verbose(2, "checking name: chrNib %s =? %s nibEl->name\n", chrNib, nibEl->name);
	    if (sameString(chrNib,nibEl->name))
	        {
	        printf("Processing %s\n", nibEl->name);
	        makeGcTab(nibEl->name, chrom, 0, tabFile);
	        }
	    }
        else
	    {
                printf("Processing %s\n", nibEl->name);
                makeGcTab(nibEl->name, chrom, 0, tabFile);
	    }
        }
    slFreeList(&nibList);
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
