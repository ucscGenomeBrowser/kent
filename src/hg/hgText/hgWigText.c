/*	hgWigText.c - functions for wiggle tracks in hgText	*/

#include "common.h"
#include "wiggle.h"
#include "hgColors.h"
#include "web.h"
#include "cheapcgi.h"
#include "hCommon.h"
#include "hgText.h"
#include "hui.h"
#include "customTrack.h"
#include "portable.h"

/*	possible two sets of data could be existing	*/
static struct wiggleData *wigData[2] =
{
	(struct wiggleData *)NULL,
	(struct wiggleData *)NULL,
};

/* Droplist menu for custom track visibility: */
static char *ctWigVisMenu[] =
{
    "hide",
    "dense",
    "full",
};
static int ctWigVisMenuSize = sizeof(ctWigVisMenu)/sizeof(char *);

/* Droplist menu for custom track data count: */
static char *ctWigCountMenu[] =
{
    "10000",
    "100000",
    "1000000",
};
static int ctWigCountMenuSize = sizeof(ctWigCountMenu)/sizeof(char *);

struct wiggleStats *wigStatsList[2] =
{
    (struct wiggleStats *) NULL,
    (struct wiggleStats *) NULL,
};

struct hash *chromsDone[2];

static void printBedEl(struct bed *bedEl)
{
printf("%s\t%u\t%u\t%s\n", bedEl->chrom, bedEl->chromStart,
	bedEl->chromEnd, bedEl->name);
}

/*********   wiggle compare functions   ***********************************/
static boolean wigInRange(int tableId, double value, boolean summaryOnly,
    struct wiggle *wiggle)
{
boolean ret = FALSE;
if (summaryOnly)
    ret = ((wiggle->lowerLimit <= wigDataConstraint[tableId][1]) &&
      (wiggle->lowerLimit+wiggle->dataRange >= wigDataConstraint[tableId][0]));
else
    ret = (value >= wigDataConstraint[tableId][0] &&
	value <= wigDataConstraint[tableId][1]);
return ret;
}
static boolean wigLessThan(int tableId, double value, boolean summaryOnly,
    struct wiggle *wiggle)
{
boolean ret = FALSE;
if (summaryOnly)
   ret = (wiggle->lowerLimit < wigDataConstraint[tableId][0]);
else
   ret = (value < wigDataConstraint[tableId][0]);
return ret;
}
static boolean wigLessEqual(int tableId, double value, boolean summaryOnly,
    struct wiggle *wiggle)
{
boolean ret = FALSE;
if (summaryOnly)
   ret = (wiggle->lowerLimit <= wigDataConstraint[tableId][0]);
else
   ret = (value <= wigDataConstraint[tableId][0]);
return ret;
}
static boolean wigEqual(int tableId, double value, boolean summaryOnly,
    struct wiggle *wiggle)
{
boolean ret = FALSE;
if (summaryOnly)
   ret = ((wiggle->lowerLimit < wigDataConstraint[tableId][0]) &&
	(wiggle->lowerLimit+wiggle->dataRange > wigDataConstraint[tableId][0]));
else
   ret = (value == wigDataConstraint[tableId][0]);
return ret;
}
static boolean wigNotEqual(int tableId, double value, boolean summaryOnly,
    struct wiggle *wiggle)
{
boolean ret = FALSE;
if (summaryOnly)
   ret = ((wiggle->lowerLimit > wigDataConstraint[tableId][0]) ||
	(wiggle->lowerLimit+wiggle->dataRange < wigDataConstraint[tableId][0]));
else
   ret = (value != wigDataConstraint[tableId][0]);
return ret;
}
static boolean wigGreaterEqual(int tableId, double value, boolean summaryOnly,
    struct wiggle *wiggle)
{
boolean ret = FALSE;
if (summaryOnly)
   ret=(wiggle->lowerLimit+wiggle->dataRange >= wigDataConstraint[tableId][0]);
else
   ret = (value >= wigDataConstraint[tableId][0]);
return ret;
}
static boolean wigGreaterThan(int tableId, double value, boolean summaryOnly,
    struct wiggle *wiggle)
{
boolean ret = FALSE;
if (summaryOnly)
   ret = (wiggle->lowerLimit+wiggle->dataRange > wigDataConstraint[tableId][0]);
else
   ret = (value > wigDataConstraint[tableId][0]);
return ret;
}

