/* Upload - put up upload pages and sub-pages. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "ra.h"
#include "portable.h"
#include "cheapcgi.h"
#include "localmem.h"
#include "cart.h"
#include "web.h"
#include "chromInfo.h"
#include "chromGraph.h"
#include "chromGraphFactory.h"
#include "errCatch.h"
#include "hgGenome.h"

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
    };

static char *markerNames[] = {
    // cgfMarkerBestGuess,
    cgfMarkerGenomic,
    cgfMarkerSts,
    cgfMarkerSnp,
    cgfMarkerAffy100,
    cgfMarkerAffy500,
    cgfMarkerHumanHap300,
    };

char *formatNames[] = {
    cgfFormatGuess,
    cgfFormatTab,
    cgfFormatComma,
    cgfFormatSpace,
    };

char *colLabelNames[] = {
    cgfColLabelGuess,
    cgfColLabelNumbered,
    cgfColLabelFirstRow,
    };

void uploadPage()
/* Put up initial upload page. */
{
char *oldFileName = cartUsualString(cart, hggUploadFile "__filename", "");
cartWebStart(cart, "Upload Data to Genome Association View");
hPrintf("<FORM ACTION=\"../cgi-bin/hgGenome\" METHOD=\"POST\" ENCTYPE=\"multipart/form-data\">");
cartSaveSession(cart);
hPrintf("Name of data set: ");
cartMakeTextVar(cart, hggDataSetName, "", 16);
hPrintf("<i>Only first 16 letters are displayed in some contexts.</i>");
hPrintf("<BR>");

hPrintf("Description: ");
cartMakeTextVar(cart, hggDataSetDescription, "", 64);
hPrintf(" <i>Optional - used in Genome Browser</i> ");
hPrintf("<BR>\n");


hPrintf("File format: ");
cgiMakeDropList(hggFormatType, formatNames, ArraySize(formatNames), 
    	cartUsualString(cart, hggFormatType, formatNames[0]));
hPrintf(" <i>Guess is usually accurate, override if problems.</i> ");
hPrintf("<BR>\n");

hPrintf(" Markers are: ");
cgiMakeDropList(hggMarkerType, markerNames, ArraySize(markerNames), 
    	cartUsualString(cart, hggMarkerType, markerNames[0]));
hPrintf("<i> This says how values are mapped to chromosomes.</i>");
hPrintf("<BR>\n");

hPrintf(" Column labels: ", colLabelNames, ArraySize(colLabelNames),
	cartUsualString(cart, hggColumnLabels, colLabelNames[0]));
cgiMakeDropList(hggColumnLabels, colLabelNames, ArraySize(colLabelNames), 
	cartUsualString(cart, hggColumnLabels, colLabelNames[0]));
hPrintf("<i> Optionally include first row of file in labels.</i>");
hPrintf("<BR>\n");

hPrintf("Display Min Value: ");
cartMakeTextVar(cart, hggMinVal, "", 5);
hPrintf(" Max Value: ");
cartMakeTextVar(cart, hggMaxVal, "", 5);
hPrintf(" <i>Leave min/max blank to take scale from data itself.</i><BR>\n");

hPrintf("Label Values: ");
cartMakeTextVar(cart, hggLabelVals, "", 32);
hPrintf(" <i>Comma-separated numbers for axis label</i><BR>\n");
hPrintf("Draw connecting lines between markers separated by up to ");
cartMakeIntVar(cart, hggMaxGapToFill, 25000000, 8);
hPrintf(" bases.<BR>");
hPrintf("File name: <INPUT TYPE=FILE NAME=\"%s\" VALUE=\"%s\">", hggUploadFile,
	oldFileName);
hPrintf(" ");
// cgiMakeButton(hggSubmitUpload, "Submit");
cgiMakeButton(hggSubmitUpload2, "Submit");
hPrintf("</FORM>\n");
hPrintf("<i>note: If you are uploading more than one data set please give them ");
hPrintf("different names.  Only the most recent data set of a given name is ");
hPrintf("kept.  Data sets will be kept for at least 8 hours.  After that time ");
hPrintf("you may have to upload them again.</i>");

/* Put up section that describes file formats. */
webNewSection("Upload file formats");
hPrintf("%s", 
"<P>The upload file is a table in some format.  In all formats there is "
"a single line for each marker. "
"Each line starts with information on the marker, and ends with "
"the numerical values associated with that marker. The exact format "
"of the line depends on what is selected from the locations drop "
"down menu.  If this is <i>chromosome base</i> then the line will "
"contain the tab or space-separated fields:  chromosome, position, "
"and value(s).  The first base in a chromosome is considered position 0. "
"An example <i>chromosome base</i> type line is is:<PRE><TT>\n"
"chrX 100000 1.23\n"
"</TT></PRE>The lines for other location type contain two fields: "
"marker and value(s).  For dbSNP rsID's an example is:<PRE><TT>\n"
"rs10218492 0.384\n"
"</TT></PRE>"
"</P><P>"
"The file can contain multiple value fields.  In this case a "
"separate graph will be available for each value column in the input "
"table. It's a "
"good idea to set the display min/max values above if you want the "
"graphs to share the same scale."
"</P>"
);

cartWebEnd();
}

