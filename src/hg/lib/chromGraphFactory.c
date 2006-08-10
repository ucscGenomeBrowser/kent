/* chromGraphFactory - turn user-uploaded track in chromGraph format
 * into a customTrack. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "errCatch.h"
#include "jksql.h"
#include "hdb.h"
#include "chromInfo.h"
#include "chromGraph.h"
#include "customTrack.h"
#include "customPp.h"
#include "customFactory.h"
#include "chromGraphFactory.h"

typedef int (*Chopper)(char *line, char **cols, int maxCol);
/* A function that breaks a row into columns. */

struct chromPos
/* Just chromosome and position */
    {
    char *chrom;	/* Not allocated here */
    int pos;
    };

struct labeledFile
/* A list of file and label */
    {
    struct labeledFile *next;
    char *fileName;	/* File name */
    char *label;	/* Label */
    struct chromGraph *cgList;	/* List of associated chrom graphs */
    double minVal,maxVal;	/* Min/mas values in graph */
    };

struct labeledFile *labeledFileNew(char *fileName, char *label)
/* Create new labeledFile struct */
{
struct labeledFile *el;
AllocVar(el);
el->fileName = cloneString(fileName);
el->label = cloneString(label);
return el;
}

static boolean chromGraphRecognizer(struct customFactory *fac,
	struct customPp *cpp, char *type, 
    	struct customTrack *track)
/* Return TRUE if looks like we're handling a wig track */
{
return sameOk(type, fac->name);
}

static int commaChopper(char *line, char **cols, int maxCol)
/* Chop line by commas and return line count */
{
return chopByChar(line, ',', cols, maxCol);
}

static int tabChopper(char *line, char **cols, int maxCol)
/* Chop line by commas and return line count */
{
return chopByChar(line, '\t', cols, maxCol);
}

static Chopper getChopper(char *formatType)
/* Get appropriate chopper function for format type */
{
if (sameString(formatType, cgfFormatTab))
    return tabChopper;
else if (sameString(formatType, cgfFormatComma))
    return commaChopper;
else if (sameString(formatType, cgfFormatSpace))
    return chopByWhite;
else
    {
    internalErr();
    return NULL;
    }
}

static struct slName *getPreview(struct customPp *cpp, int count)
/* Get preview lines. */
{
struct slName *list = NULL, *el;
int i;
for (i=0; i<count; ++i)
    {
    char *s = customFactoryNextRealTilTrack(cpp);
    if (s == NULL)
        break;
    el = slNameNew(s);
    slAddHead(&list, el);
    }
slReverse(&list);
return list;
}

static void returnPreview(struct customPp *cpp, struct slName **pList)
/* Return list of lines back to cpp for reuse. */
{
struct slName *el;
slReverse(pList);
for (el = *pList; el != NULL; el = el->next)
    customPpReuse(cpp, el->name);
slNameFreeList(pList);
}

static int delimitedTableSize(struct slName *list, int colDelim)
/* Return number of columns in list delimited by delimiter.  Returns
 * 0 if count is inconsistent. */
{
struct slName *el;
int expected = 0;
for (el = list; el != NULL; el = el->next)
    {
    int count = countChars(el->name, colDelim) + 1;
    if (expected == 0)
	{
        expected = count;
	}
    else
	{
        if (expected != count)
	    return 0;
	}
    }
return expected;
}

static int spaceDelimitedTableSize(struct slName *list)
/* Return number of columns in list delimited by whitespace.  Returns
 * 0 if count is inconsistent. */
{
struct slName *el;
int expected = 0;
for (el = list; el != NULL; el = el->next)
    {
    int count = chopByWhite(el->name, NULL, 0);
    if (expected == 0)
        expected = count;
    else
	{
        if (expected != count)
	    return 0;
	}
    }
return expected;
}

static boolean figureOutFormat(struct slName *lineList, 
	char **retType, int *retCols)