static void showConstraints(char *constraints, int tableId, boolean htmlOutput)
{
boolean foundSome = FALSE;

if (htmlOutput)
    printf("<P>Constraints in effect: ");

if ((constraints != NULL) && (constraints[0] != 0))
    {
    foundSome = TRUE;
    if (htmlOutput)
	printf("%s\n", constraints);
    else
	printf("#\tSQL query constraint: %s\n", constraints);
    }
if (wigConstraint[tableId])
    {
    if (htmlOutput)
	if (foundSome)
	    printf(" AND ");
    foundSome = TRUE;
    if (sameWord(wigConstraint[tableId],"in range"))
	{
	if (htmlOutput)
	    printf("(data value %s [%g , %g])",
		wigConstraint[tableId], wigDataConstraint[tableId][0],
		wigDataConstraint[tableId][1]);
	else
	    printf("#\tdata value constraint range: %s [%g , %g]\n",
		wigConstraint[tableId], wigDataConstraint[tableId][0],
		wigDataConstraint[tableId][1]);
	}
    else
	{
	if (htmlOutput)
	    printf("(data value %s %g)",
		wigConstraint[tableId], wigDataConstraint[tableId][0]);
	else
	    printf("#\tdata value constraint: %s %g\n",
		wigConstraint[tableId], wigDataConstraint[tableId][0]);
	}
    }

if (htmlOutput)
    {
    if (foundSome)
	printf("</P>\n");
    else
	printf("NONE</P>\n");
    }
}

void wiggleConstraints(char *cmp, char *pat, int tableIndex)
{
wigConstraint[tableIndex] = cmp;
wiggleCompare[tableIndex] = NULL;
if (strlen(pat)>0)
    {
    if (sameWord(cmp,"in range"))
	{
	char *rangeValues[2];
	char *clone = cloneString(pat);
	int records = 0;
	char *comma = strchr(pat,',');

	if (comma == (char *)NULL)
	    records = chopByWhite(clone,rangeValues,2);
	else
	    records = chopByChar(clone,',',rangeValues,2);

	if (records == 2)
	    {
	    trimSpaces(rangeValues[0]);
	    trimSpaces(rangeValues[1]);
	    wigDataConstraint[tableIndex][0] = sqlDouble(rangeValues[0]);
	    wigDataConstraint[tableIndex][1] = sqlDouble(rangeValues[1]);
	    if (wigDataConstraint[tableIndex][0] >
		    wigDataConstraint[tableIndex][1])
		{
		double d = wigDataConstraint[tableIndex][1];
		wigDataConstraint[tableIndex][1] =
		    wigDataConstraint[tableIndex][0];
		wigDataConstraint[tableIndex][0] = d;
		}
            wiggleCompare[tableIndex] = wigInRange;
	    }
	freeMem(clone);
	if (wigDataConstraint[tableIndex][1] ==
		wigDataConstraint[tableIndex][0])
	    errAbort("For \"in range\" constraint, you must give two numbers separated by whitespace or comma.");
	}
    else
	{
	wigDataConstraint[tableIndex][0] = sqlDouble(pat);
	if (sameWord(cmp,"<")) wiggleCompare[tableIndex] = wigLessThan;
	else if (sameWord(cmp,"<=")) wiggleCompare[tableIndex] = wigLessEqual;
	else if (sameWord(cmp,"=")) wiggleCompare[tableIndex] = wigEqual;
	else if (sameWord(cmp,"!=")) wiggleCompare[tableIndex] = wigNotEqual;
	else if (sameWord(cmp,">=")) wiggleCompare[tableIndex]=wigGreaterEqual;
	else if (sameWord(cmp,">")) wiggleCompare[tableIndex] = wigGreaterThan;
	}
    }
}

