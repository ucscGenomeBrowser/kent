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

static char const rcsid[] = "$Id: hgLoadGap.c,v 1.1 2003/09/27 02:01:02 kate Exp $";


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadGap - Load gap table from AGP-style file containing only gaps\n"
  "usage:\n"
  "   hgLoadGap database dir\n"
  "options:\n"
  "   -chrom=chrN - just do a single chromosome.  Don't delete old tables.\n"
  "example:\n"
  "   hgLoadGap fr1 /cluster/data/fr1\n"
  "      Gap file must be named with .gap extension"
  "      This is only needed if gap table needs to be rebuilt,"
  "      without changing the gold table.");
}

char *createGap = 
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


void makeGap(struct sqlConnection *conn, char *chromDir)
/* Read in .gap files in chromDir and use them to create the
 * gap table for the corresponding chromosome(s). */
{
struct dyString *ds = newDyString(2048);
struct fileInfo *fiList, *fi;
char dir[256], chrom[128], ext[64];
char  gapName[128];
char *gapTabName = "gap.tab";
char *gapFileName;
FILE *gapTab;
struct lineFile *lf;
int lineSize;
char *line;
char oLine[512];

fiList = listDirX(chromDir, "*.gap", TRUE);
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    char *ptr;
    char *words[16];
    int wordCount;

    /* Get full path name of .gap file and process it
     * into table names. */
    gapFileName = fi->name;
    printf("Processing %s\n", gapFileName);
    splitPath(gapFileName, dir, chrom, ext);
    while ((ptr = strchr(chrom, '.')) != NULL)
	*ptr = '_';
    sprintf(gapName, "%s_gap", chrom);

    /* Scan through .gap file */
    gapTab = mustOpen(gapTabName, "w");
    lf = lineFileOpen(gapFileName, TRUE);
    while ((wordCount = lineFileChop(lf, words)) > 0)
        {
	int start, end;
	if (wordCount < 5)
	    errAbort("Short line %d of %s", lf->lineIx, lf->fileName);
	start = sqlUnsigned(words[1])-1;
	end = sqlUnsigned(words[2]);
	if (sameWord(words[4], "N"))
	    {
	    struct agpGap gap;
	    agpGapStaticLoad(words, &gap);
	    gap.chromStart -= 1;
	    fprintf(gapTab, "%u\t", hFindBin(start, end));
	    agpGapTabOut(&gap, gapTab);
	    }
	}
    lineFileClose(&lf);
    fclose(gapTab);

    /* Create gap table and load it up. */
    if (sqlTableExists(conn, gapName))
	{
	dyStringClear(ds);
	dyStringPrintf(ds, "DROP table %s", gapName);
	sqlUpdate(conn, ds->string);
	}
    dyStringClear(ds);
    dyStringPrintf(ds, createGap, gapName);
    sqlMaybeMakeTable(conn, gapName, ds->string);
    dyStringClear(ds);
    dyStringPrintf(ds, "LOAD data local infile '%s' into table %s", 
        gapTabName, gapName);
    sqlUpdate(conn, ds->string);
    remove(gapTabName);
    }
freeDyString(&ds);
}


void hgLoadGap(char *database, char *ooDir, char *oneChrom)
/* hgLoadGap - Put chromosome .gap files into browser database.. */
{ 
struct fileInfo *chrFiList, *chrFi; 
struct sqlConnection *conn = sqlConnect(database);
char pathName[512];
boolean gotAny = FALSE;

/* target prefix is used in zoo browser */
if (oneChrom != NULL && (startsWith("chr", oneChrom) || startsWith("target", oneChrom)))
    oneChrom += 3;
    
chrFiList = listDirX(ooDir, "*", FALSE);
for (chrFi = chrFiList; chrFi != NULL; chrFi = chrFi->next)
    {
    if (chrFi->isDir && (strlen(chrFi->name) <= 2) || startsWith("NA_", chrFi->name))
        {
	if (oneChrom == NULL || sameWord(chrFi->name, oneChrom))
	    {
	    sprintf(pathName, "%s/%s", ooDir, chrFi->name);
	    makeGap(conn, pathName);
	    gotAny = TRUE;
	    uglyf("done %s\n", chrFi->name);
	    }
        }
    }
slFreeList(&chrFiList);
sqlDisconnect(&conn);
if (!gotAny)
    errAbort("No .gap files found");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
hgLoadGap(argv[1], argv[2], optionVal("chrom", NULL));
return 0;
}
