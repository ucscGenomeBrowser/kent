/* hgLoadOut - load RepeatMasker .out files into database. */
#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "options.h"
#include "cheapcgi.h"
#include "hCommon.h"
#include "hdb.h"
#include "jksql.h"
#include "rmskOut.h"

static char const rcsid[] = "$Id: hgLoadOut.c,v 1.20 2008/10/07 18:44:26 braney Exp $";

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
"   repStart int not null,		# Start (if strand is +) or -#bases after match (if strand is -) in repeat sequence\n"
"   repEnd int not null,		# End in repeat sequence\n"
"   repLeft int not null,		# -#bases after match (if strand is +) or start (if strand is -) in repeat sequence\n"
"   id char(1) not null,		# First digit of id field in RepeatMasker .out file.  Best ignored.\n"
"             #Indices\n";

boolean noBin = FALSE;
boolean split = FALSE;
boolean noSplit = FALSE;
char *tabFileName = NULL;
char *suffix = NULL;
int badRepCnt = 0;

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"tabFile", OPTION_STRING},
    {"tabfile", OPTION_STRING},
    {"nosplit", OPTION_BOOLEAN},
    {"noSplit", OPTION_BOOLEAN},
    {"split",   OPTION_BOOLEAN},
    {"table", OPTION_STRING},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadOut - load RepeatMasker .out files into database\n"
  "usage:\n"
  "   hgLoadOut database file(s).out\n"
  "For each table chrN.out this will create the table\n"
  "chrN_rmsk in the database\n"
  "options:\n"
  "   -tabFile=text.tab - don't actually load database, just create tab file\n"
  "   -nosplit - assume single rmsk table rather than chrN_rmsks\n"
  "   -split - load chrN_rmsk tables even if a single file is given\n"
  "   -table=name - use a different suffix other than the default (rmsk)\n");
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

int parenSignInt(char *s, struct lineFile *lf)
/* Convert from ascii notation where a parenthesized integer is negative
 * to straight int. */
{
if (s[0] == '(')
    return -atoi(s+1);
else
    return atoi(s);
}

boolean checkRepeat(struct rmskOut *r, struct lineFile *lf)
/* check for bogus repeat */
{
/* this is bogus on both strands */
if (r->repStart > r->repEnd)
    {
    badRepCnt++;
    if (verboseLevel() > 1)
        {
        verbose(2, "bad rep range [%d, %d] line %d of %s \n",
		r->repStart, r->repEnd, lf->lineIx, lf->fileName);
        }
    return FALSE;
    }
return TRUE;
}

FILE *theFile = NULL;
struct hash *chromFpHash = NULL;
char *defaultTempName = "rmsk.tab";

FILE *getFileForChrom(char *chrom)
/* Return the appropriate file pointer for the given chrom if -split.
 * If not using -split, then there is only one file pointer in action. */
{
static int chromCount = 0;
char *tempName = tabFileName ? tabFileName : defaultTempName;

if (split)
    {
    if (chromFpHash == NULL)
	chromFpHash = hashNew(10);
    theFile = (FILE *)hashFindVal(chromFpHash, chrom);
    if (theFile == NULL)
	{
	char fName[512];
	if (chromCount > 1000)
	    {
	    /* We will probably run into the operating system limit on open
	     * files well before this point, but just in case... */
	    errAbort("-split should not be used when there are >1000 sequences"
		     " -- too many split tables.");
	    }
	safef(fName, sizeof(fName), "%s_%s", chrom, tempName);
	theFile = mustOpen(fName, "w");
	hashAdd(chromFpHash, chrom, theFile);
	chromCount++;
	}
    }
else
    {
    if (theFile == NULL)
	theFile = mustOpen(tempName, "w");
    }
return theFile;
}

void helCarefulClose(struct hashEl *hel)
/* Call carefulClose on hashed file pointer. */
{
FILE *f = (FILE *)(hel->val);
carefulClose(&f);
}