/* Figure out format and number of columns. Return FALSE if we can't
 * figure it out, otherwise return TRUE, and extra info in retType/retCols. */
{
int colCount;
char *type = cgfFormatGuess;
boolean ok = TRUE;

if ((colCount = spaceDelimitedTableSize(lineList)) > 1)
    {
    type = cgfFormatSpace;
    }
else if ((colCount = delimitedTableSize(lineList, '\t')) > 1)
    {
    type = cgfFormatTab;
    }
else if ((colCount = delimitedTableSize(lineList, ',')) > 1)
    {
    type = cgfFormatComma;
    }
else 
    {
    type = cgfFormatGuess;
    ok = FALSE;
    }
*retType = type;
*retCols = colCount;
return ok;
}

int countColumns(struct slName *list, char *formatType)
/* Return number of columns. */
{
int count = 0;
char *line = list->name;
if (sameString(formatType, cgfFormatTab))
    count = countChars(line, '\t') + 1;
else if (sameString(formatType, cgfFormatComma))
    count = countChars(line, ',') + 1;
else if (sameString(formatType, cgfFormatSpace))
    count = chopByWhite(line, NULL, 0);
else
    internalErr();
return count;
}

static char *findSnpTable(struct sqlConnection *conn)
/* Return name of SNP table if any */
{
char *tables[] = {"snp126", "snp125", "snp"};
int i;
for (i=0; i<ArraySize(tables); ++i)
    if (sqlTableExists(conn, tables[i]))
	return tables[i];
return NULL;
}

static char *guessMarkerType(struct slName *lineList, Chopper chopper, 
	struct sqlConnection *conn, int colCount)
/* Guess which type of marker type is being used. */
{
boolean gotSts = sqlTableExists(conn, "stsAlias");
char *snpTable = findSnpTable(conn);
int stsCount = 0, snpCount = 0, chromCount = 0;
int lineCount = 0;
struct slName *el;
char *type = NULL;

for (el = lineList; el != NULL; el = el->next)
    {
    char *s = cloneString(el->name);
    char *row[2];
    chopper(s, row, 2);
    char *marker = row[0];
    if (hgOfficialChromName(marker))
        ++chromCount;
    char query[512];
    if (gotSts)
	{
	safef(query, sizeof(query), 
	    "select count(*) from stsAlias where alias='%s'", marker);
	if (sqlQuickNum(conn, query) > 0)
	    ++stsCount;
	}
    else if (snpTable != NULL)
        {
	safef(query, sizeof(query), 
	    "select count(*) from %s where name='%s'", snpTable, marker);
	if (sqlQuickNum(conn, query) > 0)
	    ++snpCount;
	}
    ++lineCount;
    }
int minCount = lineCount/2;
if (chromCount > minCount && colCount >= 3)
    type = cgfMarkerGenomic;
else if (snpCount >= minCount)
    type = cgfMarkerSnp;
else if (stsCount >= minCount)
    type = cgfMarkerSts;
return type;
}

static boolean firstRowConsistentWithData(char *line, Chopper chopper, 
	int colCount)
/* Return TRUE if first row looks like it is real data (all numbers except
 * for marker column. */
{
char *dupe = cloneString(line);
char **row;
AllocArray(row, colCount);
chopper(dupe, row, colCount);
int i;
boolean ok = TRUE;
for (i=1; i<colCount; ++i)
    {
    char *col = row[i];
    char c = col[0];
    if (!(isdigit(c) || (c == '-' && isdigit(col[1]))))
        {
	ok = FALSE;
	break;
	}
    }
freeMem(dupe);
return ok;
}

static int markerCols(char *markerType)
/* The number of columns used for location. */
{
if (sameString(markerType, cgfMarkerGenomic))
    return 2;
else
    return 1;
}

static boolean allWhite(char *s)
/* Return TRUE if s is nothing but white space */
{
s = skipLeadingSpaces(s);
return s[0] == 0;
}

static void readLabels(struct customPp *cpp, int dataStart, Chopper chopper,
	char **row, int colCount, struct labeledFile *fileList)
