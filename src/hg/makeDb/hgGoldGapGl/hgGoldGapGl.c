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


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGoldGapGl - Put chromosome .agp and .gl files into browser database.\n"
  "usage:\n"
  "   hgGoldGapGl database gsDir ooSubDir\n"
  "options:\n"
  "   -noGl  - don't do gl bits\n"
  "example:\n"
  "   hgGoldGapGl hg5 /projects/hg/gs.5 oo.21\n");
}

char *createGold =
"CREATE TABLE %s (\n"
"   bin smallint not null,"
"   chrom varchar(255) not null,        # which chromosome\n"
"   chromStart int unsigned not null,   # start position in chromosome\n"
"   chromEnd int unsigned not null,     # end position in chromosome\n"
"   ix int not null,    # ix of this fragment (useless)\n"
"   type char(1) not null,      # (P)redraft, (D)raft, (F)inished or (O)ther\n"
"   frag varchar(255) not null, # which fragment\n"
"   fragStart int unsigned not null,    # start position in frag\n"
"   fragEnd int unsigned not null,      # end position in frag\n"
"   strand char(1) not null,    # + or - (orientation of fragment)\n"
"             #Indices\n"
"   INDEX(bin),\n"
"   UNIQUE(chromStart),\n"
"   UNIQUE(chromEnd),\n"
"   INDEX(frag(14))\n"
")\n";

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

char *createGl = 
"CREATE TABLE %s (\n"
"    bin smallint not null,"
"    frag varchar(255) not null,	# Fragment name\n"
"    start int unsigned not null,	# Start position in golden path\n"
"    end int unsigned not null,	# End position in golden path\n"
"    strand char(1) not null,	# + or - for strand\n"
"              #Indices\n"
"   INDEX(bin),\n"
"    PRIMARY KEY(frag(20))\n"
")\n";


void makeGoldAndGap(struct sqlConnection *conn, char *chromDir)
/* Read in .agp files in chromDir and use them to create the
 * gold and gap tables for the corresponding chromosome(s). */
{
struct dyString *ds = newDyString(2048);
struct fileInfo *fiList, *fi;
char dir[256], chrom[128], ext[64];
char goldName[128], gapName[128];
char goldTabName[L_tmpnam], gapTabName[L_tmpnam];
char *agpName;
FILE *goldTab, *gapTab;
struct lineFile *lf;
int lineSize;
char *line;
char oLine[512];

fiList = listDirX(chromDir, "*.agp", TRUE);
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    char *words[16];
    int wordCount;

    /* Get full path name of .agp file and process it
     * into table names. */
    agpName = fi->name;
    printf("Processing %s\n", agpName);
    splitPath(agpName, dir, chrom, ext);
    sprintf(goldName, "%s_gold", chrom);
    sprintf(gapName, "%s_gap", chrom);

    /* Scan through .agp file splitting it into gold
     * and gap components. */
    tmpnam(goldTabName);
    tmpnam(gapTabName);
    goldTab = mustOpen(goldTabName, "w");
    gapTab = mustOpen(gapTabName, "w");
    lf = lineFileOpen(agpName, TRUE);
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
	else
	    {
	    struct agpFrag gold;
	    agpFragStaticLoad(words, &gold);
	    gold.chromStart -= 1;
	    fprintf(goldTab, "%u\t", hFindBin(start, end));
	    agpFragTabOut(&gold, goldTab);
	    }
	}
    lineFileClose(&lf);
    fclose(goldTab);
    fclose(gapTab);

    /* Create gold table and load it up. */
    if (sqlTableExists(conn, goldName))
	{
	dyStringClear(ds);
	dyStringPrintf(ds, "drop table %s", goldName);
	sqlUpdate(conn, ds->string);
	}
    dyStringClear(ds);
    dyStringPrintf(ds, createGold, goldName);
    sqlUpdate(conn, ds->string);
    dyStringClear(ds);
    dyStringPrintf(ds, "LOAD data local infile '%s' into table %s", 
        goldTabName, goldName);
    sqlUpdate(conn, ds->string);
    remove(goldTabName);

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