struct hash *chromInfoHash(struct sqlConnection *conn)
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

void saveFileList(struct labeledFile *fileList)
/* Save out lists to all files. */
{
struct labeledFile *fileEl;
for (fileEl = fileList; fileEl != NULL; fileEl = fileEl->next)
    {
    slSort(&fileEl->cgList, chromGraphCmp);
    chromGraphToBin(fileEl->cgList, fileEl->fileName);
    }
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


typedef int (*chopper)(char *line, char **cols, int maxCol);

static chopper getChopper(char *formatType)
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

static boolean allWhite(char *s)
/* Return TRUE if s is nothing but white space */
{
s = skipLeadingSpaces(s);
return s[0] == 0;
}


static void readLabels(struct lineFile *lf, int dataStart, chopper chopper,
	char **row, int colCount, struct labeledFile *fileList)
/* Read in first nonempty line of file and fill in labels from it. */
{
char *line;
int colsRead;
int i;
struct labeledFile *fileEl;
if (!lineFileNextReal(lf, &line))
    errAbort("%s is empty", lf->fileName);
colsRead = chopper(line, row, colCount);
lineFileExpectWords(lf, colCount, colsRead);
for (i=dataStart, fileEl = fileList; i < colCount; i++, fileEl = fileEl->next)
    {
    char *label = row[i];
    if (!allWhite(label))
	fileEl->label = cloneString(row[i]);
    }
}

void  processGenomic(struct sqlConnection *conn, struct lineFile *lf, 
	int colCount, char *formatType, boolean firstLineLabels,
	struct labeledFile *fileList)
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
chopper chopper = getChopper(formatType);
char *line;

AllocArray(row, colCount);
if (firstLineLabels)
    readLabels(lf, 2, chopper, row, colCount, fileList);
while (lineFileNextReal(lf, &line))
    {
    chopper(line, row, colCount);
    char *chrom = cloneString(row[0]);
    int start = lineFileNeedNum(lf, row, 1);
    ci = hashFindVal(chromHash, chrom);
    if (ci == NULL)
	errAbort("Error line %d of %s. "
		 "Chromosome %s not found in this assembly (%s).", 
		 lf->lineIx, lf->fileName, chrom, database);
    if (start < 0 || start >= ci->size)
	errAbort("Error line %d of %s. "
		 "Chromosome %s is %d bases long, but got coordinate %u",
		 lf->lineIx, lf->fileName, chrom, ci->size, start);
    for (i=2, fileEl = fileList; i<colCount; ++i, fileEl = fileEl->next)
	{
	char *val = row[i];
	if (val[0] != 0)
	    {
	    AllocVar(cg);
	    cg->chrom = chrom;
	    cg->chromStart = start;
	    cg->val = lineFileNeedDouble(lf, row, i);
	    slAddHead(&fileEl->cgList, cg);
	    }
	}
    ++rowCount;
    }
hPrintf("Read in %d markers and values in <i>chromosome base</i> format.<BR>", 
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
	struct lineFile *lf, int colCount, char *formatType, 
	boolean firstLineLabels, struct labeledFile *fileList, 
	char *table, char *query, char *aliasTable, char *aliasQuery)
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
hPrintf("Loaded %d elements from %s table for mapping.<BR>", hash->elCount,
	table);
if (aliasTable != NULL)
    {
    aliasHash = tableToAliasHash(conn, aliasTable, aliasQuery);
    hPrintf("Loaded %d aliases from %s table as well.<BR>", aliasHash->elCount,
    	aliasTable);
    }
AllocArray(row, colCount);
chopper chopper = getChopper(formatType);
char *line;

AllocArray(row, colCount);
if (firstLineLabels)
    readLabels(lf, 1, chopper, row, colCount, fileList);
while (lineFileNextReal(lf, &line))
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
		cg->val = lineFileNeedDouble(lf, row, i);
		slAddHead(&fileEl->cgList, cg);
		}
	    }
	}
    }
