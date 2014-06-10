/* hgGoldGapGl - Put chromosome .agp and .gl files into browser database.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
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


static boolean noLoad = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGoldGapGl - Put chromosome .agp and .gl files into browser database.\n"
  "usage:\n"
  "   hgGoldGapGl database gsDir ooSubDir\n"
  "	(this usage creates split gold and gap tables)\n"
  "or\n"
  "   hgGoldGapGl database agpFile\n"
  "	(this usage creates single gold and gap tables)\n"
  "options:\n"
  "   -noGl  - don't do gl bits\n"
  "   -chrom=chrN - just do a single chromosome.  Don't delete old tables.\n"
  "   -chromLst=chrom.lst - chromosomes subdirs are named in chrom.lst (1, 2, ...)\n"
  "   -noLoad - do not load tables, leave SQL files instead.\n"
  "   -verbose n - n==2 brief information and SQL table create statements\n"
  "              - n==3 show all gaps\n"
  "example:\n"
  "   hgGoldGapGl -noGl hg16 /cluster/data/hg16 .\n");
}

char *goldTabName = "gold.tab";
char *gapTabName = "gap.tab";

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
"             #Indices\n";

char *goldSplitIndex =
"   INDEX(bin),\n"
"   UNIQUE(chromStart),\n"
"   INDEX(frag(%d))\n"
")\n";

static int maxChromNameSize = 0;
static int maxFragNameSize = 0;

char *goldIndex = 
"   INDEX(chrom(%d),bin),\n"
"   UNIQUE(chrom(%d),chromStart),\n"
"   INDEX(frag(%d))\n"
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
"             #Indices\n";

char *gapSplitIndex =
"   INDEX(bin),\n"
"   UNIQUE(chromStart)\n"
")\n";

char *gapIndex =
"   INDEX(chrom(%d),bin),\n"
"   UNIQUE(chrom(%d),chromStart)\n"
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
"    PRIMARY KEY(frag(%d))\n"
")\n";


static void agpFragValidate(struct agpFrag *af)
/* Check for weirdness in agpFrag. */
{
/* OK if equal since these coords are 1-based */
if (af->chromStart > af->chromEnd)
  errAbort("hgGoldGapGl: unexpected coords start %d > end %d for frag %s in chrom %s\n", 
         af->chromStart, af->chromEnd, af->frag, af->chrom);
}

void splitAgp(char *agpName, char *goldFileName, char *gapFileName)
/* Split up agp file into gold and gap files. */
{
struct lineFile *lf;
char *words[16];
int wordCount;
FILE *goldTab, *gapTab;

/* Scan through .agp file splitting it into gold
 * and gap components. */
goldTab = mustOpen(goldFileName, "w");
gapTab = mustOpen(gapFileName, "w");
lf = lineFileOpen(agpName, TRUE);
while ((wordCount = lineFileChop(lf, words)) > 0)
    {
    int start, end;
    if (wordCount < 5)
	errAbort("Short line %d of %s", lf->lineIx, lf->fileName);
    int len = strlen(words[0]);
    if (len > maxChromNameSize)
	{
	maxChromNameSize = len;
	if (maxChromNameSize > 254)
	    errAbort("ERROR: chrom name size is over 254(%d) characters: "
		"'%s'", maxChromNameSize, words[0]);
	}

    start = sqlUnsigned(words[1])-1;
    end = sqlUnsigned(words[2]);
    if (words[4][0] == 'N' || words[4][0] == 'U')
	{
	struct agpGap gap;
	agpGapStaticLoad(words, &gap);
	gap.chromStart -= 1;
	fprintf(gapTab, "%u\t", hFindBin(start, end));
	agpGapTabOut(&gap, gapTab);
	verbose(3,"#GAP\t%s:%d-%d\n", gap.chrom, gap.chromStart, gap.chromEnd);
	}
    else
	{
	struct agpFrag gold;
	agpFragStaticLoad(words, &gold);
	agpFragValidate(&gold);
	len = strlen(words[5]);
	if (len > maxFragNameSize)
	    {
	    maxFragNameSize = len;
	    if (maxFragNameSize > 254)
		errAbort("ERROR: fragment name size is over 254(%d) "
		    "characters: '%s'", maxFragNameSize, words[5]);
	    }
	// file is 1-based. agpFragLoad() now assumes 0-based. 
	// and agpFragTabOut() will assume 1-based, but we will load 
	// the generated file straight into the database, so 
	// subtract 2:
	gold.chromStart -= 2;
	gold.fragStart  -= 2;
	fprintf(goldTab, "%u\t", hFindBin(start, end));
	agpFragTabOut(&gold, goldTab);
	}
    }
lineFileClose(&lf);
carefulClose(&goldTab);
carefulClose(&gapTab);

}