static void wigPrintRow(struct wiggleStats *wigStats)
{
printf("<TR><TH ALIGN=LEFT> %s </TH>\n", wigStats->chrom);
printf("\t<TD ALIGN=RIGHT> %u </TD>\n", wigStats->chromStart+1);
					/* display closed coords */
printf("\t<TD ALIGN=RIGHT> %u </TD>\n", wigStats->chromEnd);
printf("\t<TD ALIGN=RIGHT> %u </TD>\n", wigStats->count);
printf("\t<TD ALIGN=RIGHT> %d </TD>\n", wigStats->span);
printf("\t<TD ALIGN=RIGHT> %u </TD>\n", wigStats->count*wigStats->span);
printf("\t<TD ALIGN=RIGHT> %g </TD>\n", wigStats->lowerLimit);
printf("\t<TD ALIGN=RIGHT> %g </TD>\n", wigStats->lowerLimit+wigStats->dataRange);
printf("\t<TD ALIGN=RIGHT> %g </TD>\n", wigStats->dataRange);
printf("\t<TD ALIGN=RIGHT> %g </TD>\n", wigStats->mean);
printf("\t<TD ALIGN=RIGHT> %g </TD>\n", wigStats->variance);
printf("\t<TD ALIGN=RIGHT> %g </TD>\n", wigStats->stddev);
printf("<TR>\n");
}

static void wigPrintStats(struct wiggleStats **wsList, char *chrom)
{
struct wiggleStats *wigStats = (struct wiggleStats *)NULL;

for (wigStats = *wsList; wigStats; wigStats = wigStats->next)
    {
    if (sameWord(wigStats->chrom,chrom))
	wigPrintRow(wigStats);
    }
}

/*	Check to see if this stats are done for this chrom */
static boolean wigStatsDone(int tableId, char *chrom)
{
if (chromsDone[tableId] == (struct hash *)NULL)
    return FALSE;
else if (hashLookup(chromsDone[tableId],chrom))
    return TRUE;
else
    return FALSE;
}

static void wigMarkDone(int tableId, char *chrom)
{
if ((struct hash *)NULL == chromsDone[tableId])
    chromsDone[tableId] = newHash(0);
if ( (struct hashEl *)NULL == hashLookup(chromsDone[tableId],chrom))
    hashAdd(chromsDone[tableId],chrom,NULL);
}

void wigMakeBedList(char *database, char *table, char *chrom,
	char *constraints, int tableId)
{
char *setting = cartCgiUsualString(cart, "tbWigCount", ctWigCountMenu[1]);
unsigned maxLinesOut = MAX_LINES_OUT;

if (setting != (char *) NULL)
    maxLinesOut = sqlUnsigned(setting);

if ( ! wigStatsDone(tableId, chrom))
    {
    wigData[tableId] = wigFetchData(database, table, chrom, winStart, winEnd,
	WIG_ALL_DATA, WIG_DATA_NOT_RETURNED, tableId, wiggleCompare[tableId],
	    constraints, &bedListWig[tableId], maxLinesOut,
		&wigStatsList[tableId]);
    wigMarkDone(tableId, chrom);
    }
}

