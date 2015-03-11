/* hgLoadOutJoined - fixed loading RepeatMasker .out files. */
/* correctly load the id field, and new meaning for repStart, repEnd, repLeft */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "options.h"
#include "cheapcgi.h"
#include "hCommon.h"
#include "hdb.h"
#include "jksql.h"
#include "rmskOut2.h"


char *createRmskOut = "CREATE TABLE %s (\n"
"   bin smallint unsigned not null,     # bin index field for range queries\n"
"   swScore int unsigned not null,	# Smith Waterman alignment score\n"
"   milliDiv int unsigned not null,	# Base mismatches in parts per thousand\n"
"   milliDel int unsigned not null,	# Bases deleted in parts per thousand\n"
"   milliIns int unsigned not null,	# Bases inserted in parts per thousand\n"
"   genoName varchar(255) not null,	# Genomic sequence name\n"
"   genoStart int unsigned not null,	# Start in genomic sequence\n"
"   genoEnd int unsigned not null,	# End in genomic sequence\n"
"   genoLeft int not null,		# -#bases after match in genomic sequence\n"
"   strand char(1) not null,		# Relative orientation + or -\n"
"   repName varchar(255) not null,	# Name of repeat\n"
"   repClass varchar(255) not null,	# Class of repeat\n"
"   repFamily varchar(255) not null,	# Family of repeat\n"
"   repStart int not null,		# Start in repeat sequence - regardless of orient\n"
"   repEnd int not null,		# End in repeat sequence - regardless of orient\n"
"   repLeft int not null,		# -#bases after match in repeat sequence - regardless of orient\n"
"   id int not null,		# The ID of the hit. Used to link related fragments\n"
"             #Indices\n";

boolean noBin = FALSE;
char *tabFileName = NULL;
char *suffix = NULL;
int badRepCnt = 0;

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"tabFile", OPTION_STRING},
    {"tabfile", OPTION_STRING},
    {"table", OPTION_STRING},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadOutJoined - load new style (2014) RepeatMasker .out files into database\n"
  "usage:\n"
  "   hgLoadOutJoined database file(s).out\n"
  "For multiple files chrN.out this will create the single table 'rmskOutBaseline'\n"
  "in the database.\n"
  "options:\n"
  "   -tabFile=text.tab - don't actually load database, just create tab file\n"
  "   -table=name - use a different suffix other than the default (rmskOutBaseline)");
}

void badFormat(struct lineFile *lf, int id)
/* Print generic bad format message. */
{
errAbort("Badly formatted line %d in %s\n", lf->lineIx, lf->fileName);
}

int makeMilli(char *s, struct lineFile *lf)
/* Convert ascii floating point to parts per thousand representation. */
{
/* Cope with -0.0  and -0.2 etc.*/
if (s[0] == '-')
    {
    if (!sameString(s, "-0.0"))
        warn("Strange perc. field %s line %d of %s", s, lf->lineIx, lf->fileName);
    s = "0.0";
    }
if (!isdigit(s[0]))
    badFormat(lf,1);
return round(10.0*atof(s));
}

static int parenInt(char *s, struct lineFile *lf)
/* Convert from ascii to int where a parenthesized integer is the same */
{
if (s[0] == '(')
    return atoi(s+1);
else
    return atoi(s);
}

boolean checkRepeat(struct rmskOut2 *r, struct lineFile *lf)
/* check for bogus repeat */
{
/* this is bogus on both strands */
if (r->repStart > r->repEnd)
    {
    badRepCnt++;
    if (verboseLevel() > 1)
        {
        verbose(2, "bad rep range [%d, %d] line %d of %s %s:%d-%d\n",
		r->repStart, r->repEnd, lf->lineIx, lf->fileName, r->genoName, r->genoStart, r->genoEnd);
        }
    return FALSE;
    }
return TRUE;
}

FILE *theFile = NULL;
struct hash *chromFpHash = NULL;
char *defaultTempName = "rmskOutBaseline.tab";

FILE *getFileForChrom(char *chrom)
/* Return the appropriate file pointer for the given chrom */
{
char *tempName = tabFileName ? tabFileName : defaultTempName;

if (theFile == NULL)
    theFile = mustOpen(tempName, "w");

return theFile;
}

void helCarefulClose(struct hashEl *hel)
/* Call carefulClose on hashed file pointer. */
{
FILE *f = (FILE *)(hel->val);
carefulClose(&f);
}

void closeFiles()
/* Close the * single file pointer that we've been using. */
{
carefulClose(&theFile);
}

