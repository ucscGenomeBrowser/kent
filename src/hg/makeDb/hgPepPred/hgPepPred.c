/* hgPepPred - Load peptide predictions from Ensembl or Genie. */
#include "common.h"
#include "cheapcgi.h"
#include "linefile.h"
#include "dystring.h"
#include "hash.h"
#include "jksql.h"
#include "pepPred.h"

static char const rcsid[] = "$Id: hgPepPred.c,v 1.10 2003/10/26 08:13:20 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgPepPred - Load peptide predictions from Ensembl or Genie\n"
  "usage:\n"
  "   hgPepPred database type file(s)\n"
  "where type is either 'ensembl' or 'genie' or 'softberry'\n"
  "or\n"
  "   hgPepPred database generic table files(s)\n"
  "which will load the given table assuming the peptide name is\n"
  "in the fasta record\n"
  "options:\n"
  "   -abbr=prefix  Abbreviate by removing prefix from names\n"
  );
}

char *abbr = NULL;


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
"    PRIMARY KEY(name(64))\n"
")\n";

char *findEnsTrans(struct lineFile *lf, char *line)
/* Find transcript name out of ensemble line.  Squawk and die
 * if a problem. */
{
char *words[32];
int wordCount, i;
char *pat = "Translation:";
int patSize = strlen(pat);

wordCount = chopLine(line+1, words);
for (i=0; i<wordCount; ++i)
    {
    if (startsWith(pat, words[i]))
        return words[i] + patSize;
    }
errAbort("Couldn't find '%s' key for transcript name line %d of %s",
    pat, lf->lineIx, lf->fileName);
return NULL;
}

void oneEnsFile(char *ensFile, struct hash *uniq, struct hash *pToT, FILE *f)
/* Process one ensemble peptide prediction file into tab delimited
 * output f, using uniq hash to make sure no dupes. */
{
struct lineFile *lf = lineFileOpen(ensFile, TRUE);
char *line;
int lineSize;
boolean firstTime = TRUE;
char *translation;

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
	char *transcript;
	/* End last line. */
	if (firstTime)
	    firstTime = FALSE;
	else
	    fputc('\n', f);
	translation = findEnsTrans(lf, line);
	if (hashLookup(uniq, translation) != NULL)
	    errAbort("Duplicate %s line %d of %s", translation, lf->lineIx, lf->fileName);
	hashAdd(uniq, translation, NULL);
	transcript = hashFindVal(pToT, translation);
	if (transcript == NULL)
	    errAbort("Can't find transcript for %s", translation);
	fprintf(f, "%s\t", transcript);
	}
    else
        {
	mustWrite(f, line, lineSize-1);
	}
    }
fputc('\n', f);
lineFileClose(&lf);
}

struct hash *ensProtToTrans(char *database)
/* Use ensGtp table to create hash keyed by protein and
 * returning transcript values. */
{
char *table = "ensGtp";
struct sqlConnection *conn = sqlConnect(database);
char query[256], **row;
struct sqlResult *sr;
struct hash *hash = newHash(16);

if (!sqlTableExists(conn, table))
     errAbort("No %s table, need to build that first", table);
safef(query, sizeof(query), "select protein,transcript from %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    chopSuffix(row[0]);
    chopSuffix(row[1]);
    hashAdd(hash, row[0], cloneString(row[1]));
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return hash;
}

void ensPepPred(char *database, int fileCount, char *fileNames[])
/* Load Ensembl peptide predictions into database. */
{
int i;
char *fileName;
char *tempName = "ensPep.tab";
FILE *f = mustOpen(tempName, "w");
struct hash *uniq = newHash(16);
struct hash *pToT = ensProtToTrans(database);

makeCustomTable(database, "ensPep", createString);
for (i=0; i<fileCount; ++i)
    {
    fileName = fileNames[i];
    printf("Processing %s\n", fileName);
    oneEnsFile(fileName, uniq, pToT, f);
    }
carefulClose(&f);
loadTableFromTabFile(database, "ensPep", tempName);
freeHash(&uniq);
}


void oneGenieFile(char *fileName, struct hash *uniq, FILE *f)
/* Process one genie peptide prediction file into known and alt tab files. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
int lineSize;
boolean firstTime = TRUE;
char *trans;
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
	if (abbr != NULL && startsWith(abbr, trans))
	    trans += strlen(abbr);
	if (hashLookup(uniq, trans) != NULL)
	    {
	    warn("Duplicate %s line %d of %s. Ignoring all but first.", trans, lf->lineIx, lf->fileName);
	    skip = TRUE;
	    }
	else
	    {
	    hashAdd(uniq, trans, NULL);
	    fprintf(f, "%s\t", trans);
	    skip = FALSE;
	    }
	}
    else if (!skip)
        {
	mustWrite(f, line, lineSize-1);
	}
    }
fputc('\n', f);
lineFileClose(&lf);
}

void geniePepPred(char *database, int fileCount, char *fileNames[])
/* Load Genie peptide predictions into database. */
{
char *altTab = "alt.tab";
FILE *alt =  mustOpen(altTab, "w");
struct hash *uniq = newHash(16);
int i;
char *fileName;

makeCustomTable(database, "genieAltPep", createString);
for (i=0; i<fileCount; ++i)
    {
    fileName = fileNames[i];
    printf("Processing %s\n", fileName);
    oneGenieFile(fileName, uniq, alt);
    }
carefulClose(&alt);
loadTableFromTabFile(database, "genieAltPep", altTab);
freeHash(&uniq);
}

void genericOne(char *fileName, struct hash *uniq, FILE *f)
/* Process one ensemble peptide prediction file into tab delimited
 * output f, using uniq hash to make sure no dupes. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
int lineSize;
boolean firstTime = TRUE;
char *trans;

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
	if (abbr != NULL && startsWith(abbr, trans))
	    trans += strlen(abbr);
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


void genericPepPred(char *database, int fileCount, char *fileNames[], char *table)
/* Load a generic (simple) peptide file. */
{
int i;
char *fileName;
char tempName[256];
FILE *f;
struct hash *uniq = newHash(16);

sprintf(tempName, "%s.tab", table);
f = mustOpen(tempName, "w");
makeCustomTable(database, table, createString);
for (i=0; i<fileCount; ++i)
    {
    fileName = fileNames[i];
    printf("Processing %s\n", fileName);
    genericOne(fileName, uniq, f);
    }
carefulClose(&f);
loadTableFromTabFile(database, table, tempName);
freeHash(&uniq);
}

void softberryPepPred(char *database, int fileCount, char *fileNames[])
/* Load Softberry peptide predictions into database. */
{
genericPepPred(database, fileCount, fileNames, "softberryPep");
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *type;
cgiSpoof(&argc, argv);
if (argc < 4)
    usage();
abbr = cgiUsualString("abbr", abbr);
type = argv[2];
if (sameWord(type, "ensembl"))
    ensPepPred(argv[1], argc-3, argv+3);
else if (sameWord(type, "genie"))
    geniePepPred(argv[1], argc-3, argv+3);
else if (sameWord(type, "softberry"))
    softberryPepPred(argv[1], argc-3, argv+3);
else if (sameWord(type, "generic"))
    genericPepPred(argv[1], argc-4, argv+4, argv[3]);
else
    usage();
return 0;
}