hPrintf("Mapped %d of %d (%3.1f%%) of markers<BR>", match, total, 
	100.0*match/total);
}


boolean errCatchFinish(struct errCatch **pErrCatch)
/* Finish up error catching.  Report error if there is a
 * problem and return FALSE.  If no problem return TRUE.
 * This handles errCatchEnd and errCatchFree. */
{
struct errCatch *errCatch = *pErrCatch;
boolean ok = TRUE;
if (errCatch != NULL)
    {
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	{
	ok = FALSE;
	warn(errCatch->message->string);
	}
    errCatchFree(pErrCatch);
    }
return ok;
}

boolean mayProcessGenomic(struct sqlConnection *conn, struct lineFile *lf, 
	int colCount, char *formatType, boolean firstLineLabels, 
	struct labeledFile *fileList)
/* Process three column file into chromGraph.  If there's a problem
 * print warning message and return FALSE. */
{
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
     processGenomic(conn, lf, colCount, formatType, firstLineLabels, fileList);
return errCatchFinish(&errCatch);
}

boolean mayProcessDb(struct sqlConnection *conn,
	struct lineFile *lf, int colCount, char *formatType, 
	boolean firstLineLabels, struct labeledFile *fileList, 
	char *table, char *query, char *aliasTable, char *aliasQuery)
/* Process database table into chromGraph.  If there's a problem
 * print warning message and return FALSE. */
{
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
     processDb(conn, lf, colCount, formatType, firstLineLabels, 
     	fileList, table, query, aliasTable, aliasQuery);
return errCatchFinish(&errCatch);
}

void raSaveNext(struct hash *ra, FILE *f)
/* Write hash to file */
{
struct hashEl *el, *list = hashElListHash(ra);
slSort(&list, hashElCmp);
for (el = list; el != NULL; el = el->next)
    fprintf(f, "%s\t%s\n", el->name, (char*)el->val);
fprintf(f, "\n");
hashElFreeList(&list);
}

void raSaveAll(struct hash *allHash, char *fileName)
/* Save all elements of allHas to ra file */
{
FILE *f = mustOpen(fileName, "w");
struct hashEl *el, *list = hashElListHash(allHash);
slSort(&list, hashElCmp);
for (el = list; el != NULL; el = el->next)
    raSaveNext(el->val, f);
hashElFreeList(&list);
carefulClose(&f);
}

static void addIfNonempty(struct hash *ra, char *cgiVar, char *raVar)
/* If cgiVar exists and is non-empty, add it to ra. */
{
char *val = skipLeadingSpaces(cartUsualString(cart, cgiVar, ""));
if (val[0] != 0)
    hashAdd(ra, raVar, val);
}

struct labeledFile *labeledFileNew(char *fileName, char *label)
/* Create new labeledFile struct */
{
struct labeledFile *el;
AllocVar(el);
el->fileName = cloneString(fileName);
el->label = cloneString(label);
return el;
}

void updateUploadRa(struct labeledFile *list)
/* Update upload ra file with current upload data */
{
char *fileName = cartOptionalString(cart, hggUploadRa);
struct labeledFile *el;
struct tempName tempName;
struct hash *allRaHash, *ra;
char *dataSetName = skipLeadingSpaces(cartUsualString(cart, hggDataSetName, ""));
char graphName[512];

if (dataSetName[0] == 0)
    dataSetName = "user data";

/* Read in old ra file if possible, otherwise just dummy up an
 * empty hash */
if (fileName == NULL || !fileExists(fileName))
    {
    makeTempName(&tempName, "hggUp", ".ra");
    fileName = tempName.forCgi;
    allRaHash = hashNew(0);
    cartSetString(cart, hggUploadRa, fileName);
    }
else
    {
    allRaHash = raReadAll(fileName, "name");
    }

uglyf("updateUploadRa %s<BR>\n", fileName);
for (el = list; el != NULL; el = el->next)
    {
    if (el->label[0] != 0)
        safef(graphName, sizeof(graphName), "%s %s", dataSetName, el->label);
    else
        safef(graphName, sizeof(graphName), "%s", dataSetName);

    /* Get rid of old ra record of same name if any */
    if (hashLookup(allRaHash, graphName))
	hashRemove(allRaHash, graphName);

    /* Create ra hash with our info in it. */
    ra = hashNew(8);
    hashAdd(ra, "name", cloneString(graphName));
    hashAdd(ra, "description", 
	    cartUsualString(cart, hggDataSetDescription, graphName));
    hashAdd(ra, "markerType",
	    cartUsualString(cart, hggMarkerType, markerNames[0]));
    hashAdd(ra, "binaryFile", el->fileName);
    addIfNonempty(ra, hggMinVal, "minVal");
    addIfNonempty(ra, hggMaxVal, "maxVal");
    addIfNonempty(ra, hggMaxGapToFill, "maxGapToFill");
    addIfNonempty(ra, hggLabelVals, "linesAt");

    /* Update allRaHash and save */
    hashAdd(allRaHash, graphName, ra);
    }

raSaveAll(allRaHash, fileName);

hPrintf("This data is now available in the drop down menus on the ");
hPrintf("main page for graphing.<BR>");
}

