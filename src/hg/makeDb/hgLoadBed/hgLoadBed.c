/* hgLoadBed - Load a generic bed file into database. */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "obscure.h"
#include "hash.h"
#include "jksql.h"
#include "dystring.h"
#include "bed.h"
#include "hdb.h"
#include "hgRelate.h"
#include "portable.h"

static char const rcsid[] = "$Id: hgLoadBed.c,v 1.66 2008/12/04 17:21:13 mikep Exp $";

/* Command line switches. */
boolean noSort = FALSE;		/* don't sort */
boolean noBin = FALSE;		/* Suppress bin field. */
boolean hasBin = FALSE;		/* Input bed file includes bin. */
boolean strictTab = FALSE;	/* Separate on tabs. */
boolean oldTable = FALSE;	/* Don't redo table. */
boolean noLoad = FALSE;		/* Do not load DB or remove tab file */
boolean noHistory = FALSE;	/* Do not add history table comments */
boolean itemRgb = TRUE;		/* parse field nine as r,g,b when commas seen */
boolean notItemRgb = FALSE;	/* do NOT parse field nine as r,g,b */
boolean noStrict = FALSE;	/* skip the coord sanity checks */
int minScore = 0;		/* minimum score for fillInScore option */
int bedGraph = 0;		/* bedGraph column option, non-zero means yes */
char *sqlTable = NULL;		/* Read table from this .sql if non-NULL. */
boolean renameSqlTable = FALSE;	/* Rename table created with -sqlTable to */
                                /*     to match track */
boolean trimSqlTable = FALSE;   /* If we're loading fewer columns than defined */
                                /* in the SQL table, trim off extra columns */
int maxChromNameLength = 0;	/* specify to avoid chromInfo */
char *tmpDir = (char *)NULL;	/* location to create a temporary file */
boolean nameIx = TRUE;	        /* FALSE == do not create the name index */
boolean ignoreEmpty = FALSE;	/* TRUE == empty input files are not an error */
boolean allowNegativeScores = FALSE;	/* TRUE == score column set to int */
char *fillInScoreColumn = NULL;      /* column to use to fill-in score column */
boolean customTrackLoader = FALSE; /*TRUE == turn on all custom track options
                                    * turns on: noNameIx, ignoreEmpty, allowNegativeScores
                                    * -verbose=0 */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"noSort", OPTION_BOOLEAN},
    {"noBin", OPTION_BOOLEAN},
    {"nobin", OPTION_BOOLEAN},
    {"oldTable", OPTION_BOOLEAN},
    {"onServer", OPTION_BOOLEAN},
    {"sqlTable", OPTION_STRING},
    {"renameSqlTable", OPTION_BOOLEAN},
    {"trimSqlTable", OPTION_BOOLEAN},
    {"tab", OPTION_BOOLEAN},
    {"hasBin", OPTION_BOOLEAN},
    {"noLoad", OPTION_BOOLEAN},
    {"noHistory", OPTION_BOOLEAN},
    {"bedGraph", OPTION_INT},
    {"minScore", OPTION_INT},
    {"notItemRgb", OPTION_BOOLEAN},
    {"noStrict", OPTION_BOOLEAN},
    {"nostrict", OPTION_BOOLEAN},
    {"maxChromNameLength", OPTION_INT},
    {"tmpDir", OPTION_STRING},
    {"noNameIx", OPTION_BOOLEAN},
    {"ignoreEmpty", OPTION_BOOLEAN},
    {"allowNegativeScores", OPTION_BOOLEAN},
    {"customTrackLoader", OPTION_BOOLEAN},
    {"fillInScore", OPTION_STRING},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadBed - Load a generic bed file into database\n"
  "usage:\n"
  "   hgLoadBed database track files(s).bed\n"
  "options:\n"
  "   -noSort  don't sort (you better be sorting before this)\n"
  "   -noBin   suppress bin field\n"
  "   -oldTable add to existing table\n"
  "   -onServer This will speed things up if you're running in a directory that\n"
  "             the mysql server can access.\n"
  "   -sqlTable=table.sql Create table from .sql file\n"
  "   -renameSqlTable Rename table created with -sqlTable to match track\n"
  "   -trimSqlTable  if sqlTable has n rows, and input has m rows, only load m rows, meaning the last n-m rows in the sqlTable are optional\n"
  "   -tab  Separate by tabs rather than space\n"
  "   -hasBin   Input bed file starts with a bin field.\n"
  "   -noLoad  - Do not load database and do not clean up tab files\n"
  "   -noHistory  - Do not add history table comments (for custom tracks)\n"
  "   -notItemRgb  - Do not parse column nine as r,g,b when commas seen (bacEnds)\n"
  "   -bedGraph=N - wiggle graph column N of the input file as float dataValue\n"
  "               - bedGraph N is typically 4: -bedGraph=4\n"
  "   -maxChromNameLength=N  - specify max chromName length to avoid\n"
  "               - reference to chromInfo table\n"
  "   -tmpDir=<path>  - path to directory for creation of temporary .tab file\n"
  "                   - which will be removed after loading\n"
  "   -noNameIx  - no index for the name column (default creates index)\n"
  "   -ignoreEmpty  - no error on empty input file\n"
  "   -noStrict  - don't perform coord sanity checks\n"
  "              - by default we abort when: chromStart > chromEnd\n"
  "   -allowNegativeScores  - sql definition of score column is int, not unsigned\n"
  "   -customTrackLoader  - turns on: -noNameIx, -noHistory, -ignoreEmpty,\n"
  "                         -allowNegativeScores -verbose=0\n"
  "   -fillInScore=colName - if every score value is zero, then use column 'colName' to fill in the score column (from minScore-1000)\n"
  "   -minScore=N - minimum value for score field for -fillInScore option (default 100)\n"
  "   -verbose=N - verbose level for extra information to STDERR\n"
  );
}