void addGlBin(char *in, char *out)
/* Copy in to out, but adding bin field in first column. */
{
char *row[4];
int i, start, end;
struct lineFile *lf = lineFileOpen(in, TRUE);
FILE *f = mustOpen(out, "w");

while (lineFileRow(lf, row))
    {
    start = sqlUnsigned(row[1]);
    end = sqlUnsigned(row[2]);
    fprintf(f, "%u", hFindBin(start, end));
    for (i=0; i<ArraySize(row); ++i)
        fprintf(f, "\t%s", row[i]);
    fprintf(f, "\n");
    }
carefulClose(&f);
lineFileClose(&lf);
}

void makeGl(struct sqlConnection *conn, char *chromDir, 
	 struct hash *cloneVerHash)
/* Read in .gl files in chromDir and use them to create the
 * gl tables for the corresponding chromosome(s). */
{
struct dyString *ds = newDyString(2048);
struct fileInfo *fiList, *fi;
char dir[256], chrom[128], ext[64];
char *glFileName;
char glTable[128];
char *tab = "gl.tab";

fiList = listDirX(chromDir, "*.gl", TRUE);
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    glFileName = fi->name;
    printf("Processing %s\n", glFileName);

    splitPath(glFileName, dir, chrom, ext);
    sprintf(glTable, "%s_gl", chrom);
    if (sqlTableExists(conn, glTable))
	{
	dyStringClear(ds);
	dyStringPrintf(ds, "DROP table %s", glTable);
	sqlUpdate(conn, ds->string);
	}
    dyStringClear(ds);
    dyStringPrintf(ds, createGl, glTable);
    sqlMaybeMakeTable(conn, glTable, ds->string);
    dyStringClear(ds);
    addGlBin(glFileName, tab);
    dyStringPrintf(ds, "LOAD data local infile '%s' into table %s", 
        tab, glTable);
    sqlUpdate(conn, ds->string);
    }
freeDyString(&ds);
}

void makeCloneVerHash(char *fileName, struct hash *cloneVerHash)
/* Make up a hash indexed by clone accession that has accession.version
 * values. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[8];
char acc[32];

while (lineFileRow(lf, row))
    {
    strncpy(acc, row[0], sizeof(acc));
    chopSuffix(acc);
    hashAdd(cloneVerHash, acc, cloneString(row[0]));
    }
lineFileClose(&lf);
}

void hgGoldGapGl(char *database, char *gsDir, char *ooSubDir, boolean doGl)
/* hgGoldGapGl - Put chromosome .agp and .gl files into browser database.. */
{ 
struct fileInfo *chrFiList, *chrFi; 
struct sqlConnection *conn = sqlConnect(database);
char ooDir[512];
char pathName[512];
struct hash *cloneVerHash = newHash(0);
boolean gotAny = FALSE;

sprintf(ooDir, "%s/%s", gsDir, ooSubDir);

if (doGl)
    {
    sprintf(pathName, "%s/ffa/sequence.inf", gsDir);
    makeCloneVerHash(pathName, cloneVerHash);
    }

chrFiList = listDirX(ooDir, "*", FALSE);
for (chrFi = chrFiList; chrFi != NULL; chrFi = chrFi->next)
    {
    if (chrFi->isDir && (strlen(chrFi->name) <= 2) || startsWith("NA_", chrFi->name))
        {
	sprintf(pathName, "%s/%s", ooDir, chrFi->name);
	makeGoldAndGap(conn, pathName);
	if (doGl)
	    makeGl(conn, pathName, cloneVerHash);
	gotAny = TRUE;
	uglyf("done %s\n", chrFi->name);
        }
    }
slFreeList(&chrFiList);
sqlDisconnect(&conn);
if (!gotAny)
    errAbort("No contig agp and gold files found");
}

int main(int argc, char *argv[])
/* Process command line. */
{
boolean doGl;
optionHash(&argc, argv);
if (argc != 4)
    usage();
doGl = !(optionExists("noGl") || optionExists("nogl"));
hgGoldGapGl(argv[1], argv[2], argv[3], doGl);
return 0;
}