/* Read in first nonempty line of file and fill in labels from it. */
{
char *line;
int colsRead;
int i;
struct labeledFile *fileEl;
if ((line = customFactoryNextRealTilTrack(cpp)) == NULL)
    errAbort("%s is empty", cpp->fileStack->fileName);
colsRead = chopper(line, row, colCount);
lineFileExpectWords(cpp->fileStack, colCount, colsRead);
for (i=dataStart, fileEl = fileList; i < colCount; i++, fileEl = fileEl->next)
    {
    char *label = row[i];
    if (!allWhite(label))
	fileEl->label = cloneString(row[i]);
    }
}

static struct hash *chromInfoHash(struct sqlConnection *conn)
/* Build up hash of chromInfo keyed by name */
{
struct sqlResult *sr;
char **row;
struct hash *hash = hashNew(0);
sr = sqlGetResult(conn, "select * from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct chromInfo *ci = chromInfoLoad(row);
    hashAdd(hash, ci->chrom, ci);
    }
sqlFreeResult(&sr);
return hash;
}

static void processGenomic(struct sqlConnection *conn, struct customPp *cpp, 
	int colCount, char *formatType, boolean firstLineLabels,
	struct labeledFile *fileList, boolean report)
/* Process three column file into chromGraph.  Abort if
 * there's a problem. */
{
char **row;
struct chromGraph *cg;
struct hash *chromHash = chromInfoHash(conn);
struct labeledFile *fileEl;
struct chromInfo *ci;
int i;
int rowCount = 0;
Chopper chopper = getChopper(formatType);
char *line;

AllocArray(row, colCount);
if (firstLineLabels)
    readLabels(cpp, 2, chopper, row, colCount, fileList);
while ((line = customFactoryNextRealTilTrack(cpp)) != NULL)
    {
    chopper(line, row, colCount);
    char *chrom = cloneString(row[0]);
    int start = lineFileNeedNum(cpp->fileStack, row, 1);
    ci = hashFindVal(chromHash, chrom);
    if (ci == NULL)
	lineFileAbort(cpp->fileStack,
		 "Chromosome %s not found in this assembly (%s).", 
		 chrom, hGetDb());
    if (start < 0 || start >= ci->size)
	lineFileAbort(cpp->fileStack,
		 "Chromosome %s is %d bases long, but got coordinate %u",
		 chrom, ci->size, start);
    for (i=2, fileEl = fileList; i<colCount; ++i, fileEl = fileEl->next)
	{
	char *val = row[i];
	if (val[0] != 0)
	    {
	    AllocVar(cg);
	    cg->chrom = chrom;
	    cg->chromStart = start;
	    cg->val = lineFileNeedDouble(cpp->fileStack, row, i);
	    slAddHead(&fileEl->cgList, cg);
	    }
	}
    ++rowCount;
    }
if (report)
    printf("Read in %d markers and values in <i>chromosome base</i> format.<BR>\n", 
	rowCount);
}

struct hash *tableToChromPosHash(struct sqlConnection *conn, char *table, 
	char *query)
/* Create hash of chromPos keyed by name field. */ 
{
char buf[256];
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(23);
struct lm *lm = hash->lm;
struct chromPos *pos;
safef(buf, sizeof(buf), query, table);
sr = sqlGetResult(conn, buf);
while ((row = sqlNextRow(sr)) != NULL)
    {
    lmAllocVar(lm, pos);
    pos->chrom = lmCloneString(lm, row[0]);
    pos->pos = sqlUnsigned(row[1]);
    touppers(row[2]);
    hashAdd(hash, row[2], pos);
    }
sqlFreeResult(&sr);
return hash;
}

struct hash *tableToAliasHash(struct sqlConnection *conn, char *table,
	char *query)
