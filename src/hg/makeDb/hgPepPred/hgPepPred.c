/* hgPepPred - Load peptide predictions from Ensembl or Genie. */
#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "hash.h"
#include "jksql.h"
#include "pepPred.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgPepPred - Load peptide predictions from Ensembl or Genie\n"
  "usage:\n"
  "   hgPepPred database type file(s)\n"
  "where type is either 'ensembl' or 'genie'");
}


void makeCustomTable(char *database, char *table, char *defString)
/* Make table if it doesn't exist already.  If it does exist clear it. */
{
struct sqlConnection *conn = sqlConnect(database);
struct dyString *ds = newDyString(2048);

dyStringPrintf(ds, defString, table);
sqlMaybeMakeTable(conn, table, ds->string);
dyStringClear(ds);
dyStringPrintf(ds, 
   "delete from %s", table);
sqlUpdate(conn, ds->string);
sqlDisconnect(&conn);
freeDyString(&ds);
}

void loadTableFromTabFile(char *database, char *table, char *tabFile)
/* Load up table from tab file. */
{
struct sqlConnection *conn = sqlConnect(database);
struct dyString *ds = newDyString(2048);
dyStringPrintf(ds, 
   "load data local infile '%s' into table %s", tabFile, table);
sqlUpdate(conn, ds->string);
sqlDisconnect(&conn);
freeDyString(&ds);
}

char *createString = 
"CREATE TABLE %s (\n"
"    name varchar(255) not null,	# Name of gene - same as in genePred\n"
"    seq longblob not null,	# Peptide sequence\n"
"              #Indices\n"
"    PRIMARY KEY(name)\n"
")\n";

char *findEnsTrans(struct lineFile *lf, char *line)
/* Find transcript name out of ensemble line.  Squawk and die
 * if a problem. */
{
char *words[32];
int wordCount, i;

wordCount = chopLine(line+1, words);
for (i=0; i<wordCount; ++i)
    {
    if (startsWith("trans:", words[i]))
        return words[i] + 6;
    }
errAbort("Couldn't find 'trans:' key for transcript name line %d of %s",
    lf->lineIx, lf->fileName);
return NULL;
}

void oneEnsFile(char *ensFile, struct hash *uniq, FILE *f)
/* Process one ensemble peptide prediction file into tab delimited
 * output f, using iniq hash to make sure no dupes. */
{
struct lineFile *lf = lineFileOpen(ensFile, TRUE);
char *line;
int lineSize;
boolean firstTime = TRUE;
char *trans;

/* Do cursory sanity check. */
if (!lineFileNext(lf, &line, &lineSize))
    errAbort("%s is empty", ensFile);
if (line[0] != '>')
    errAbort("%s is badly formatted, doesn't begin with '>'", ensFile);
lineFileReuse(lf);

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '>')
        {
	/* End last line. */
	if (firstTime)
	    firstTime = FALSE;
	else
	    fputc('\n', f);
	trans = findEnsTrans(lf, line);
	if (hashLookup(uniq, trans) != NULL)
	    errAbort("Duplicate %s line %d of %s", trans, lf->lineIx, lf->fileName);
	hashAdd(uniq, trans, NULL);
	fprintf(f, "%s\t", trans);
	}
    else
        {
	mustWrite(f, line, lineSize-1);
	}
    }
fputc('\n', f);
lineFileClose(&lf);
}

int ensPepPred(char *database, int fileCount, char *fileNames[])
/* Load Ensembl peptide predictions into database. */
{
int i;
char *fileName;
char *tempName = "ensPep.tab";
FILE *f = mustOpen(tempName, "w");
struct hash *uniq = newHash(16);

makeCustomTable(database, "ensPep", createString);
for (i=0; i<fileCount; ++i)
    {
    fileName = fileNames[i];
    printf("Processing %s\n", fileName);
    oneEnsFile(fileName, uniq, f);
    }
carefulClose(&f);
loadTableFromTabFile(database, "ensPep", tempName);
freeHash(&uniq);
}


void oneGenieFile(char *fileName, struct hash *uniq, FILE *known, FILE *alt)
/* Process one genie peptide prediction file into known and alt tab files. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
int lineSize;
boolean firstTime = TRUE;
char *trans;
FILE *f = NULL;
boolean skip = FALSE;

/* Do cursory sanity check. */
if (!lineFileNext(lf, &line, &lineSize))
    errAbort("%s is empty", fileName);
if (line[0] != '>')
    errAbort("%s is badly formatted, doesn't begin with '>'", fileName);
lineFileReuse(lf);

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '>')
        {
	/* End last line. */
	if (firstTime)
	    firstTime = FALSE;
	else
	    fputc('\n', f);
	trans = firstWordInLine(line+1);
	if (hashLookup(uniq, trans) != NULL)
	    {
	    warn("Duplicate %s line %d of %s. Ignoring all but first.", trans, lf->lineIx, lf->fileName);
	    skip = TRUE;
	    }
	else
	    {
	    hashAdd(uniq, trans, NULL);
	    if (startsWith("RS.", trans) || startsWith("C.", trans) || startsWith("AK.", trans))
		f = known;
	    else if (startsWith("A.", trans) || startsWith("S.", trans))
		f = alt;
	    else
		errAbort("Unrecognized name type %s line %d of %s", trans, lf->lineIx, lf->fileName);
	    fprintf(f, "%s\t", trans);
	    skip = FALSE;
	    }
	}
    else if (!skip)
        {
	mustWrite(f, line, lineSize-1);
	}
    }
fputc('\n', alt);
fputc('\n', known);
lineFileClose(&lf);
}

void geniePepPred(char *database, int fileCount, char *fileNames[])
/* Load Genie peptide predictions into database. */
{
char *knownTab = "known.tab";
char *altTab = "alt.tab";
FILE *known = mustOpen(knownTab, "w");
FILE *alt =  mustOpen(altTab, "w");
struct hash *uniq = newHash(16);
int i;
char *fileName;

makeCustomTable(database, "genieKnownPep", createString);
makeCustomTable(database, "genieAltPep", createString);
for (i=0; i<fileCount; ++i)
    {
    fileName = fileNames[i];
    printf("Processing %s\n", fileName);
    oneGenieFile(fileName, uniq, known, alt);
    }
carefulClose(&known);
carefulClose(&alt);
loadTableFromTabFile(database, "genieKnownPep", knownTab);
loadTableFromTabFile(database, "genieAltPep", altTab);
freeHash(&uniq);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *type;
if (argc < 4)
    usage();
type = argv[2];
if (sameWord(type, "ensembl"))
    ensPepPred(argv[1], argc-3, argv+3);
else if (sameWord(type, "genie"))
    geniePepPred(argv[1], argc-3, argv+3);
else
    usage();
return 0;
}