void wigDoStats(char *database, char *table, struct slName *chromList,
    int tableId, char *constraints)
{
struct slName *chromPtr;
char *db = getTableDb();
struct sqlConnection *conn = hAllocOrConnect(db);
struct sqlResult *sr = (struct sqlResult *)NULL;
char query[256];
char **row = (char **)NULL;
char wigFullTableName[256];
char *setting = cartCgiUsualString(cart, "tbWigCount", ctWigCountMenu[1]);
unsigned maxLinesOut = MAX_LINES_OUT;
int numChroms = 0;
int tableRowsDisplayed = 0;

if (setting != (char *) NULL)
    maxLinesOut = sqlUnsigned(setting);

if (tableIsSplit)
    {
    getFullTableName(wigFullTableName, hDefaultChromDb(db), table);
    snprintf(query, sizeof(query), "show table status like '%s'", wigFullTableName);
    }
else
    snprintf(query, sizeof(query), "show table status like '%s'", table);

sr = sqlMustGetResult(conn,query);
row = sqlNextRow(sr);

// For some reason BORDER=1 does not work in our web.c nested table scheme.
// So use web.c's trick of using an enclosing table to provide a border.
puts("<P><!--outer table is for border purposes-->" "\n"
     "<TABLE BGCOLOR='#" HG_COL_BORDER "' BORDER=0 CELLSPACING=0 CELLPADDING=1><TR><TD>");

puts("<TABLE BGCOLOR='#" HG_COL_INSIDE "' BORDER=1 CELLSPACING=0>");

if (row != NULL)
    {
    printf("<TR><TD COLSPAN=12>\n");
    printf("<TABLE COLS=12 ALIGN=CENTER HSPACE=0>"
	"<TR><TH COLSPAN=1 ALIGN=LEFT> Database: %s </TH><TH COLSPAN=1 ALIGN=CENTER> Table: %s </TH><TH COLSPAN=10 ALIGN=RIGHT>  Last update: %s </TH></TR></TABLE></TD></TR>\n",
	database, table, row[11]);
    }
else
    {
    printf("<TR><TH COLSPAN=6 ALIGN=LEFT> Database: %s </TH><TH COLSPAN=6 ALIGN=RIGHT> Table: %s </TH></TR>\n", database,
	table);
    }

sqlFreeResult(&sr);
hFreeOrDisconnect(&conn);

printf("<TR><TH> Chrom </TH><TH> Data <BR> start </TH>");
printf("<TH> Data <BR> end </TH>");
printf("<TH> # of Data <BR> values </TH><TH> Data <BR> span </TH>");
printf("<TH> Bases <BR> covered </TH><TH> Minimum </TH>");
printf("<TH> Maximum </TH><TH> Range </TH><TH> Mean </TH>");
printf("<TH> Variance </TH><TH> Standard <BR> deviation </TH></TR>\n");

numChroms = slCount(chromList);
for (chromPtr=chromList;  chromPtr != NULL; chromPtr=chromPtr->next)
    {
    char *chrom = chromPtr->name;
    char wigFullTableName[256];

    getFullTableName(wigFullTableName, chrom, table);

    if ( ! wigStatsDone(tableId, chrom))
	{
	boolean dataFetchType = WIG_SUMMARY_ONLY;

	if (numChroms == 1)
		dataFetchType = WIG_ALL_DATA;
	if (wiggleCompare[tableId])
		dataFetchType = WIG_ALL_DATA;

	if (numChroms > 1)
	wigData[tableId] = wigFetchData(database, wigFullTableName, chrom,
	    winStart, winEnd, dataFetchType, WIG_DATA_NOT_RETURNED, tableId,
		wiggleCompare[tableId], constraints, (struct bed **)NULL,
		    maxLinesOut, &wigStatsList[tableId]);
	else
	wigData[tableId] = wigFetchData(database, wigFullTableName, chrom,
	    winStart, winEnd, dataFetchType, WIG_DATA_NOT_RETURNED, tableId,
		wiggleCompare[tableId], constraints, (struct bed **)NULL,
		    0, &wigStatsList[tableId]);
	wigMarkDone(tableId, chrom);
	}

    if (wigStatsDone(tableId, chrom))
	{
	wigPrintStats(&wigStatsList[tableId], chrom);
	if (wigStatsList[tableId] != (struct wiggleStats *) NULL)
	    ++tableRowsDisplayed;
	}
    else
	{
	printf("<TR><TH ALIGN=LEFT> %s </TH>", chrom);
	printf("<TH COLSPAN=11> No data </TH></TR>\n");
	++tableRowsDisplayed;
	}
    }

/*	No rows in table, indicate this no result */
if (!tableRowsDisplayed)
    puts("<TR><TH ALIGN=CENTER COLSPAN=12> No data found matching this request </TH></TR>");

printf("</TABLE>\n");
puts("</TD></TR></TABLE></P>");
}	/*	void wigDoStats()	*/

void doWiggleCtOptions(boolean doCt)
{
struct hTableInfo *hti = getOutputHti();
char *table = getTableName();
char *table2 = getTable2Name();
char *op = cgiOptionalString("tbIntersectOp");
char *db = getTableDb();
char *outputType = cgiUsualString("outputType", cgiString("phase"));
char *phase = (existsAndEqual("phase", getOutputPhase) ?
	       cgiString("outputType") : cgiString("phase"));
char *setting = NULL;
char buf[256];
char *constraints = constrainFields(NULL);

saveOutputOptionsState();
saveIntersectOptionsState();

webStart(cart, "Table Browser: %s %s: %s", hOrganism(database), freezeName,
	 phase);
checkTableExists(fullTableName);
printf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=\"%s\">\n",
       hgTextName(), httpFormMethod);