/* Create hash of true name keyed by alias */
{
struct sqlResult *sr;
char **row;
struct hash *hash = hashNew(19);
char buf[256];
safef(buf, sizeof(buf), query, table);
sr = sqlGetResult(conn, buf);
while ((row = sqlNextRow(sr)) != NULL)
    {
    touppers(row[0]);
    touppers(row[1]);
    hashAdd(hash, row[0], lmCloneString(hash->lm, row[1]));
    }
sqlFreeResult(&sr);
return hash;
}

void processDb(struct sqlConnection *conn,
	struct customPp *cpp, int colCount, char *formatType, 
	boolean firstLineLabels, struct labeledFile *fileList, 
	char *table, char *query, char *aliasTable, char *aliasQuery,
	boolean report)
/* Process two column input file into chromGraph.  Treat first
 * column as a name to look up in bed-format table, which should
 * not be split. Return TRUE on success. */
{
struct hash *hash = tableToChromPosHash(conn, table, query);
struct hash *aliasHash = NULL;
char **row;
int match = 0, total = 0;
struct chromGraph *cg;
struct chromPos *pos;
if (report)
    printf("Loaded %d elements from %s table for mapping.<BR>\n", 
    	hash->elCount, table);
if (aliasTable != NULL)
    {
    aliasHash = tableToAliasHash(conn, aliasTable, aliasQuery);
    if (report)
	printf("Loaded %d aliases from %s table as well.<BR>\n", 
		aliasHash->elCount, aliasTable);
    }
AllocArray(row, colCount);
Chopper chopper = getChopper(formatType);
char *line;

AllocArray(row, colCount);
if (firstLineLabels)
    readLabels(cpp, 1, chopper, row, colCount, fileList);
while ((line = customFactoryNextRealTilTrack(cpp)) != NULL)
    {
    chopper(line, row, colCount);
    char *name = row[0];
    touppers(name);
    ++total;
    pos = hashFindVal(hash, name);
    if (pos == NULL && aliasHash != NULL)
        {
	name = hashFindVal(aliasHash, name);
	if (name != NULL)
	    pos = hashFindVal(hash, name);
	}
    if (pos != NULL)
        {
	int i;
	struct labeledFile *fileEl;
	++match;
	for (i=1, fileEl=fileList; i < colCount; ++i, fileEl = fileEl->next)
	    {
	    char *val = row[i];
	    if (val[0] != 0)
		{
		AllocVar(cg);
		cg->chrom = pos->chrom;
		cg->chromStart = pos->pos;
		cg->val = lineFileNeedDouble(cpp->fileStack, row, i);
		slAddHead(&fileEl->cgList, cg);
		}
	    }
	}
    }
if (report)
    printf("Mapped %d of %d (%3.1f%%) of markers<BR>\n", match, total, 
	    100.0*match/total);
}

boolean mayProcessGenomic(struct sqlConnection *conn, struct customPp *cpp, 
	int colCount, char *formatType, boolean firstLineLabels, 
	struct labeledFile *fileList, boolean report)
/* Process three column file into chromGraph.  If there's a problem
 * print warning message and return FALSE. */
{
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
     processGenomic(conn, cpp, colCount, formatType, firstLineLabels, fileList,
     	report);
return errCatchFinish(&errCatch);
}

boolean mayProcessDb(struct sqlConnection *conn,
	struct customPp *cpp, int colCount, char *formatType, 
	boolean firstLineLabels, struct labeledFile *fileList, 
	char *table, char *query, char *aliasTable, char *aliasQuery, 
	boolean report)
/* Process database table into chromGraph.  If there's a problem
 * print warning message and return FALSE. */
{
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
     processDb(conn, cpp, colCount, formatType, firstLineLabels, 
     	fileList, table, query, aliasTable, aliasQuery, report);
return errCatchFinish(&errCatch);
}

static struct labeledFile *parseToLabeledFiles(struct customPp *cpp,
	int colCount, char *formatType, char *markerType,
	boolean firstLineLabels, struct sqlConnection *conn, boolean report)