int findBedSize(char *fileName, struct lineFile **retLf)
/* Read first line of file and figure out how many words in it. */
/* Input file could be stdin, in which case we really don't want to open,
 * read, and close it here.  So if retLf is non-NULL, return the open 
 * linefile (having told it to reuse the line we just read). */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[64], *line;
int wordCount;

if (!lineFileNextReal(lf, &line))
    if (ignoreEmpty)
        return(0);
line = cloneString(line);
if (strictTab)
    wordCount = chopTabs(line, words);
else
    wordCount = chopLine(line, words);
if (wordCount == 0)
    errAbort("%s appears to be empty", fileName);
if (retLf != NULL)
    {
    lineFileReuse(lf);
    *retLf = lf;
    }
else
    lineFileClose(&lf);
freeMem(line);
return wordCount;
}

struct bedStub
/* A line in a bed file with chromosome, start, end position parsed out. */
    {
    struct bedStub *next;	/* Next in list. */
    char *chrom;                /* Chromosome . */
    int chromStart;             /* Start position. */
    int chromEnd;		/* End position. */
    char *line;                 /* Line. */
    };

int bedStubCmp(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct bedStub *a = *((struct bedStub **)va);
const struct bedStub *b = *((struct bedStub **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
return dif;
}

void loadOneBed(struct lineFile *lf, int bedSize, struct bedStub **pList)
/* Load one bed file.  Make sure all lines have bedSize fields.
 * Put results in *pList. */
{
char *words[64], *line, *dupe;
int wordCount;
struct bedStub *bed;

verbose(1, "Reading %s\n", lf->fileName);
while (lineFileNextReal(lf, &line))
    {
    if (hasBin)
	nextWord(&line);
    dupe = cloneString(line);
    if (strictTab)
	wordCount = chopTabs(line, words);
    else
	wordCount = chopLine(line, words);
    /* ignore empty lines	*/
    if (0 == wordCount)
	continue;
    lineFileExpectWords(lf, bedSize, wordCount);
    AllocVar(bed);
    bed->chrom = cloneString(words[0]);
    bed->chromStart = lineFileNeedNum(lf, words, 1);
    bed->chromEnd = lineFileNeedNum(lf, words, 2);
    if (! noStrict)
	{
	if (bed->chromEnd < 1)
	    errAbort("ERROR: line %d:'%s'\nchromEnd is less than 1\n",
		     lf->lineIx, dupe);
	if (bed->chromStart > bed->chromEnd)
	    errAbort("ERROR: line %d:'%s'\nchromStart after chromEnd (%d > %d)\n",
		     lf->lineIx, dupe, bed->chromStart, bed->chromEnd);
	}
    bed->line = dupe;
    slAddHead(pList, bed);
    }
}

#define writeFailed(fileName) { errAbort("Write to %s failed -- disk full?", fileName); }


void writeBedTab(char *fileName, struct bedStub *bedList, int bedSize)
/* Write out bed list to tab-separated file. */
{
struct bedStub *bed;
FILE *f = mustOpen(fileName, "w");
char *words[64];
int i, wordCount;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (!noBin)
        if (fprintf(f, "%u\t", hFindBin(bed->chromStart, bed->chromEnd)) <= 0)
	    writeFailed(fileName);
    if (strictTab)
	wordCount = chopTabs(bed->line, words);
    else
	wordCount = chopLine(bed->line, words);
    for (i=0; i<wordCount; ++i)
        {
	/*	new definition for old "reserved" field, now itemRgb */
	/*	and when itemRgb, it is a comma separated string r,g,b */
	if (itemRgb && (i == 8))
	    {
	    char *comma;
	    /*  Allow comma separated list of rgb values here   */
	    comma = strchr(words[8], ',');
	    if (comma)
		{
		int itemRgb = 0;
		if (-1 == (itemRgb = bedParseRgb(words[8])))
		    errAbort("ERROR: expecting r,g,b specification, "
				"found: '%s'", words[8]);
		else
		    if (fprintf(f, "%d", itemRgb) <= 0)
			writeFailed(fileName);

		verbose(2, "itemRgb: %s, rgb: %#x\n", words[8], itemRgb);
		}
	    else
		if (fputs(words[i], f) == EOF)
		    writeFailed(fileName);
	    }
	else
	    if (fputs(words[i], f) == EOF)
		writeFailed(fileName);

	if (i == wordCount-1)
	    {
	    if (fputc('\n', f) == EOF)
		writeFailed(fileName);
	    }
	else
	    if (fputc('\t', f) == EOF)
		writeFailed(fileName);
	}
    }
fclose(f);
}

static void maybeBedGraph(int col, struct dyString *dy, char *colDefinition)
/* output definition "default" for column "col" when not bedGraph */
{
if (col == bedGraph)
    dyStringAppend(dy, "  dataValue float not null,\n");
else
    dyStringAppend(dy, colDefinition);
}

static boolean colAlreadyThere(struct sqlConnection *conn, char *tableName, char *col)
/* Check to see if there's a field in the table already */
{
char existsSql[128];
safef(existsSql, sizeof(existsSql), "desc %s %s", tableName, col);
return sqlExists(conn, existsSql);
}

static void addBinToEmptyTable(struct sqlConnection *conn, char *tableName)
/* Add bin field and corresponding index. */
{
if (!colAlreadyThere(conn, tableName, "bin"))
    {
    char addBinSql[256];
    char addIndexSql[256];
    safef(addBinSql, sizeof(addBinSql), "alter table %s add column bin smallint unsigned not null first;", tableName);
    sqlUpdate(conn, addBinSql);
    /* add related index */
    if (colAlreadyThere(conn, tableName, "chrom"))
	safef(addIndexSql, sizeof(addIndexSql), "create index binChrom on %s (chrom,bin)", tableName);
    else
	safef(addIndexSql, sizeof(addIndexSql), "create index bin on %s (bin)", tableName);
    sqlUpdate(conn, addIndexSql);
    }
}

static void adjustSqlTableColumns(struct sqlConnection *conn, char *tableName, int bedSize)
/* Go through and drop columns to fit the file it the -trimSqlTable option is used. */
/* Otherwise the data is unloadable. */
{
struct slName *fieldNames = sqlFieldNames(conn, tableName);
int numFields = slCount(fieldNames);
if (! noBin)
    numFields--;  // subtract bin column if sql includes it
if (numFields != bedSize)
    {
    struct slName *oneName;
    int i;
    if (!trimSqlTable || (bedSize > numFields))
	errAbort(".sql table has wrong number of columns in the definition. Try -trimSqlTable");
    slReverse(&fieldNames);
    for (oneName = fieldNames, i = 0; (i < numFields - bedSize) 
	     && (oneName != NULL); i++, oneName = oneName->next)
	{
	char query[256];
	safef(query, sizeof(query), "alter table %s drop column %s", tableName, oneName->name);
	sqlUpdate(conn, query);
	} 
    }
slFreeList(&fieldNames);
}

static void loadDatabase(char *database, char *track, int bedSize, struct bedStub *bedList)
/* Load database from bedList. */
{
struct sqlConnection *conn;
struct dyString *dy = newDyString(1024);
char *tab = (char *)NULL;
int loadOptions = (optionExists("onServer") ? SQL_TAB_FILE_ON_SERVER : 0);

if ( ! noLoad )
    conn = sqlConnect(database);

if ((char *)NULL != tmpDir)
    tab = cloneString(rTempName(tmpDir,"loadBed",".tab"));
else
    tab = cloneString("bed.tab");

/* First make table definition. */
if (sqlTable != NULL && !oldTable)
    {
    /* Read from file. */
    char *sql, *s;
    readInGulp(sqlTable, &sql, NULL);
    /* Chop off end-of-statement semicolon if need be. */
    s = strchr(sql, ';');
    if (s != NULL) *s = 0;
    
    if ( !noLoad )
        {
        if (renameSqlTable)
            {
            char *pos = stringIn("CREATE TABLE ", sql);
            if (pos == NULL)
                errAbort("Can't find CREATE TABLE in %s\n", sqlTable);
            char *oldSql = cloneString(sql);
            nextWord(&pos); nextWord(&pos);
            char *tableName = nextWord(&pos);
            sql = replaceChars(oldSql, tableName, track);
            }
        verbose(1, "Creating table definition for %s\n", track);
        sqlRemakeTable(conn, track, sql);
        if (!noBin) 
	    addBinToEmptyTable(conn, track);
	adjustSqlTableColumns(conn, track, bedSize);
	}
    
    freez(&sql);
    }
else if (!oldTable)
    {
    int minLength;

    if (noLoad)
	minLength=6;
    else if (maxChromNameLength)
	minLength = maxChromNameLength;
    else
	minLength = hGetMinIndexLength(database);
    verbose(2, "INDEX chrom length: %d\n", minLength);

    /* Create definition statement. */
    verbose(1, "Creating table definition for %s\n", track);
    dyStringPrintf(dy, "CREATE TABLE %s (\n", track);
    if (!noBin)
       dyStringAppend(dy, "  bin smallint unsigned not null,\n");
    dyStringAppend(dy, "  chrom varchar(255) not null,\n");
    dyStringAppend(dy, "  chromStart int unsigned not null,\n");
    dyStringAppend(dy, "  chromEnd int unsigned not null,\n");
    if (bedSize >= 4)
       maybeBedGraph(4, dy, "  name varchar(255) not null,\n");
    if (bedSize >= 5)
	{
	if (allowNegativeScores)
	    maybeBedGraph(5, dy, "  score int not null,\n");
	else
	    maybeBedGraph(5, dy, "  score int unsigned not null,\n");
	}
    if (bedSize >= 6)
       maybeBedGraph(6, dy, "  strand char(1) not null,\n");
    if (bedSize >= 7)
       maybeBedGraph(7, dy, "  thickStart int unsigned not null,\n");
    if (bedSize >= 8)
       maybeBedGraph(8, dy, "  thickEnd int unsigned not null,\n");
    /*	As of 2004-11-22 the reserved field is used as itemRgb in code */
    if (bedSize >= 9)
       maybeBedGraph(9, dy, "  reserved int unsigned  not null,\n");
    if (bedSize >= 10)
       maybeBedGraph(10, dy, "  blockCount int unsigned not null,\n");
    if (bedSize >= 11)
       maybeBedGraph(11, dy, "  blockSizes longblob not null,\n");
    if (bedSize >= 12)
       maybeBedGraph(12, dy, "  chromStarts longblob not null,\n");
    if (bedSize >= 13)
       maybeBedGraph(13, dy, "  expCount int unsigned not null,\n");
    if (bedSize >= 14)
       maybeBedGraph(14, dy, "  expIds longblob not null,\n");
    if (bedSize >= 15)
       maybeBedGraph(15, dy, "  expScores longblob not null,\n");
    dyStringAppend(dy, "#Indices\n");
    if (nameIx && (bedSize >= 4) && (0 == bedGraph))
       dyStringAppend(dy, "  INDEX(name(16)),\n");
    if (noBin)
	{
	dyStringPrintf(dy, "  INDEX(chrom(%d),chromStart)\n", minLength);
	}
    else
	{
        dyStringPrintf(dy, "  INDEX(chrom(%d),bin)\n", minLength);
	}
    dyStringAppend(dy, ")\n");
    if (noLoad)
	verbose(2,"%s", dy->string);
    else
	sqlRemakeTable(conn, track, dy->string);
    }

verbose(1, "Saving %s\n", tab);
writeBedTab(tab, bedList, bedSize);

if ( ! noLoad )
    {
    verbose(1, "Loading %s\n", database);
    if (customTrackLoader)
	sqlLoadTabFile(conn, tab, track, loadOptions|SQL_TAB_FILE_WARN_ON_WARN);
    else
	sqlLoadTabFile(conn, tab, track, loadOptions);

    if (! noHistory)
	{
	char comment[256];
	/* add a comment to the history table and finish up connection */
	safef(comment, sizeof(comment),
	    "Add %d element(s) from bed list to %s table",
		slCount(bedList), track);
	hgHistoryComment(conn, comment);
	}
    if(fillInScoreColumn != NULL)
        {
        char query[500];
        char buf[500];
        struct sqlResult *sr;
        safef(query, sizeof(query), "select sum(score) from %s", track);
        if(sqlQuickQuery(conn, query, buf, sizeof(buf)))
            {
            unsigned sum = sqlUnsigned(buf);
            if (!sum)
                {
                safef(query, sizeof(query), "select min(%s), max(%s) from %s", fillInScoreColumn, fillInScoreColumn, track);
                if ((sr = sqlGetResult(conn, query)) != NULL)
                    {
                    char **row = sqlNextRow(sr);
                    if(row != NULL)
                        {
                        float min = sqlFloat(row[0]);
                        float max = sqlFloat(row[1]);
			if (max == min || sameString(row[0],row[1])) // this will lead to 'inf' score value in SQL update causing an error
			    errAbort("Could not set score in table %s max(%s)=min(%s)=%s\n", track, fillInScoreColumn, fillInScoreColumn, row[0]);
                        sqlFreeResult(&sr);

                        // Calculate a, b s/t f(x) = ax + b maps min-max => minScore-1000
                        float a = (1000-minScore) / (max - min);
                        float b = 1000 - ((1000-minScore) * max) / (max - min);

                        safef(query, sizeof(query), "update %s set score = round((%f * %s) + %f)",  track, a, fillInScoreColumn, b);
                        int changed = sqlUpdateRows(conn, query, NULL);
                        verbose(2, "update query: %s; changed: %d\n", query, changed);
                        }
                    else
                        {
                        sqlFreeResult(&sr);
                        }
                    }
                }
            }

        }
    sqlDisconnect(&conn);
    /*	if temp dir specified, unlink file to make it disappear */
    if ((char *)NULL != tmpDir)
	unlink(tab);
    }
else
    verbose(1, "No load option selected, see file: %s\n", tab);

}	/*	static void loadDatabase()	*/

void hgLoadBed(char *database, char *track, int bedCount, char *bedFiles[])
/* hgLoadBed - Load a generic bed file into database. */
{
struct lineFile *lf = NULL;
int bedSize = findBedSize(bedFiles[0], &lf);
struct bedStub *bedList = NULL;
int i;
int loadedElementCount;

if ((0 == bedSize) && !ignoreEmpty)
    errAbort("empty input file for table %s.%s", database,track);
if ((0 == bedSize) && ignoreEmpty)
    return;

if (hasBin)
    bedSize--;

/*	verify proper usage of bedGraph column, can not be more than columns */
if ((bedGraph > 0) && (bedGraph > bedSize))
    errAbort("bedGraph column %d can not be past last column, last column is %d",
	bedGraph, bedSize);

for (i=0; i<bedCount; ++i)
    {
    /* bedFiles[0] was opened by findBedSize above -- since it might be stdin,
     * it's left open and reused by loadOneBed.  After that, open here: */
    if (i > 0)
	lf = lineFileOpen(bedFiles[i], TRUE);
    loadOneBed(lf, bedSize, &bedList);
    lineFileClose(&lf);
    }
loadedElementCount = slCount(bedList);
verbose(1, "Loaded %d elements of size %d\n", loadedElementCount, bedSize);
if (!noSort)
    {
    slSort(&bedList, bedStubCmp);
    verbose(1, "Sorted\n");
    }
else
    {
    verbose(1, "Not Sorting\n");
    slReverse(&bedList);
    }

if (loadedElementCount > 0)
    loadDatabase(database, track, bedSize, bedList);
else if (! ignoreEmpty)
    errAbort("empty input file for %s.%s", database,track);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 4)
    usage();
noBin = optionExists("noBin") || optionExists("nobin");
noSort = optionExists("noSort");
strictTab = optionExists("tab");
oldTable = optionExists("oldTable");
sqlTable = optionVal("sqlTable", sqlTable);
renameSqlTable = optionExists("renameSqlTable");
trimSqlTable = optionExists("trimSqlTable");
hasBin = optionExists("hasBin");
noLoad = optionExists("noLoad");
noHistory = optionExists("noHistory");
bedGraph = optionInt("bedGraph",0);
minScore = optionInt("minScore",100);
if (minScore<0 || minScore>1000)
    errAbort("minScore must be between 0-1000\n");
notItemRgb = optionExists("notItemRgb");
if (notItemRgb) itemRgb = FALSE;
maxChromNameLength = optionInt("maxChromNameLength",0);
noStrict = optionExists("noStrict") || optionExists("nostrict");
tmpDir = optionVal("tmpDir", tmpDir);
nameIx = ! optionExists("noNameIx");
ignoreEmpty = optionExists("ignoreEmpty");
allowNegativeScores = optionExists("allowNegativeScores");
customTrackLoader = optionExists("customTrackLoader");
/* turns on: noNameIx, ignoreEmpty, allowNegativeScores
 * -verbose=0 */
if (customTrackLoader)
    {
    ignoreEmpty = TRUE;
    noHistory = TRUE;
    nameIx = FALSE;
    allowNegativeScores = TRUE;
    verboseSetLevel(0);
    }
fillInScoreColumn = optionVal("fillInScore", NULL);
hgLoadBed(argv[1], argv[2], argc-3, argv+3);
return 0;
}
