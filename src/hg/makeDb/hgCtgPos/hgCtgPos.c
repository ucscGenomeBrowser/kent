/* hgCtgPos - Store contig positions ( from lift files ) in database.. */
#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "dystring.h"
#include "hash.h"
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
  "   hgCtgPos database ooDir\n");
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
"CREATE TABLE ctgPos (\n"
"   contig varchar(255) not null,	# Name of contig\n"
"   size int unsigned not null,	# Size of contig\n"
"   chrom varchar(255) not null,	# Chromosome name\n"
"   chromStart int unsigned not null,	# Start in chromosome\n"
"   chromEnd int unsigned not null,	# End in chromosome\n"
"             #Indices\n"
"   PRIMARY KEY(contig),\n"
"   UNIQUE(chrom(16),chromStart),\n"
"   UNIQUE(chrom(16),chromEnd)\n"
")\n";

void saveCtgPos(struct ctgPos *ctgList, char *database)
/* Save ctgList to database. */
{
struct sqlConnection *conn = sqlConnect(database);
struct ctgPos *ctg;
char tabFileName[L_tmpnam];
FILE *f;
struct dyString *ds = newDyString(2048);

/* Create tab file from ctg list. */
printf("Creating tab file\n");
tmpnam(tabFileName);
f = mustOpen(tabFileName, "w");
for (ctg = ctgList; ctg != NULL; ctg = ctg->next)
    ctgPosTabOut(ctg, f);
fclose(f);

/* Create table if it doesn't exist, delete whatever is
 * already in it, and fill it up from tab file. */
printf("Loading ctgPos table\n");
sqlMaybeMakeTable(conn, "ctgPos", createCtgPos);
sqlUpdate(conn, "DELETE from ctgPos");
dyStringPrintf(ds, "LOAD data local infile '%s' into table ctgPos", 
    tabFileName);
sqlUpdate(conn, ds->string);

/* Clean up. */
remove(tabFileName);
sqlDisconnect(&conn);
}


void hgCtgPos(char *database, char *ooDir)
/* hgCtgPos - Store contig positions ( from lift files ) in database.. */
{
struct ctgPos *ctgList = NULL, *ctg;
char liftFileName[512];
struct fileInfo *fiList, *fi;
static char *liftNames[2] = {"lift/ordered.lft", "lift/random.lft"};
static char *chromSuffixes[2] = {"", "_random"};
int i;

fiList = listDirX(ooDir, "*", FALSE);
for (fi = fiList; fi != NULL; fi = fi->next)
    {
    if (fi->isDir && (strlen(fi->name) <= 2) || startsWith("NA_", fi->name))
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
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
hgCtgPos(argv[1], argv[2]);
return 0;
}