cartSaveSession(cart);
cgiMakeHiddenVar("db", db);

cgiMakeHiddenVar("table", getTableVar());
displayPosition();
cgiMakeHiddenVar("outputType", outputType);
preserveConstraints(fullTableName, db, NULL);
preserveTable2();
printf("<H3> Select %s Options for %s: </H3>\n",
       (doCt ? "Custom Track" : "DATA"), hti->rootName);
puts("<A HREF=\"/goldenPath/help/hgTextHelp.html#BEDOptions\">"
     "<B>Help</B></A><P>");
puts("<TABLE><TR><TD>");
if (doCt)
    {
    puts("</TD><TD>"
	 "<A HREF=\"/goldenPath/help/customTrack.html\" TARGET=_blank>"
	 "Custom track</A> header: </B>");
    }
else
    {
    cgiMakeCheckBox("tbDoCustomTrack",
		    cartCgiUsualBoolean(cart, "tbDoCustomTrack", FALSE));
    puts("</TD><TD> <B> Include "
	 "<A HREF=\"/goldenPath/help/customTrack.html\" TARGET=_blank>"
	 "custom track</A> header: </B>");
    }
puts("</TD></TR><TR><TD></TD><TD>name=");
if (op == NULL)
    table2 = NULL;
snprintf(buf, sizeof(buf), "tb_%s", hti->rootName);
setting = cgiUsualString("tbCtName", buf);
cgiMakeTextVar("tbCtName", setting, 16);
puts("</TD></TR><TR><TD></TD><TD>description=");
snprintf(buf, sizeof(buf), "table browser query on %s%s%s",
	 table,
	 (table2 ? ", " : ""),
	 (table2 ? table2 : ""));
setting = cgiUsualString("tbCtDesc", buf);
cgiMakeTextVar("tbCtDesc", setting, 50);
puts("</TD></TR><TR><TD></TD><TD>visibility=");
setting = cartCgiUsualString(cart, "tbCtVis", ctWigVisMenu[2]);
cgiMakeDropList("tbCtVis", ctWigVisMenu, ctWigVisMenuSize, setting);
puts("</TD></TR><TR><TD></TD></TR></TABLE>");

showConstraints(constraints, WIG_TABLE_1, TRUE);

printf("<P> <B> Select type of data output: </B> <BR>\n");
if (doCt)
    setting = cartCgiUsualString(cart, "tbWigDataType", "bedData");
else
    setting = cartCgiUsualString(cart, "tbWigDataType", "wigData");
cgiMakeRadioButton("tbWigDataType", "bedData", sameString(setting, "bedData"));
printf("BED format (no data value information, only position)<BR>\n");
cgiMakeRadioButton("tbWigDataType", "wigData", sameString(setting, "wigData"));
printf("DATA VALUE format (position and real valued data)</P>\n");

puts("<P> <B> Limit output to: </B>");
setting = cartCgiUsualString(cart, "tbWigCount", ctWigCountMenu[1]);
cgiMakeDropList("tbWigCount", ctWigCountMenu, ctWigCountMenuSize, setting);
puts("<B> lines of data </B> (to avoid browser overload)</P>\n");

if (doCt)
    {
    cartSetBoolean(cart, "tbDoCustomTrack", TRUE);
    cgiMakeButton("phase", getCtWiggleTrackPhase);
    cgiMakeButton("phase", getCtWiggleFilePhase);
    }
else
    {
    cgiMakeButton("phase", getWigglePhase);
    }
puts("</FORM>");
webEnd();
}	/*	void doWiggleCtOptions(boolean doCt)	*/

static void fileConstraints(char *constraints, int tableId, FILE *f)
{
if (constraints)
	fprintf(f, "#\tSQL query constraint: %s\n", constraints);
if (wigConstraint[tableId])
    {
    if (sameWord(wigConstraint[tableId],"in range"))
	fprintf(f,"#\tdata value constraint range: %s [%g , %g]\n",
	    wigConstraint[tableId], wigDataConstraint[tableId][0],
		wigDataConstraint[tableId][1]);
    else
	fprintf(f,"#\tdata value constraint: %s %g\n",
	    wigConstraint[tableId], wigDataConstraint[tableId][0]);
    }
}

