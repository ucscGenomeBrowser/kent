/* hgLoadBed - Load a generic bed file into database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "dystring.h"
#include "bed.h"
#include "hdb.h"

/* Command line switches. */
boolean noBin = FALSE;		/* Suppress bin field. */


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadBed - Load a generic bed file into database\n"
  "usage:\n"
  "   hgLoadBed database track files(s).bed\n"
  "options:\n"
  "   -nobin   suppress bin field\n"
  );
}

int findBedSize(char *fileName)
/* Read first line of file and figure out how many words in it. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[32];
int wordCount;
wordCount = lineFileChop(lf, words);
if (wordCount == 0)
    errAbort("%s appears to be empty", fileName);
lineFileClose(&lf);
return wordCount;
}

void loadOneBed(char *fileName, int bedSize, struct bed **pList)
/* Load one bed file.  Make sure all lines have bedSize fields.
 * Put results in *pList. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[32];
int wordCount;
struct bed *bed;

printf("Reading %s\n", fileName);
while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    lineFileExpectWords(lf, bedSize, wordCount);
    bed = bedLoadN(words, wordCount);
    slAddHead(pList, bed);
    }
lineFileClose(&lf);
}

void writeBedTab(char *fileName, struct bed *bedList, int bedSize)
/* Write out bed list to tab-separated file. */
{
struct bed *bed;
FILE *f = mustOpen(fileName, "w");
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (!noBin)
        fprintf(f, "%u\t", hFindBin(bed->chromStart, bed->chromEnd));
    bedTabOutN(bed, bedSize, f);
    }
fclose(f);
}

void loadDatabase(char *database, char *track, int bedSize, struct bed *bedList)
/* Load database from bedList. */
{
struct sqlConnection *conn = sqlConnect(database);
struct dyString *dy = newDyString(1024);
char *tab = "bed.tab";

printf("Creating table definition for \n");
dyStringPrintf(dy, "CREATE TABLE %s (\n", track);
if (!noBin)
   dyStringAppend(dy, "  bin smallint unsigned not null,\n");
dyStringAppend(dy, "  chrom varchar(255) not null,\n");
dyStringAppend(dy, "  chromStart int unsigned not null,\n");
dyStringAppend(dy, "  chromEnd int unsigned not null,\n");
if (bedSize >= 4)
   dyStringAppend(dy, "  name varchar(255) not null,\n");
if (bedSize >= 5)
   dyStringAppend(dy, "  score int unsigned not null,\n");
if (bedSize >= 6)
   dyStringAppend(dy, "  strand char(1) not null,\n");
if (bedSize >= 7)
   dyStringAppend(dy, "  thickStart int unsigned not null,\n");
if (bedSize >= 8)
   dyStringAppend(dy, "  thickEnd int unsigned not null,\n");
if (bedSize >= 9)
   dyStringAppend(dy, "  reserved int unsigned  not null,\n");
if (bedSize >= 10)
   dyStringAppend(dy, "  blockCount int unsigned not null,\n");
if (bedSize >= 11)
   dyStringAppend(dy, "  blockSizes longblob not null,\n");
if (bedSize >= 12)
   dyStringAppend(dy, "  chromStarts longblob not null,\n");
dyStringAppend(dy, "#Indices\n");
if (!noBin)
   dyStringAppend(dy, "  INDEX(chrom(8),bin),\n");
if (bedSize >= 4)
   dyStringAppend(dy, "  INDEX(name(16)),\n");
dyStringAppend(dy, "  INDEX(chrom(8),chromStart),\n");
dyStringAppend(dy, "  INDEX(chrom(8),chromEnd)\n");
dyStringAppend(dy, ")\n");
sqlRemakeTable(conn, track, dy->string);

printf("Saving %s\n", tab);
writeBedTab(tab, bedList, bedSize);

printf("Loading %s\n", database);
dyStringClear(dy);
dyStringPrintf(dy, "load data local infile '%s' into table %s", tab, track);
sqlUpdate(conn, dy->string);
sqlDisconnect(&conn);
}

void hgLoadBed(char *database, char *track, int bedCount, char *bedFiles[])
/* hgLoadBed - Load a generic bed file into database. */
{
int bedSize = findBedSize(bedFiles[0]);
struct bed *bedList = NULL, *bed;
int i;

for (i=0; i<bedCount; ++i)
    loadOneBed(bedFiles[i], bedSize, &bedList);
printf("Loaded %d elements\n", slCount(bedList));
slSort(&bedList, bedLineCmp);
loadDatabase(database, track, bedSize, bedList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc < 4)
    usage();
hgLoadBed(argv[1], argv[2], argc-3, argv+3);
return 0;
}