void readOneOut(char *rmskFile)
/* Read .out file rmskFile, check each line, and print OK lines to .tab. */
{
struct lineFile *lf;
char *line, *words[24];
int lineSize, wordCount;

/* Open .out file and process header. */
lf = lineFileOpen(rmskFile, TRUE);
if (!lineFileNext(lf, &line, &lineSize))
    errAbort("Empty %s", lf->fileName);
if (!startsWith("   SW  perc perc", line))
    {
    if (!startsWith("   SW   perc perc", line))
	errAbort("%s doesn't seem to be a RepeatMasker .out file, first "
	    "line seen:\n%s", lf->fileName, line);
    }
lineFileNext(lf, &line, &lineSize);
lineFileNext(lf, &line, &lineSize);

/* Process line oriented records of .out file. */
while (lineFileNext(lf, &line, &lineSize))
    {
    static struct rmskOut2 r;
    char *s;

    wordCount = chopLine(line, words);
    if (wordCount < 14)
        errAbort("Expecting 14 or 15 words line %d of %s", 
	    lf->lineIx, lf->fileName);
    r.swScore = atoi(words[0]);
    r.milliDiv = makeMilli(words[1], lf);
    r.milliDel = makeMilli(words[2], lf);
    r.milliIns = makeMilli(words[3], lf);
    r.genoName = words[4];
    r.genoStart = atoi(words[5])-1;
    r.genoEnd = atoi(words[6]);
    r.genoLeft = parenInt(words[7], lf);
    r.strand[0]  = (words[8][0] == '+' ? '+' : '-');
    r.repName = words[9];
    r.repClass = words[10];
    char *repClassTest = cloneString(r.repClass);
    stripChar(repClassTest, '(');
    stripChar(repClassTest, ')');
    int nonDigitCount = countLeadingNondigits(repClassTest);
    int wordOffset = 0;
    // this repClass is only digits, (or only (digits) with surrounding parens)
    //   this is the sign of an empty field here
    // due to custom library in use that has no class/family indication
    if (0 == nonDigitCount)
        {
        wordOffset = 1;
        r.repClass = cloneString("Unspecified");
        r.repFamily = cloneString("Unspecified");
        }
    else
        {
        s = strchr(r.repClass, '/');
        if (s == NULL)
            r.repFamily = r.repClass;
        else
           {
           *s++ = 0;
           r.repFamily = s;
           }
        }
    r.repStart = parenInt(words[11-wordOffset], lf);
    r.repEnd = atoi(words[12-wordOffset]);
    r.repLeft = parenInt(words[13-wordOffset], lf);
    r.id = atoi(words[14-wordOffset]);
    if (words[8][0] == 'C')
	{
	r.repLeft = parenInt(words[11-wordOffset], lf);
	r.repStart = parenInt(words[13-wordOffset], lf);
	}
    if (checkRepeat(&r, lf))
        {
	FILE *f = getFileForChrom(r.genoName);
        if (!noBin)
            fprintf(f, "%u\t", hFindBin(r.genoStart, r.genoEnd));
        rmskOut2TabOut(&r, f);
        }
    }
}

void loadOneTable(char *database, struct sqlConnection *conn, char *tempName, char *tableName)
/* Load .tab file tempName into tableName and remove tempName. */
{
struct dyString *query = newDyString(1024);

verbose(1, "Loading up table %s\n", tableName);
if (sqlTableExists(conn, tableName))
    {
    sqlDyStringPrintf(query, "DROP table %s", tableName);
    sqlUpdate(conn, query->string);
    }

/* Create first part of table definitions, the fields. */
dyStringClear(query);
sqlDyStringPrintf(query, createRmskOut, tableName);

/* Create the indexes */
int indexLen = hGetMinIndexLength(database);
sqlDyStringPrintf(query, "   INDEX(genoName(%d),bin))\n", indexLen);

sqlUpdate(conn, query->string);

/* Load database from tab-file. */
dyStringClear(query);
sqlDyStringPrintf(query, "LOAD data local infile '%s' into table %s",
	       tempName, tableName);
sqlUpdate(conn, query->string);
remove(tempName);
}

void processOneOut(char *database, struct sqlConnection *conn, char *rmskFile, char *suffix)
/* Read one RepeatMasker .out file and load it into database. */
{
verbose(1, "Processing %s\n", rmskFile);

readOneOut(rmskFile);

/* Create database table (if not -tabFile). */
if (tabFileName == NULL)
    {
    char dir[256], base[128], extension[64];
    char tableName[256];
    splitPath(rmskFile, dir, base, extension);
    chopSuffix(base);
    safef(tableName, sizeof(tableName), "%s_%s", base, suffix);
    closeFiles();
    loadOneTable(database, conn, defaultTempName, tableName);
    }
}

struct sqlConnection *theConn = NULL;

void hgLoadOutJoined(char *database, int rmskCount, char *rmskFileNames[], char *suffix)
/* hgLoadOutJoined - load RepeatMasker .out files into database. */
{
struct sqlConnection *conn = NULL;
int i;

if (tabFileName == NULL)
    {
    conn = hAllocConn(database);
    verbose(2,"#\thgLoadOutJoined: connected to database: %s\n", database);
    }
for (i=0; i<rmskCount; ++i)
    {
    readOneOut(rmskFileNames[i]);
    }
closeFiles();
if (tabFileName == NULL)
    {
    loadOneTable(database, conn, defaultTempName, suffix);
    }
hFreeConn(&conn);
if (badRepCnt > 0)
    {
    warn("note: %d records dropped due to repEnd < 0 or repStart > repEnd\n", badRepCnt);
    if (verboseLevel() < 2)
        warn("      run with -verbose=2 for details\n");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 3)
    usage();
suffix = optionVal("table", "rmskOutBaseline");
tabFileName = optionVal("tabFile", tabFileName);
if (tabFileName == NULL)
    tabFileName = optionVal("tabfile", tabFileName);
hgLoadOutJoined(argv[1], argc-2, argv+2, suffix) ;
return 0;
}