/* Parse out cpp until next track, creating a list of labeled
 * binary files. */
{
/* Allocate a labeledFile for each column of real data. */
struct labeledFile *fileList = NULL, *fileEl;
int posColCount = markerCols(markerType);
int i;
for (i=posColCount; i<colCount; ++i)
    {
    struct tempName tempName;
    char buf[16];
    safef(buf, sizeof(buf), "hggUp%d", i);
    makeTempName(&tempName, buf, ".cgb");
    safef(buf, sizeof(buf), "%d", i+1-posColCount);
    fileEl = labeledFileNew(tempName.forCgi, buf);
    slAddHead(&fileList, fileEl);
    }
slReverse(&fileList);

boolean ok = FALSE;
if (sameString(markerType, cgfMarkerGenomic))
    ok = mayProcessGenomic(conn, cpp, colCount, formatType, 
    	firstLineLabels, fileList, report);
else if (sameString(markerType, cgfMarkerSts))
    ok = mayProcessDb(conn, cpp, colCount, formatType, 
    	firstLineLabels, fileList, "stsMap",
    	"select chrom,round((chromStart+chromEnd)*0.5),name from %s",
	"stsAlias", "select alias,trueName from %s", report);
else if (sameString(markerType, cgfMarkerSnp))
    {
    char *snpTable = findSnpTable(conn);
    if (snpTable == NULL)
        errAbort("No SNP table in %s", hGetDb());
    char *query = "select chrom,chromStart,name from %s";
    ok = mayProcessDb(conn, cpp, colCount, formatType, 
	    firstLineLabels, fileList, snpTable, 
	    query, NULL, NULL, report);
    }
else if (sameString(markerType, cgfMarkerAffy100))
    {
    warn("Support for Affy 100k chip coming soon.");
    }
else if (sameString(markerType, cgfMarkerAffy500))
    {
    warn("Support for Affy 500k chip coming soon.");
    }
else if (sameString(markerType, cgfMarkerHumanHap300))
    {
    warn("Support for Illumina HumanHap300 coming soon.");
    }
else
    {
    errAbort("Unknown identifier format.");
    }
if (ok)
    return fileList;
else
    return NULL;
}

static void saveLabeledFileList(struct labeledFile *fileList)
/* Save out list of labled files to all binary files. */
{
struct labeledFile *fileEl;
for (fileEl = fileList; fileEl != NULL; fileEl = fileEl->next)
    {
    slSort(&fileEl->cgList, chromGraphCmp);
    chromGraphToBinGetMinMax(fileEl->cgList, fileEl->fileName, 
    	&fileEl->minVal, &fileEl->maxVal);
    }
}

struct customTrack *chromGraphParser(struct customPp *cpp,
	char *formatType, char *markerType, char *columnLabels,
	char *name, char *description, struct hash *settings,
	boolean report)
