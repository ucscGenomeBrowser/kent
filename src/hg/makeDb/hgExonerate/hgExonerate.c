/* hgExonerate - Convert Exonerate modified GFF files to BED format and load in database.. */
#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "jksql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgExonerate - Convert Exonerate modified GFF files to BED format and load in database.\n"
  "usage:\n"
  "   hgExonerate database table inputFile\n");
}

char *createString = "#A rough alignment - not detailed\n"
"CREATE TABLE %s (\n"
    "chrom varchar(255) not null,	# Human chromosome or FPC contig\n"
    "chromStart int unsigned not null,	# Start position in chromosome\n"
    "chromEnd int unsigned not null,	# End position in chromosome\n"
    "name varchar(255) not null,	# Name of other sequence\n"
    "score int unsigned not null,	# Score from 0 to 1000\n"
    "strand char(1) not null,	# + or -\n"
    "otherStart int unsigned not null,	# Start in other sequence\n"
    "otherEnd int unsigned not null,	# End in other sequence\n"
              "#Indices\n"
    "INDEX(chrom(12),chromStart),\n"
    "INDEX(chrom(12),chromEnd),\n"
    "INDEX(name(12))\n"
")\n";

void loadIntoDb(char *tabFile, char *database, char *table, boolean clear)
/* Load database table from tab file. */
{
struct sqlConnection *conn = sqlConnect(database);
struct dyString *dy = newDyString(2048);

dyStringPrintf(dy, createString, table);
sqlMaybeMakeTable(conn, table, dy->string);
if (clear)
    {
    dyStringClear(dy);
    dyStringPrintf(dy, "delete from %s", table);
    sqlUpdate(conn, dy->string);
    }
dyStringClear(dy);
dyStringPrintf(dy, "LOAD data local infile '%s' into table %s", tabFile, table);
sqlUpdate(conn, dy->string);
sqlDisconnect(&conn);
}

void hgExonerate(char *database, char *table, char *file)
/* hgExonerate - Convert Exonerate modified GFF files to BED format and load in database.. */
{
struct lineFile *lf = lineFileOpen(file, TRUE);
FILE *f;
char tabFileName[512];
char *row[8];
char *parts[6];
char *subParts[3];
int partCount;
double score;
int count = 0;

sprintf(tabFileName, "%s.tab", table);
f = mustOpen(tabFileName, "w");
printf("Converting %s to %s\n", file, tabFileName);
while (lineFileRow(lf, row))
    {
    ++count;
    fprintf(f, "%s\t", row[0]);				/* chromosome. */
    fprintf(f, "%d\t", lineFileNeedNum(lf, row, 2)-1);       /* chromStart. */
    fprintf(f, "%s\t", row[3]);                         /* chromEnd. */
    partCount = chopString(row[7], ".:", parts, ArraySize(parts));
    if (partCount != 4)
       errAbort("Unparsable field 8 line %d of %s", lf->lineIx, lf->fileName);
    fprintf(f, "%s.%s\t", parts[0], parts[1]);          /* name. */
    score = atof(row[4]);                               
    if (score > 1000) score = 1000;
    if (score < 0) score = 0;
    fprintf(f, "%d\t", round(score));			/* milliScore */
    fprintf(f, "%s\t", row[5]);				/* strand */
    partCount = chopByChar(parts[2], '-', subParts, ArraySize(subParts));
    if (partCount != 2 || !isdigit(subParts[0][0]) || !isdigit(subParts[1][0]))
       errAbort("Unparsable number range in field 8 line %d of %s", lf->lineIx, lf->fileName);
    fprintf(f, "%d\t", atoi(subParts[0])-1);		/* otherStart */
    fprintf(f, "%s\n", subParts[1]);			/* otherEnd */
    }
carefulClose(&f);
lineFileClose(&lf);

printf("Loading %d items into table %s in database %s\n", count, table, database);
loadIntoDb(tabFileName, database, table, TRUE);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
hgExonerate(argv[1], argv[2], argv[3]);
return 0;
}