void makeGoldAndGap(struct sqlConnection *conn, char *chromDir)
/* Read in .agp files in chromDir and use them to create the
 * gold and gap tables for the corresponding chromosome(s). */
{
struct dyString *ds = newDyString(2048);
struct fileInfo *fiList, *fi;
char dir[256], chrom[128], ext[64];
char goldName[128], gapName[128];
char *agpName;
char *ptr;
char goldFileName[128];
char gapFileName[128];

if (! noLoad)
    {
    safef(goldFileName, ArraySize(goldFileName), "%s", goldTabName);
    safef(gapFileName, ArraySize(gapFileName), "%s", gapTabName);
    }
fiList = listDirX(chromDir, "*.agp", TRUE);
for (fi = fiList; fi != NULL; fi = fi->next)
    {

    /* Get full path name of .agp file and process it
     * into table names. */
    agpName = fi->name;
    printf("Processing %s\n", agpName);
    splitPath(agpName, dir, chrom, ext);
    while ((ptr = strchr(chrom, '.')) != NULL)
	*ptr = '_';
    sprintf(goldName, "%s_gold", chrom);
    sprintf(gapName, "%s_gap", chrom);

    if (noLoad)
	{
	safef(goldFileName, ArraySize(goldFileName), "%s_gold.tab", chrom);
	safef(gapFileName, ArraySize(gapFileName), "%s_gap.tab", chrom);
	}

    /* Create gold & gap tab separated files. */
    splitAgp(fi->name, goldFileName, gapFileName);

    /* Create gold table and load it up. */
    dyStringClear(ds);
    sqlDyStringPrintf(ds, createGold, goldName);
    sqlDyStringPrintf(ds, goldSplitIndex, maxFragNameSize);
    verbose(2, "%s", ds->string);
    if (! noLoad)
	sqlRemakeTable(conn, goldName, ds->string);
    dyStringClear(ds);
    sqlDyStringPrintf(ds, "LOAD data local infile '%s' into table %s", 
        goldFileName, goldName);
    if (! noLoad)
	{
	sqlUpdate(conn, ds->string);
	remove(goldFileName);
	}

    /* Create gap table and load it up. */
    dyStringClear(ds);
    sqlDyStringPrintf(ds, createGap, gapName);
    dyStringAppend(ds, gapSplitIndex);
    verbose(2, "%s", ds->string);
    if (! noLoad)
	{
	sqlRemakeTable(conn, gapName, ds->string);
	sqlMaybeMakeTable(conn, gapName, ds->string);
	}
    dyStringClear(ds);
    sqlDyStringPrintf(ds, "LOAD data local infile '%s' into table %s", 
        gapFileName, gapName);
    if (! noLoad)
	{
	sqlUpdate(conn, ds->string);
	remove(gapFileName);
	}
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
    if ( (! noLoad) && sqlTableExists(conn, glTable))
	{
	dyStringClear(ds);
	sqlDyStringPrintf(ds, "DROP table %s", glTable);
	sqlUpdate(conn, ds->string);
	}
    dyStringClear(ds);
    sqlDyStringPrintf(ds, createGl, glTable, maxFragNameSize);
    verbose(2, "%s", ds->string);
    if (! noLoad)
	sqlMaybeMakeTable(conn, glTable, ds->string);
    dyStringClear(ds);
    addGlBin(glFileName, tab);
    sqlDyStringPrintf(ds, "LOAD data local infile '%s' into table %s", 
        tab, glTable);
    if (! noLoad)
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

void hgGoldGapGl(char *database, char *gsDir, char *ooSubDir, boolean doGl, char *oneChrom)
/* hgGoldGapGl - Put chromosome .agp and .gl files into browser database.. */
{ 
struct fileInfo *chrFiList, *chrFi; 
struct sqlConnection *conn = NULL;
char ooDir[512];
char pathName[512];
struct hash *cloneVerHash = newHash(0);
boolean gotAny = FALSE;
struct hash *chromDirHash = newHash(4);
char *chromLst = optionVal("chromLst", NULL);

if (! noLoad)
    conn = sqlConnect(database);

verbose(2,"#\tcomplete gold, gap and .gl files produced\n");

if (chromLst != NULL)
    {
    struct lineFile *clf = lineFileOpen(chromLst, TRUE);
    char *row[1];
    while (lineFileRow(clf, row))
	{
	hashAdd(chromDirHash, row[0], NULL);
	}
    lineFileClose(&clf);
    }

sprintf(ooDir, "%s/%s", gsDir, ooSubDir);
/* target prefix is used in zoo browser */
if (oneChrom != NULL && (startsWith("chr", oneChrom) || startsWith("target", oneChrom)))
    oneChrom += 3;
    

if (doGl)
    {
    sprintf(pathName, "%s/ffa/sequence.inf", gsDir);
    makeCloneVerHash(pathName, cloneVerHash);
    }

chrFiList = listDirX(ooDir, "*", FALSE);
for (chrFi = chrFiList; chrFi != NULL; chrFi = chrFi->next)
    {
    if (chrFi->isDir &&
	((strlen(chrFi->name) <= 2) || startsWith("NA_", chrFi->name) ||
	 (NULL != hashLookup(chromDirHash, chrFi->name))))
        {
	if (oneChrom == NULL || sameWord(chrFi->name, oneChrom))
	    {
	    sprintf(pathName, "%s/%s", ooDir, chrFi->name);
	    makeGoldAndGap(conn, pathName);
	    if (doGl)
		makeGl(conn, pathName, cloneVerHash);
	    gotAny = TRUE;
	    uglyf("done %s\n", chrFi->name);
	    }
        }
    }
slFreeList(&chrFiList);
if (! noLoad)
    sqlDisconnect(&conn);
hashFree(&chromDirHash);
if (!gotAny)
    errAbort("No contig agp and gold files found");
}


void hgGoldGap(char *database, char *agpFile)
/* hgGoldGap - Put chromosome .agp file into browser database.. */
{
struct dyString *ds = dyStringNew(0);
struct sqlConnection *conn = NULL;

if (! noLoad)
    conn = sqlConnect(database);

verbose(2,"#\tsimple gold gap, no .gl files produced, from agp file: %s\n",
	agpFile);

splitAgp(agpFile, goldTabName, gapTabName);

/* Create gold table and load it up. */
dyStringClear(ds);
sqlDyStringPrintf(ds, createGold, "gold");
sqlDyStringPrintf(ds, goldIndex, maxChromNameSize, maxChromNameSize, maxFragNameSize);
verbose(2, "%s", ds->string);
if (! noLoad)
    sqlRemakeTable(conn, "gold", ds->string);
dyStringClear(ds);
sqlDyStringPrintf(ds, "LOAD data local infile '%s' into table %s", 
    goldTabName, "gold");
if (! noLoad)
    {
    sqlUpdate(conn, ds->string);
    remove(goldTabName);
    }

/* Create gap table and load it up. */
dyStringClear(ds);
sqlDyStringPrintf(ds, createGap, "gap");
sqlDyStringPrintf(ds, gapIndex, maxChromNameSize, maxChromNameSize);
verbose(2, "%s", ds->string);
if (! noLoad)
    {
    sqlRemakeTable(conn, "gap", ds->string);
    sqlMaybeMakeTable(conn, "gap", ds->string);
    }
dyStringClear(ds);
sqlDyStringPrintf(ds, "LOAD data local infile '%s' into table %s", 
    gapTabName, "gap");
if (! noLoad)
    {
    sqlUpdate(conn, ds->string);
    remove(gapTabName);
    sqlDisconnect(&conn);
    }
dyStringFree(&ds);
}

int main(int argc, char *argv[])
/* Process command line. */
{
boolean doGl = FALSE;

optionHash(&argc, argv);

if (argc != 4 && argc != 3)
    usage();

noLoad = optionExists("noLoad");
if (noLoad)
    verbose(2,"#\tnoLoad option, leaving SQL files, no table loading\n");

doGl = !(optionExists("noGl") || optionExists("nogl"));

if (argc == 3)
    hgGoldGap(argv[1], argv[2]);
else
    hgGoldGapGl(argv[1], argv[2], argv[3], doGl, optionVal("chrom", NULL));
return 0;
}