void doGetWiggleData(boolean doCt)
/* Find wiggle data and display it */
{
struct slName *chromList, *chromPtr;
char *db = getTableDb();
char *table = getTableName();
struct wiggleData *wigData = (struct wiggleData *) NULL;
struct wiggleData *wdPtr = (struct wiggleData *) NULL;
struct trackDb *tdb = (struct trackDb *)NULL;
char *track = getTrackName();
unsigned linesOutput = 0;
boolean doCtHdr = (cartCgiUsualBoolean(cart, "tbDoCustomTrack", FALSE) || doCt);
char *constraints;
char *setting = cartCgiUsualString(cart, "tbWigCount", ctWigCountMenu[1]);
unsigned maxLinesOut = MAX_LINES_OUT;
char *longLabel = cloneString("User Supplied Track");
char *ctDesc = cgiUsualString("tbCtDesc", table);
char *ctName = cgiUsualString("tbCtName", table);
char *ctVis  = cgiUsualString("tbCtVis", "hide");
char *ctUrl  = cgiUsualString("tbCtUrl", "");
struct customTrack *ctNew = NULL;
char tableName[128];
char *visibility;
int visNum = 0;
boolean wigBED = FALSE;
struct bed *bedList = NULL;
FILE *wigAsciiFH = (FILE *) NULL;


if (! sameString(customTrackPseudoDb, db))
    {
    struct sqlConnection *conn = hAllocOrConnect(db);
    tdb = hMaybeTrackInfo(conn, track);
    hFreeOrDisconnect(&conn);
    }

if (setting != (char *) NULL)
    maxLinesOut = sqlUnsigned(setting);

setting = cartCgiUsualString(cart, "tbWigDataType", "wigData");
if ( sameString(setting, "bedData") )
    wigBED = TRUE;

saveOutputOptionsState();
saveIntersectOptionsState();

constraints = constrainFields(NULL);
if ((constraints != NULL) && (constraints[0] == 0))
    constraints = NULL;

if (!doCt)
    {
    printf("Content-Type: text/plain\n\n");
    webStartText();
    }

if (differentWord(ctName,table) )
    snprintf(tableName, sizeof(tableName), "%s", ctName);
else
    snprintf(tableName, sizeof(tableName), "tb_%s", table);

if (ctDesc != (char *)NULL)
    {
    freeMem(longLabel);
    longLabel = cloneString(ctDesc);
    }

if (ctVis != (char *)NULL)
    visibility = cloneString(ctVis);
else
    visibility = cloneString("hide");

visNum = (int) hTvFromStringNoAbort(visibility);
if (visNum < 0)
    visNum = 0;

if (allGenome)
    chromList = getOrderedChromList();
else
    chromList = newSlName(chrom);

if (doCtHdr && wigBED && !doCt)
    {
    printf("track name=\"%s\" description=\"%s\" "
	"visibility=%s\n", tableName, longLabel, visibility);
    showConstraints(constraints, WIG_TABLE_1, FALSE);
    }

if (doCt)
    {
    int fields=0;
    if (wigBED)
	fields=4;
    ctNew = newCT(tableName, longLabel, visNum, ctUrl, fields);
    if (wigBED)
	ctNew->wiggle = FALSE;
    else
	{
	struct tempName tn;
	makeTempName(&tn, "hgtct", ".wig");
	ctNew->wigFile = cloneString(tn.forCgi);
	makeTempName(&tn, "hgtct", ".wib");
	ctNew->wibFile = cloneString(tn.forCgi);
	makeTempName(&tn, "hgtct", ".wia");
	ctNew->wigAscii = cloneString(tn.forCgi);
	wigAsciiFH = mustOpen(ctNew->wigAscii, "w");
#if defined(DEBUG)	/*	dbg	*/
	/* allow file readability for debug */
	chmod(ctNew->wigAscii, 0666);
#endif

	ctNew->wiggle = TRUE;
	}
    }

for (chromPtr=chromList;  chromPtr != NULL && (linesOutput < maxLinesOut);
	chromPtr=chromPtr->next)
    {
    char *chrom = chromPtr->name;
    char wigFullTableName[256];
    int bedLength = 0;

    getFullTableName(wigFullTableName, chrom, table);

    if (wigBED)
	{
	wigData = wigFetchData(database, table, chrom, winStart, winEnd,
	    WIG_ALL_DATA, WIG_DATA_NOT_RETURNED, WIG_TABLE_1,
		wiggleCompare[WIG_TABLE_1], constraints,
		    &bedListWig[WIG_TABLE_1], maxLinesOut,
			(struct wiggleStats **)NULL);
	wigMarkDone(WIG_TABLE_1, chrom);
	bedLength = slCount(bedListWig[WIG_TABLE_1]);
	}
    else
	{
	wigData = wigFetchData(database, table, chrom, winStart, winEnd,
	    WIG_ALL_DATA, WIG_RETURN_DATA, WIG_TABLE_1,
		wiggleCompare[WIG_TABLE_1], constraints,
		    (struct bed **)NULL, maxLinesOut,
			(struct wiggleStats **)NULL);
	wigMarkDone(WIG_TABLE_1, chrom);
	}

    if (wigData)
	{
	unsigned span = 0;
	char *chrom = (char *) NULL;
	unsigned char colorR, colorG, colorB;
	unsigned char altColorR, altColorG, altColorB;
	float priority;
	int wordCount;
	char *words[128];
	char *trackType = (char *) NULL;
	boolean nextSpan = FALSE;

	if (tdb && tdb->type)
	    {
	    char *typeLine = cloneString(tdb->type);
	    if (ctDesc == (char *)NULL)
		{
		freeMem(longLabel);
		longLabel = cloneString(tdb->longLabel);
		}
	    priority = tdb->priority;
	    wordCount = chopLine(typeLine,words);
	    if (wordCount > 0)
		trackType = words[0];
	    colorR = tdb->colorR; colorG = tdb->colorG; colorB = tdb->colorB;
	    altColorR = tdb->altColorR; altColorG = tdb->altColorG;
	    altColorB = tdb->altColorB;
	    colorR = colorG = colorB = 0;
	    altColorR = altColorG = altColorB = 128;
	    }
	else
	    {
	    priority = 42;
	    if (ctDesc == (char *)NULL)
		{
		freeMem(longLabel);
		longLabel = cloneString("User Supplied Track");
		}
	    colorR = colorG = colorB = 255;
	    altColorR = altColorG = altColorB = 128;
	    }

	if (doCt)
	    {
	    ctNew->tdb->longLabel = longLabel;
	    }

	if (doCtHdr && (!wigBED))
	    {
	    if (doCt)
		{
		struct dyString *wigSettings = newDyString(0);
		ctNew->tdb->priority = priority;
		ctNew->tdb->colorR = colorR;
		ctNew->tdb->colorG = colorB;
		ctNew->tdb->colorB = colorB;
		ctNew->tdb->altColorR = altColorR;
		ctNew->tdb->altColorG = altColorB;
		ctNew->tdb->altColorB = altColorB;
		/*	more settings to be done */
		dyStringPrintf(wigSettings,
			    "type='wiggle_0'\twigFile='%s'\twibFile='%s'",
			    ctNew->wigFile, ctNew->wibFile);
		ctNew->tdb->settings = dyStringCannibalize(&wigSettings);
		fileConstraints(constraints, WIG_TABLE_1, wigAsciiFH);
		}
	    else
		{
		printf("track type=wiggle_0 name=%s description=\"%s\" "
		    "visibility=%s color=%d,%d,%d altColor=%d,%d,%d "
		    "priority=%g\n", tableName, longLabel, visibility,
		    colorR, colorG, colorB, altColorR, altColorG, altColorB,
		    priority);
		showConstraints(constraints, WIG_TABLE_1, FALSE);
		}
	    }

	for (wdPtr = wigData; (linesOutput < maxLinesOut) &&
		(wdPtr != (struct wiggleData *) NULL); wdPtr= wdPtr->next)
	    {
	    if ((chrom == (char *) NULL) || differentWord(chrom,wdPtr->chrom))
		{
		if (!(doCtHdr | wigBED))
		    {
		    if (!doCt)
			printf("#\t");
		    }
		if (!wigBED)
		    {
		    if (doCt)
			fprintf(wigAsciiFH,
			    "variableStep chrom=%s span=%u\n",
			    wdPtr->chrom, wdPtr->span);
		    else
			printf("variableStep chrom=%s span=%u\n",
			    wdPtr->chrom, wdPtr->span);
		    }
		chrom = wdPtr->chrom;
		span = wdPtr->span;
		}
	    if (span != wdPtr->span)
		{
		if (!(doCtHdr | wigBED))
		    printf("#\t");
		if (!wigBED)
		    {
		    if (doCt)
			fprintf(wigAsciiFH,
			    "variableStep chrom=%s span=%u\n",
			    wdPtr->chrom, wdPtr->span);
		    else
			printf("variableStep chrom=%s span=%u\n",
			    wdPtr->chrom, wdPtr->span);
		    }
		span = wdPtr->span;
		}
	    if (wdPtr->data)
		{
		struct wiggleDatum *wd = wdPtr->data;
		int i;
		for (i = 0; (!nextSpan) && (linesOutput < maxLinesOut) &&
			    (i < wdPtr->count); ++i, ++wd)
		    {
		    if (doCt)
			fprintf(wigAsciiFH, "%u\t%g\n",
			    wd->chromStart+1, wd->value);
		    else
			printf("%u\t%g\n", wd->chromStart+1, wd->value);
		    ++linesOutput;
		    }
		}
	    else if (bedLength > 0)
		{
		struct bed *bedEl = bedListWig[WIG_TABLE_1];
		for ( ; linesOutput < maxLinesOut && bedEl;
			bedEl = bedEl->next)
		    {
		    if (doCt)
			{
			struct bed *newBed = cloneBed(bedEl);
			slAddHead(&bedList, newBed);
			}
		    else
			printBedEl(bedEl);
		    ++linesOutput;
		    }
		if (doCt)
		    bedFreeList(&bedListWig[WIG_TABLE_1]);
		    bedLength = 0;
		}
	    }
	wigFreeData(&wigData);
	}
    }

if ((ctNew != NULL) && (bedList != NULL))
    {
    slReverse(&bedList);
    ctNew->bedList = bedList;
    }

if ((ctNew != NULL) && ((ctNew->bedList != NULL) || (ctNew->wigAscii != NULL)))
    {
    /* Load existing custom tracks and add this new one: */
    struct customTrack *ctList = getCustomTracks();
    slAddHead(&ctList, ctNew);
    carefulClose(&wigAsciiFH);
    /* Save the custom tracks out to file (overwrite the old file): */
    customTracksSaveCart(cart, ctList);
    }

if (doCt)
    {
    char browserUrl[128];
    char headerText[256];
    int redirDelay = 5;

    if (linesOutput < 1)
	{
	printf("Content-Type: text/plain\n\n");
	webStartText();
	printf("#\tno results returned from query\n");
	webEnd();
	}
    else
	{
	safef(browserUrl, sizeof(browserUrl),
	      "%s?db=%s&position=%s:%d-%d",
	      hgTracksName(), database, chrom, winStart, winEnd);
	safef(headerText, sizeof(headerText),
	      "<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"%d;URL=%s\">",
	      redirDelay, browserUrl);
	webStartHeader(cart, headerText,
		       "Table Browser: %s %s: %s", hOrganism(database),
		       freezeName, getCtPhase);
	printf("You will be automatically redirected to the genome browser in\n"
	       "%d seconds, or you can <BR>\n"
	       "<A HREF=\"%s\">click here to continue</A>.\n",
	       redirDelay, browserUrl);
	webEnd();
	}
    }
else
    {
    if (linesOutput < 1)
	{
	printf("#\tno results returned from query\n");
	}
    else if (linesOutput >= maxLinesOut)
	printf("#\tmaximum data output of %u lines reached\n", maxLinesOut);

	webEnd();
    }
}	/*	void doGetWiggleData(boolean doCt)	*/
