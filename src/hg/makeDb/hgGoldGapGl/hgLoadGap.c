/* hgGoldGapGl - Put chromosome .agp and .gl files into browser database.. */
#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "dystring.h"
#include "hash.h"
#include "options.h"
#include "agpFrag.h"
#include "agpGap.h"
#include "jksql.h"
#include "ntContig.h"
#include "glDbRep.h"
#include "hdb.h"

static char const rcsid[] = "$Id: hgLoadGap.c,v 1.4 2006/04/26 16:23:16 angie Exp $";


/* Command line switches. */
char *chrom = NULL;
boolean unsplit = FALSE;
boolean noLoad = FALSE;

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"chrom", OPTION_STRING},
    {"unsplit", OPTION_BOOLEAN},
    {"noLoad", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadGap - Load gap table from AGP-style file containing only gaps\n"
  "usage:\n"
  "   hgLoadGap database dir\n"
  "options:\n"
  "   -chrom=chrN - just do a single chromosome.  Don't delete old tables.\n"
  "   -unsplit    - Instead of making chr*_gap tables from .gap files found \n"
  "                 in dir, expect dir to be a .gap file and make an \n"
  "                 unsplit gap table from its contents.\n"
  "   -noLoad     - Don't load the database table (for testing).\n"
  "example:\n"
  "   hgLoadGap fr1 /cluster/data/fr1\n"
  "      Gap file must be named with .gap extension \n"
  "      This is only needed if gap table needs to be rebuilt,\n"
  "      without changing the gold table.\n"
);
}

char *createGapSplit = 
"CREATE TABLE %s (\n"
"   bin smallint not null,"
"   chrom varchar(255) not null,	# which chromosome\n"
"   chromStart int unsigned not null,	# start position in chromosome\n"
"   chromEnd int unsigned not null,	# end position in chromosome\n"
"   ix int not null,	# ix of this fragment (useless)\n"
"   n char(1) not null,	# always 'N'\n"
"   size int unsigned not null,	# size of gap\n"
"   type varchar(255) not null,	# contig, clone, fragment, etc.\n"
"   bridge varchar(255) not null,	# yes, no, mrna, bacEndPair, etc.\n"
"             #Indices\n"
"   INDEX(bin),\n"
"   UNIQUE(chromStart),\n"
"   UNIQUE(chromEnd)\n"
")\n";

char *createGapUnsplit = 
"CREATE TABLE %s (\n"
"   bin smallint not null,"
"   chrom varchar(255) not null,	# which chromosome\n"
"   chromStart int unsigned not null,	# start position in chromosome\n"
"   chromEnd int unsigned not null,	# end position in chromosome\n"
"   ix int not null,	# ix of this fragment (useless)\n"
"   n char(1) not null,	# always 'N'\n"
"   size int unsigned not null,	# size of gap\n"
"   type varchar(255) not null,	# contig, clone, fragment, etc.\n"
"   bridge varchar(255) not null,	# yes, no, mrna, bacEndPair, etc.\n"
"             #Indices\n"
"   INDEX(chrom(16), bin),\n"
"   UNIQUE(chrom(16), chromStart),\n"
"   UNIQUE(chrom(16), chromEnd)\n"
")\n";


void gapFileToTable(struct sqlConnection *conn, char *gapFileName,
		    char *gapTableName)
/* Build a single gap table from a single gap file. */
{
struct lineFile *lf = lineFileOpen(gapFileName, TRUE);
char tabFileName[256];
FILE *tabFile = NULL;
char *words[16];
int wordCount;

safef(tabFileName, sizeof(tabFileName), "%s.tab", gapTableName);
tabFile = mustOpen(tabFileName, "w");
while ((wordCount = lineFileChop(lf, words)) > 0)
    {
    if (wordCount < 5)
	errAbort("Short line %d of %s", lf->lineIx, lf->fileName);
    if (words[4][0] == 'N' || words[4][0] == 'U')
	{
	struct agpGap gap;
	agpGapStaticLoad(words, &gap);
	gap.chromStart -= 1;
	fprintf(tabFile, "%u\t", hFindBin(gap.chromStart, gap.chromEnd));
	agpGapTabOut(&gap, tabFile);
	}
    }
lineFileClose(&lf);
fclose(tabFile);

if (! noLoad)
    {
    char *createGapBase = unsplit ? createGapUnsplit : createGapSplit;
    char createGap[2048];
    char query[1024];
    safef(createGap, sizeof(createGap), createGapBase, gapTableName);
    sqlRemakeTable(conn, gapTableName, createGap);
    safef(query, sizeof(query), "LOAD data local infile '%s' into table %s", 
	  tabFileName, gapTableName);
    sqlUpdate(conn, query);
    remove(tabFileName);
    }
}


void makeGap(struct sqlConnection *conn, char *chromDir)
/* Read in .gap files in chromDir and use them to create the
 * gap table for the corresponding chromosome(s). */
{
struct fileInfo *fiList, *fi;

fiList = listDirX(chromDir, "*.gap", TRUE);
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    char dir[256], chrom[128], ext[64];
    char *ptr;
    char  gapName[128];
    char *gapFileName = fi->name;

    verbose(1, "Processing %s\n", gapFileName);
    /* Get full path name of .gap file and process it
     * into table names. */
    splitPath(gapFileName, dir, chrom, ext);
    while ((ptr = strchr(chrom, '.')) != NULL)
	*ptr = '_';
    safef(gapName, sizeof(gapName), "%s_gap", chrom);
    gapFileToTable(conn, gapFileName, gapName);
    }
}


void hgLoadGap(char *database, char *ooDir, char *oneChrom)
/* hgLoadGap - Put chromosome .gap files into browser database.. */
{ 
struct sqlConnection *conn = sqlConnect(database);
/* target prefix is used in zoo browser */
if (oneChrom != NULL)
    {
    if (startsWith("chr", oneChrom))
	oneChrom += 3;
    else if (startsWith("target", oneChrom))
	oneChrom += 6;
    }
    
if (unsplit)
    gapFileToTable(conn, ooDir, "gap");
else
    {
    struct fileInfo *chrFiList, *chrFi; 
    char pathName[512];
    boolean gotAny = FALSE;

    chrFiList = listDirX(ooDir, "*", FALSE);
    for (chrFi = chrFiList; chrFi != NULL; chrFi = chrFi->next)
	{
	if (chrFi->isDir &&
	    ((strlen(chrFi->name) <= 2) || startsWith("NA_", chrFi->name)))
	    {
	    if (oneChrom == NULL || sameWord(chrFi->name, oneChrom))
		{
		safef(pathName, sizeof(pathName), "%s/%s", ooDir, chrFi->name);
		makeGap(conn, pathName);
		gotAny = TRUE;
		verbose(2, "done %s\n", chrFi->name);
		}
	    }
	}
    slFreeList(&chrFiList);
    if (!gotAny)
	errAbort("No .gap files found");
    }
sqlDisconnect(&conn);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
chrom = optionVal("chrom", chrom);
unsplit = optionExists("unsplit");
noLoad = optionExists("noLoad");
hgLoadGap(argv[1], argv[2], chrom);
return 0;
}