/* Parse out a chromGraph file (not including any track lines) */
{
char *minVal = hashFindVal(settings, "minVal");
char *maxVal = hashFindVal(settings, "maxVal");

/* Get first lines of track.  If track empty then
 * might as well return NULL here. */
struct slName *preview = getPreview(cpp, 10);
if (preview == NULL)
    return NULL;

/* Figure out format type - scanning preview if it isn't well defined. */
struct sqlConnection *conn = hAllocConn();
int colCount;
if (sameString(formatType, cgfFormatGuess))
    {
    if (!figureOutFormat(preview, &formatType, &colCount))
	errAbort("Can't figure out format for chromGraph track %s", 
		emptyForNull(name));
    }
hashMayRemove(settings, "formatType");

/* Now that we know format can count columns and determin how to
 * chop up lines. */
colCount = countColumns(preview, formatType);
Chopper chopper = getChopper(formatType);

/* Figure out marker type - scanning marker column of preview if it isn't
 * well defined. */
if (sameString(markerType, cgfMarkerGuess))
    {
    markerType = guessMarkerType(preview, chopper, conn, colCount);
    if (markerType == NULL)
	errAbort("Can't figure out marker column type for chromGraph track %s",
		emptyForNull(name));
    }
hashMayRemove(settings, "markerType");

/* Figure out if columns are labeled in file, using preview if needed. */
if (sameString(columnLabels, cgfColLabelGuess))
    {
    if (firstRowConsistentWithData(preview->name, chopper, colCount))
	columnLabels = cgfColLabelNumbered;
    else
	columnLabels = cgfColLabelFirstRow;
    }
hashMayRemove(settings, "columnLabels");
returnPreview(cpp, &preview);
boolean labelsInData = sameString(columnLabels, cgfColLabelFirstRow);

/* Load data into list of labeled temp files. */
struct labeledFile *fileEl, *fileList;
fileList = parseToLabeledFiles(cpp, colCount, formatType, markerType,
    labelsInData, conn, report);
saveLabeledFileList(fileList);

/* Create a customTrack for each element in file list. */
struct customTrack *outputTracks = NULL;
for (fileEl = fileList; fileEl != NULL; fileEl = fileEl->next)
    {
    struct customTrack *track;
    AllocVar(track);
    struct trackDb *tdb = customTrackTdbDefault();
    track->tdb = tdb;

    /* Figure out short and long names and type*/
    char shortLabel[128];
    char longLabel[512];
    if (name == NULL)
        name = track->tdb->shortLabel;
    if (description == NULL)
        description = track->tdb->longLabel;
    if (colCount > 1 || labelsInData)
        {
	safef(shortLabel, sizeof(shortLabel), "%s %s", name, fileEl->label);
	safef(longLabel, sizeof(longLabel), "%s %s", description, fileEl->label);
	}
    else
        {
	safef(shortLabel, sizeof(shortLabel), "%s", name);
	safef(longLabel, sizeof(longLabel), "%s", description);
	}
    tdb->shortLabel = cloneString(shortLabel);
    tdb->longLabel = cloneString(longLabel);
    tdb->type = "chromGraph";
    tdb->tableName = customTrackTableFromLabel(tdb->shortLabel);

    /* Create settings */
    struct dyString *dy = dyStringNew(0);
    dyStringAppend(dy, hashToRaString(settings));
    dyStringPrintf(dy, "binaryFile %s\n", fileEl->fileName);
    dyStringPrintf(dy, "type %s\n", tdb->type);
    dyStringPrintf(dy, "tdbType %s\n", tdb->type); /* Needed if outside factory */
    if (minVal == NULL)
        dyStringPrintf(dy, "minVal %g\n", fileEl->minVal);
    if (maxVal == NULL)
        dyStringPrintf(dy, "maxVal %g\n", fileEl->maxVal);
    tdb->settings = dyStringCannibalize(&dy);
    
    /* Add to list. */
    slAddHead(&outputTracks, track);
    }
hFreeConn(&conn);
slReverse(&outputTracks);
return outputTracks;
}


static struct customTrack *chromGraphLoader(struct customFactory *fac,  
	struct hash *chromHash, struct customPp *cpp, struct customTrack *track, 
	boolean dbRequested)
/* Load up chromGraph data until get next track line. */
{
/* See if binary file has already been made.  If so then
 * no need for rest of this routine. */
struct trackDb *tdb = track->tdb;
struct hash *settings = tdb->settingsHash;
char *binaryFileName = hashFindVal(settings, "binaryFile");
if (binaryFileName != NULL)
    {
    if (fileExists(binaryFileName))
	return track;
    else
        return NULL;
    }
return chromGraphParser(cpp, 
    trackDbSettingOrDefault(tdb, "formatType", cgfFormatGuess),
    trackDbSettingOrDefault(tdb, "markerType", cgfMarkerGuess),
    trackDbSettingOrDefault(tdb, "columnLabels", cgfColLabelGuess),
    trackDbSetting(tdb, "name"),
    trackDbSetting(tdb, "description"),
    tdb->settingsHash, TRUE);
}

struct customFactory chromGraphFactory = 
/* Factory for wiggle tracks */
    {
    NULL,
    "chromGraph",
    chromGraphRecognizer,
    chromGraphLoader,
    };

