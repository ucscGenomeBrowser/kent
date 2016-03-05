/* hgCtgPos - Store contig positions ( from lift files ) in database.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "dystring.h"
#include "hash.h"
#include "options.h"
#include "hCommon.h"
#include "jksql.h"
#include "liftSpec.h"
#include "ctgPos.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgCtgPos - Store contig positions ( from lift files ) in database.\n"
  "usage:\n"
  "   hgCtgPos <options> database ooDir\n"
  "options:\n"
  "   -chromLst=chrom.lst - chromosomes subdirs are named in chrom.lst (1, 2, ...)\n"
  "example:\n"
  "   cd /cluster/data/hg17\n"
  "   hgCtgPos -chromLst=chrom.lst hg17 .\n");
}

int cmpCtgPos(const void *va, const void *vb)
/* Compare to sort based on chromosome and chromStart. */
{
const struct ctgPos *a = *((struct ctgPos **)va);
const struct ctgPos *b = *((struct ctgPos **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
return dif;
}

char *skipPastSlash(char *s)
/* Return past first slash in s. */
{
char *e = strchr(s, '/');
if (e == NULL)
    return s;
return e+1;
}

void addCtgFile(char *liftFileName, struct ctgPos **pCtgList)
/* Create ctgPos's out of liftSpecs in liftFile. */
{
struct lineFile *lf = lineFileOpen(liftFileName, TRUE);
int lineSize, wordCount;
char *line, *words[16];
struct liftSpec lift;
struct ctgPos *ctg;

printf("Processing %s\n", liftFileName);
while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;
    if (wordCount != 5)
        errAbort("Expecting 5 words line %d of %s", lf->lineIx, lf->fileName);
    liftSpecStaticLoad(words, &lift);
    AllocVar(ctg);
    ctg->contig = cloneString(skipPastSlash(lift.oldName));
    ctg->size = lift.oldSize;
    ctg->chrom = cloneString(lift.newName);
    ctg->chromStart = lift.offset;
    ctg->chromEnd = lift.offset + lift.oldSize;
    slAddHead(pCtgList, ctg);
    }
lineFileClose(&lf);
}

char *createCtgPos =
NOSQLINJ "CREATE TABLE ctgPos (\n"
"   contig varchar(255) not null,	# Name of contig\n"
"   size int unsigned not null,	# Size of contig\n"
"   chrom varchar(255) not null,	# Chromosome name\n"
"   chromStart int unsigned not null,	# Start in chromosome\n"
"   chromEnd int unsigned not null,	# End in chromosome\n"
"             #Indices\n"
"   PRIMARY KEY(contig),\n"
"   UNIQUE(chrom(16),chromStart)\n"
")\n";

void saveCtgPos(struct ctgPos *ctgList, char *database)
/* Save ctgList to database. */
{
struct sqlConnection *conn = sqlConnect(database);
struct ctgPos *ctg;
char *tabFileName = "ctgPos.tab";
FILE *f;
struct dyString *ds = newDyString(2048);

/* Create tab file from ctg list. */
printf("Creating tab file\n");
f = mustOpen(tabFileName, "w");
for (ctg = ctgList; ctg != NULL; ctg = ctg->next)
    ctgPosTabOut(ctg, f);
fclose(f);

/* Create table if it doesn't exist, delete whatever is
 * already in it, and fill it up from tab file. */
printf("Loading ctgPos table\n");
sqlMaybeMakeTable(conn, "ctgPos", createCtgPos);
sqlUpdate(conn, NOSQLINJ "DELETE from ctgPos");
sqlDyStringPrintf(ds, "LOAD data local infile '%s' into table ctgPos", 
    tabFileName);
sqlUpdate(conn, ds->string);

/* Clean up. */
remove(tabFileName);
sqlDisconnect(&conn);
}


void hgCtgPos(char *database, char *ooDir)
/* hgCtgPos - Store contig positions ( from lift files ) in database.. */
{
struct ctgPos *ctgList = NULL;
char liftFileName[512];
struct fileInfo *fiList, *fi;
static char *liftNames[2] = {"lift/ordered.lft", "lift/random.lft"};
int i;
struct hash *chromDirHash = newHash(4);
char *chromLst = optionVal("chromLst", NULL);

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

fiList = listDirX(ooDir, "*", FALSE);
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    if (fi->isDir && ((strlen(fi->name) <= 2) || startsWith("NA_", fi->name)
		|| hashLookup(chromDirHash, fi->name) ))
        {
	for (i=0; i<ArraySize(liftNames); ++i)
	    {
	    sprintf(liftFileName, "%s/%s/%s", ooDir, fi->name, liftNames[i]);
	    if (fileExists(liftFileName))
	        {
		addCtgFile(liftFileName, &ctgList);
		}
	    }
	}
    }
slSort(&ctgList, cmpCtgPos);
printf("Got %d contigs total\n", slCount(ctgList));
saveCtgPos(ctgList, database);
hashFree(&chromDirHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
hgCtgPos(argv[1], argv[2]);
return 0;
}
