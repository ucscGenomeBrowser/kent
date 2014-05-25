/* hgExonerate - Convert Exonerate modified GFF files to BED format and load in database.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "cheapcgi.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgExonerate - Convert Exonerate modified GFF files to BED format and load in database.\n"
  "usage:\n"
  "   hgExonerate database table inputFile\n"
  "options:\n"
  "   -elia    - handle Elia's 11 column GFF's");
}

char *createString = 
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
    "INDEX(chrom(8),chromStart),\n"
    "INDEX(chrom(8),chromEnd),\n"
    "INDEX(name(12))\n"
")\n";

void loadIntoDb(char *tabFile, char *database, char *table, boolean clear)
/* Load database table from tab file. */
{
struct sqlConnection *conn = sqlConnect(database);
struct dyString *dy = newDyString(2048);

sqlDyStringPrintf(dy, createString, table);
sqlMaybeMakeTable(conn, table, dy->string);
if (clear)
    {
    dyStringClear(dy);
    sqlDyStringPrintf(dy, "delete from %s", table);
    sqlUpdate(conn, dy->string);
    }
dyStringClear(dy);
sqlDyStringPrintf(dy, "LOAD data local infile '%s' into table %s", tabFile, table);
sqlUpdate(conn, dy->string);
sqlDisconnect(&conn);
}

struct hash *hashGroup(struct lineFile *lf, char *s)
/* Make a little hash from group field. */
{
struct hash *hash = newHash(4);
char *words[2], *e;
int wordCount;

for (;;)
    {
    s = skipLeadingSpaces(s);
    if (s == NULL || s[0] == 0)
        break;
    e = strchr(s, ';');
    if (e != NULL)
       *e++ = 0;
    wordCount = chopString(s, "=;", words, ArraySize(words));
    if (wordCount != 2)
       errAbort("malformed group line %d of %s\n", lf->lineIx, lf->fileName);
    hashAdd(hash, words[0], cloneString(words[1]));
    if (e == NULL)
       break;
    s = e;
    }
return hash;
}

void hgExonerate(char *database, char *table, char *file)
/* hgExonerate - Convert Exonerate modified GFF files to BED format and load in database.. */
{
struct lineFile *lf = lineFileOpen(file, TRUE);
char *line;
int lineSize;
FILE *f;
char tabFileName[512];
char *row[12];
char *parts[6];
char *subParts[3];
char *strand;
int partCount;
double score;
int count = 0;
boolean doElia = cgiBoolean("elia");
int wordCount;

sprintf(tabFileName, "%s.tab", table);
f = mustOpen(tabFileName, "w");
printf("Converting %s to %s\n", file, tabFileName);
while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
        continue;
    ++count;
    wordCount = chopTabs(line, row);
    if (wordCount < 9)
        errAbort("Expecting at least 9 words line %d of %s\n", lf->lineIx, lf->fileName);
    fprintf(f, "%s\t", row[0]);				/* chromosome. */
    fprintf(f, "%d\t", lineFileNeedNum(lf, row, 3)-1);       /* chromStart. */
    fprintf(f, "%s\t", row[4]);                         /* chromEnd. */
    score = atof(row[5]) * 0.70;                               
    if (score > 1000) score = 1000;
    if (score < 0) score = 0;
    strand = row[6];
    if (doElia)
        {
	int start, end;
	if (wordCount < 10)
	    errAbort("Expecting at least 10 words line %d of %s\n", lf->lineIx, lf->fileName);
	if (strand[0] == '-')
	    {
	    strand = "-";
	    start = atoi(row[9])-1;
	    end = atoi(row[8]);
	    }
	else
	    {
	    strand = "+";
	    start = atoi(row[8])-1;
	    end = atoi(row[9]);
	    }
	fprintf(f, "%s\t%d\t%s\t%d\t%d\n", row[7], round(score), strand, start, end);
	}
    else
	{
	if (startsWith("trace=", row[8]))
	    {
	    struct hash *hash = hashGroup(lf, row[8]);
	    char *trace = hashMustFindVal(hash, "trace");
	    int start = atoi(hashMustFindVal(hash, "hstart")) - 1;
	    int end = atoi(hashMustFindVal(hash, "hend"));
	    fprintf(f, "%s\t%d\t%s\t%d\t%d\n", trace, round(score), strand, start, end);
	    freeHashAndVals(&hash);
	    }
	else
	    {
	    partCount = chopString(row[8], ".:", parts, ArraySize(parts));
	    if (partCount != 4)
	       errAbort("Unparsable field 9 line %d of %s", lf->lineIx, lf->fileName);
	    fprintf(f, "%s.%s\t", parts[0], parts[1]);          /* name. */
	    fprintf(f, "%d\t", round(score));			/* milliScore */
	    fprintf(f, "%s\t", strand);				/* strand */
	    partCount = chopByChar(parts[2], '-', subParts, ArraySize(subParts));
	    if (partCount != 2 || !isdigit(subParts[0][0]) || !isdigit(subParts[1][0]))
	       errAbort("Unparsable number range in field 9 line %d of %s", lf->lineIx, lf->fileName);
	    fprintf(f, "%d\t", atoi(subParts[0])-1);		/* otherStart */
	    fprintf(f, "%s\n", subParts[1]);			/* otherEnd */
	    }
	}
    }
carefulClose(&f);
lineFileClose(&lf);

printf("Loading %d items into table %s in database %s\n", count, table, database);
loadIntoDb(tabFileName, database, table, TRUE);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
hgExonerate(argv[1], argv[2], argv[3]);
return 0;
}