void closeFiles()
/* If split, close each file pointer in chromFpHash.  Otherwise close the
 * single file pointer that we've been using. */
{
if (split)
    {
    hashTraverseEls(chromFpHash, helCarefulClose);
    }
else
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
    errAbort("%s doesn't seem to be a RepeatMasker .out file", lf->fileName);
lineFileNext(lf, &line, &lineSize);
lineFileNext(lf, &line, &lineSize);

/* Process line oriented records of .out file. */
while (lineFileNext(lf, &line, &lineSize))
    {
    static struct rmskOut r;
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
    r.genoLeft = parenSignInt(words[7], lf);
    r.strand[0]  = (words[8][0] == '+' ? '+' : '-');
    r.repName = words[9];
    r.repClass = words[10];
    s = strchr(r.repClass, '/');
    if (s == NULL)
        r.repFamily = r.repClass;
    else
       {
       *s++ = 0;
       r.repFamily = s;
       }
    r.repStart = parenSignInt(words[11], lf);
    r.repEnd = atoi(words[12]);
    r.repLeft = parenSignInt(words[13], lf);
    r.id[0] = ((wordCount > 14) ? words[14][0] : ' ');
    if (checkRepeat(&r, lf))
        {
	FILE *f = getFileForChrom(r.genoName);
        if (!noBin)
            fprintf(f, "%u\t", hFindBin(r.genoStart, r.genoEnd));
        rmskOutTabOut(&r, f);
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
    dyStringPrintf(query, "DROP table %s", tableName);
    sqlUpdate(conn, query->string);
    }

/* Create first part of table definitions, the fields. */
dyStringClear(query);
dyStringPrintf(query, createRmskOut, tableName);

/* Create the indexes */
if (!noSplit)
    {
    dyStringAppend(query, "   INDEX(bin))\n");
    }
else
    {
    int indexLen = hGetMinIndexLength(database);
    dyStringPrintf(query, "   INDEX(genoName(%d),bin))\n", indexLen);
    }

sqlUpdate(conn, query->string);

/* Load database from tab-file. */
dyStringClear(query);
dyStringPrintf(query, "LOAD data local infile '%s' into table %s",
	       tempName, tableName);
sqlUpdate(conn, query->string);
remove(tempName);
}

void processOneOut(char *database, struct sqlConnection *conn, char *rmskFile, char *suffix)
/* Read one RepeatMasker .out file and load it into database. */
{
verbose(1, "Processing %s\n", rmskFile);

if (split || noSplit)
    errAbort("program error: processOneOut doesn't do -split or -nosplit.");

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

void loadOneSplitTable(struct hashEl *hel)
/* Load the table for the given chrom. */
{
char tempName[512];
char tableName[256];
char *chrom = hel->name;
safef(tempName, sizeof(tempName), "%s_%s", chrom, defaultTempName);
safef(tableName, sizeof(tableName), "%s_%s", chrom, suffix);
loadOneTable(sqlGetDatabase(theConn), theConn, tempName, tableName);
}

void loadSplitTables(char *database, struct sqlConnection *conn)
/* For each chrom in chromHash, load its tempfile into table chrom_suffix. */
{
theConn = conn;
hashTraverseEls(chromFpHash, loadOneSplitTable);
}


void hgLoadOut(char *database, int rmskCount, char *rmskFileNames[], char *suffix)
/* hgLoadOut - load RepeatMasker .out files into database. */
{
struct sqlConnection *conn = NULL;
int i;

if (tabFileName == NULL)
    {
    conn = hAllocConn(database);
    verbose(2,"#\thgLoadOut: connected to database: %s\n", database);
    }
for (i=0; i<rmskCount; ++i)
    {
    if (split || noSplit)
	readOneOut(rmskFileNames[i]);
    else
	processOneOut(database, conn, rmskFileNames[i], suffix);
    }
closeFiles();
if (tabFileName == NULL)
    {
    if (split)
	loadSplitTables(database, conn);
    else if (noSplit)
	loadOneTable(database, conn, defaultTempName, suffix);
    }
hFreeConn(&conn);
if (badRepCnt > 0)
    {
    warn("note: %d records dropped due to repStart > repEnd\n", badRepCnt);
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
noSplit = (optionExists("noSplit") || optionExists("nosplit"));
split = optionExists("split");
suffix = optionVal("table", "rmsk");
tabFileName = optionVal("tabFile", tabFileName);
if (tabFileName == NULL)
    tabFileName = optionVal("tabfile", tabFileName);
if (split && noSplit)
    errAbort("-split and -nosplit cannot be used together.");
hgLoadOut(argv[1], argc-2, argv+2, suffix) ;
return 0;
}