void updateUploadRaOne(char *binFileName)
/* Update upload ra file with current upload data */
{
updateUploadRa(labeledFileNew(binFileName, ""));
}

static int markerCols(char *markerType)
/* The number of columns used for location. */
{
if (sameString(markerType, cgfMarkerGenomic))
    return 2;
else
    return 1;
}

void processUpload(char *text, int colCount, 
	char *formatType, char *markerType,  boolean firstLineLabels, 
	struct sqlConnection *conn)
/* Parse uploaded text.  If it looks good then make a 
 * binary chromGraph file out of it, and save information
 * about it in the upload ra file. */
{
boolean ok = FALSE;
struct lineFile *lf = lineFileOnString("uploaded data", TRUE, text);
int posColCount = markerCols(markerType);
/* NB - do *not* lineFileClose this or a double free can happen. */
struct labeledFile *fileList = NULL, *fileEl;
int i;

/* Create list of files. */
for (i=posColCount; i<colCount; ++i)
    {
    struct tempName tempName;
    char buf[16];
    safef(buf, sizeof(buf), "hggUp%d", i);
    makeTempName(&tempName, buf, ".cgb");
    safef(buf, sizeof(buf), "%d", i+1);
    fileEl = labeledFileNew(tempName.forCgi, buf);
    slAddTail(&fileList, fileEl);
    }
slReverse(&fileList);


if (sameString(markerType, cgfMarkerGenomic))
    ok = mayProcessGenomic(conn, lf, colCount, formatType, 
    	firstLineLabels, fileList);
else if (sameString(markerType, cgfMarkerSts))
    ok = mayProcessDb(conn, lf, colCount, formatType, 
    	firstLineLabels, fileList, "stsMap",
    	"select chrom,round((chromStart+chromEnd)*0.5),name from %s",
	"stsAlias", "select alias,trueName from %s");
else if (sameString(markerType, cgfMarkerSnp))
    {
    char *query = "select chrom,chromStart,name from %s";
    if (sqlTableExists(conn, "snp126"))
        ok = mayProcessDb(conn, lf, colCount, formatType, 
		firstLineLabels, fileList, "snp126", 
		query, NULL, NULL);
    else if (sqlTableExists(conn, "snp125"))
        ok = mayProcessDb(conn, lf, colCount, formatType, 
		firstLineLabels, fileList, 
		"snp125", query, NULL, NULL);
    else if (sqlTableExists(conn, "snp"))
        ok = mayProcessDb(conn, lf, colCount, formatType, 
		firstLineLabels, fileList, 
		"snp", query, NULL, NULL);
    else
        warn("Couldn't find SNP table");
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
    {
    saveFileList(fileList);
    updateUploadRa(fileList);
    }
}

#ifdef OLD
void submitUpload(struct sqlConnection *conn)
/* Called when they've submitted from uploads page */
{
char *rawText = cartUsualString(cart, hggUploadFile, NULL);
if (rawText == NULL || rawText[0] == 0)
    {
    cartWebStart(cart, "Upload error");
    hPrintf("Please go back, enter a file with something in it, and try again.");
    cartWebEnd();
    }
else
    {
    int rawTextSize = strlen(rawText);
    char *markerType = cartUsualString(cart, hggMarkerType, cgfMarkerGenomic);
    cartWebStart(cart, "Data Upload Complete (%d bytes)", rawTextSize);
    hPrintf("<FORM ACTION=\"../cgi-bin/hgGenome\">");
    cartSaveSession(cart);
    processUpload(rawText, 1 + markerCols(markerType), cgfFormatSpace, markerType,
    	FALSE, conn);
    cartRemove(cart, hggUploadFile);
    hPrintf("<CENTER>");
    cgiMakeButton("submit", "OK");
    hPrintf("</CENTER>");
    hPrintf("</FORM>");
    cartWebEnd();
    }
}
#endif /* OLD */

char *findNthUseOfChar(char *s, char c, int n)
/* Return the nth occurence of c in s, or NULL if not that many. */
{
int i;
s -= 1;	/* To make loop go more easily */
for (i=0; i<n; ++i)
    {
    s += 1;
    s = strchr(s, c);
    if (s == NULL)
        break;
    }
return s;
}

void fixLineEndings(char *s)
/* Convert <CR> or <CR><LF> line endings to <LF> */
{
char *in = s, *out = s, c;
int crLfCount = 0;
int crCount = 0;
while ((c = *in++) != 0)
    {
    if (c == '\r')
        {
	if (*in == '\n')
	   {
	   ++in;
	   ++crLfCount;
	   }
	c = '\n';
	++crCount;
	}
    *out++ = c;
    }
*out++ = 0;
}

char *dupeLines(char *text, int lineCount)
/* Return duplicate of first lines of text. */
{
char *dupe;
char *end = findNthUseOfChar(text, '\n', lineCount);
if (end == NULL) 
    dupe = cloneString(text);
else
    dupe = cloneStringZ(text, end-text+1);
return dupe;
}

boolean delimitedTableSize(char *text, int colDelim, int rowDelim,
	int *retCols, int *retRows)
/* Return TRUE if text looks to be a table delimited by a simple character
 * set.  If true, then retCols and retRows is size of table. */
{
char *s = text;
int rowCount = 0;
int colDelimExpected = 0;
int colDelimCount = 0;
for (;;)
    {
    char c = *s++;
    if (c == rowDelim)
        {
	if (rowCount == 0)
	    colDelimExpected = colDelimCount;
	else
	    if (colDelimExpected != colDelimCount)
	        return FALSE;
	rowCount += 1;
	colDelimCount = 0;
	}
    else if (c == colDelim)
        {
	colDelimCount += 1;
	}
    else if (c == 0)
        {
	break;
	}
    }
*retRows = rowCount;
*retCols = colDelimExpected+1;
return colDelimExpected > 0;
}

boolean spaceDelimitedTableSize(char *text, int *retCols, int *retRows)
/* Return TRUE if text looks to be a table delimited by white space */
{
char *dupe = cloneString(text);
char **rows;
boolean ok = TRUE;
int colsObserved = 0, rowsObserved = 0;

/* Chop it into lines, tolerating missing final end of line. */
int origRowCount = countChars(dupe, '\n')+1;
AllocArray(rows, origRowCount);
origRowCount = chopByChar(dupe, '\n', rows, origRowCount);

if (origRowCount > 0)
    {
    int colsExpected = 0;
    int i;
    for (i=0; i < origRowCount; ++i)
        {
	char *row = rows[i];
	if (!allWhite(row))
	    {
	    colsObserved = chopByWhite(row, NULL, 0);
	    if (colsExpected == 0)
	        colsExpected = colsObserved;
	    else if (colsExpected != colsObserved)
	        {
		ok = FALSE;
		break;
		}
	    ++rowsObserved;
	    }
	}
    }
else
    ok = FALSE;

freeMem(rows);
freeMem(dupe);
*retRows = rowsObserved;
*retCols = colsObserved;
return ok;
}


boolean analyseText(char *text, char **retType, int *retCols)
/* Look at first ten lines of text and figure out 
 * what sort of table we think it is.  Return FALSE if we
 * can't figure it out, otherwise return info in retType/retCols */
{
char *sampleText = dupeLines(text, 10);
int colCount, rowCount;
char *type = cgfFormatGuess;
boolean ok = TRUE;

if (delimitedTableSize(sampleText, '\t', '\n', &colCount, &rowCount))
    {
    type = cgfFormatTab;
    }
else if (spaceDelimitedTableSize(sampleText, &colCount, &rowCount))
    {
    type = cgfFormatSpace;
    }
else if (delimitedTableSize(sampleText, ',', '\n', &colCount, &rowCount))
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

boolean firstRowConsistentWithData(char *text, char *formatType, int colCount)
/* Return TRUE if first row is consistent with being real data. */
{
/* We'll actually get the first 10 lines, and look at the first one of
 * these that is non-blank. */
int sampleSize = 10, i;
char *sampleText = dupeLines(text, sampleSize);
char **rows, **cols;
AllocArray(rows, sampleSize);
AllocArray(cols, colCount);
sampleSize = chopByChar(sampleText, '\n', rows, sampleSize);
boolean ok = TRUE;
int oneColCount = 0;

for (i=0; i<sampleSize; ++i)
    {
    char *line = rows[0];
    oneColCount = 0;
    if (!allWhite(line))
	{
	if (sameString(formatType, cgfFormatTab))
	    {
	    oneColCount = chopByChar(line, '\t', cols, colCount);
	    break;
	    }
	else if (sameString(formatType, cgfFormatComma))
	    {
	    oneColCount = chopByChar(line, ',', cols, colCount);
	    break;
	    }
	else if (sameString(formatType, cgfFormatSpace))
	    {
	    oneColCount = chopByWhite(line, cols, colCount);
	    break;
	    }
	else
	    internalErr();
	}
    }
if (oneColCount == colCount)
    {
    if (allWhite(cols[i]))
        ok = FALSE;
    else
	{
	for (i=1; i<colCount; ++i)
	    {
	    char *col = cols[i];
	    char c = col[0];
	    if (!(isdigit(c) || (c == '-' && isdigit(col[1]))))
		{
		ok = FALSE;
		break;
		}
	    }
	}
    }
else
    ok = FALSE;
freeMem(sampleText);
freeMem(rows);
freeMem(cols);
return ok;
}

int countColumns(char *text, char *formatType)
/* Return number of columns. */
{
int sampleSize = 10;
char *sampleText = dupeLines(text, sampleSize);
char **rows;
AllocArray(rows, sampleSize);
sampleSize = chopByChar(sampleText, '\n', rows, sampleSize);
int i;
int count = 0;
for (i=0; i<sampleSize; ++i)
    {
    char *line = rows[0];
    if (!allWhite(line))
        {
	if (sameString(formatType, cgfFormatTab))
	    {
	    count = countChars(line, '\t') + 1;
	    break;
	    }
	else if (sameString(formatType, cgfFormatComma))
	    {
	    count = countChars(line, ',') + 1;
	    break;
	    }
	else if (sameString(formatType, cgfFormatSpace))
	    {
	    count = chopByWhite(line, NULL, 0);
	    break;
	    }
	else
	    internalErr();
	}
    }
freeMem(rows);
return count;
}

void trySubmitUpload2(struct sqlConnection *conn, char *rawText)
/* Called when they've submitted from uploads page */
{
int colCount = 0;
char *formatType = cartUsualString(cart, hggFormatType, formatNames[0]);
fixLineEndings(rawText);
if (sameString(formatType, cgfFormatGuess))
    if (!analyseText(rawText, &formatType, &colCount))
	errAbort("Sorry, can't figure out this file's format. Please go back and try another file.");
colCount = countColumns(rawText, formatType);
char *labelType = cartUsualString(cart, hggColumnLabels, colLabelNames[0]);
if (sameString(labelType, cgfColLabelGuess))
    {
    if (firstRowConsistentWithData(rawText, formatType, colCount))
        labelType = cgfColLabelNumbered;
    else
        labelType = cgfColLabelFirstRow;
    }
char *markerType = cartUsualString(cart, hggMarkerType, cgfMarkerGenomic);
processUpload(rawText, colCount, formatType, markerType,
    sameString(labelType, cgfColLabelFirstRow), conn);
cartRemove(cart, hggUploadFile);
hPrintf("<CENTER>");
cgiMakeButton("submit", "OK");
hPrintf("</CENTER>");
}

void submitUpload2(struct sqlConnection *conn)
/* Called when they've submitted from uploads page */
{
char *rawText = cartUsualString(cart, hggUploadFile, NULL);
int rawTextSize = strlen(rawText);
// struct errCatch *errCatch = errCatchNew();
cartWebStart(cart, "Data Upload2 Complete (%d bytes)", rawTextSize);
hPrintf("<FORM ACTION=\"../cgi-bin/hgGenome\">");
cartSaveSession(cart);
// if (errCatchStart(errCatch))
     trySubmitUpload2(conn, rawText);
// errCatchFinish(&errCatch);
hPrintf("</FORM>");
cartWebEnd();
}
