/*	wigDataStream - an object to access wiggle data values from
 *	either a DB access or from a .wig text file (==custom track)
 */
#include "common.h"
#include "memalloc.h"
#include "wiggle.h"
#include "portable.h"
#include "hgColors.h"
#include "obscure.h"
#include "customTrack.h"

static char const rcsid[] = "$Id: wigDataStream.c,v 1.83.6.4 2008/08/12 23:35:36 markd Exp $";

/*	Routines that are not strictly part of the wigDataStream object,
	but they are used to do things with the object.
**************************************************************************/

void wigStatsTableHeading(FILE * fh, boolean htmlOut)
/*	Print the single html (or text) table row for statistics
	column headings */
{
if (htmlOut)
    {
    fprintf(fh,"<TR><TH> Chrom </TH><TH> Data <BR> start </TH>");
    fprintf(fh,"<TH> Data <BR> end </TH>");
    fprintf(fh,"<TH> #&nbsp;of&nbsp;Data <BR> values </TH><TH> "
	    "Each&nbsp;data <BR> value&nbsp;spans <BR> #&nbsp;bases </TH>");
    fprintf(fh,"<TH> Bases <BR> covered </TH><TH> Minimum </TH>");
    fprintf(fh,"<TH> Maximum </TH><TH> Range </TH><TH> Mean </TH>");
    fprintf(fh,"<TH> Variance </TH><TH> Standard <BR> deviation </TH></TR>\n");
    }
else
    {
    fprintf(fh,"# Chrom\tData\tData");
    fprintf(fh,"\t# Data\tData");
    fprintf(fh,"\tBases\tMinimum");
    fprintf(fh,"\tMaximum\tRange\tMean");
    fprintf(fh,"\tVariance Standard\n");

    fprintf(fh,"#\tstart\tend");
    fprintf(fh,"\tvalues\tspan");
    fprintf(fh,"\tcovered\t");
    fprintf(fh,"\t\t\t");
    fprintf(fh,"\t\tdeviation\n");
    }
}

void wigPrintDataConstraint(struct wiggleDataStream *wds, FILE * fh)
/*	output string to file handle fh indicating current data constraint */
{
if (wds->useDataConstraint && wds->dataConstraint)
    {
    if (sameWord(wds->dataConstraint,"in range"))
	    fprintf (fh, "#\tdata values in range [%g : %g)\n",
		    wds->limit_0, wds->limit_1);
    else
	    fprintf (fh, "#\tdata values %s %g\n",
		    wds->dataConstraint, wds->limit_0);
    }
}

void statsPreamble(struct wiggleDataStream *wds, char *chrom,
    int winStart, int winEnd, unsigned span, unsigned long long valuesMatched,
	char *table2)
{
char num1Buf[64], num2Buf[64]; /* big enough for 2^64 (and then some) */

sprintLongWithCommas(num1Buf, BASE_1(winStart));
sprintLongWithCommas(num2Buf, winEnd);
printf("<P><B> Position: </B> %s:%s-%s</P>\n", chrom, num1Buf, num2Buf );
sprintLongWithCommas(num1Buf, winEnd - winStart);
printf("<P><B> Total Bases in view: </B> %s </P>\n", num1Buf);

if (wds->useDataConstraint)
    {
    if (sameWord(wds->dataConstraint,"in range"))
	printf("<P><B> Filter: %g <= (data value) < %g </B></P>\n",
		wds->limit_0, wds->limit_1);
    else
	printf("<P><B> Filter: (data value %s %g) </B> </P>\n",
		wds->dataConstraint, wds->limit_0);
    }
if (table2)
    printf("<P><B> Intersection with table: %s </B></P>\n", table2);

if (table2 && (valuesMatched == 0))
    {
    printf("<P><B> No data found in this intersection. </B></P>\n");
    }
else
    {
    if (valuesMatched == 0)
	{
	if ( (span * 3) > (winEnd - winStart))
	    {
	    printf("<P><B> Viewpoint has too few bases to calculate statistics. </B></P>\n");
	    printf("<P><B> Zoom out to at least %d bases to see statistics. </B></P>\n", 3 * span);
	    }
	else
	    printf("<P><B> No data found in this region. </B></P>\n");
	}
    else
	{
	unsigned printSpan = wds->stats->span;

	if ((span > 0) && (span < wds->stats->span))
	    printSpan = span;

	sprintLongWithCommas(num1Buf, wds->stats->count * printSpan);
	printf(
	    "<P><B> Statistics on: </B> %s <B> bases </B> (%% %.4f coverage)</P>\n",
	    num1Buf,
	    100.0*(wds->stats->count * printSpan)/(winEnd - winStart));
	}
    }
}

void wigStatsHeader(struct wiggleDataStream *wds, FILE * fh, boolean htmlOut)
/*	begin wiggle stats table */
{
if (htmlOut)
    {
    /* For some reason BORDER=1 does not work in our web.c nested table
     * scheme.  So use web.c's trick of using an enclosing table
     *	to provide a border.  
     */
    fprintf(fh,"<P><!--outer table is for border purposes-->" "\n"
	"<TABLE BGCOLOR=\"#" HG_COL_BORDER
	"\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>");

    fprintf (fh, "<TABLE COLS=12 BORDER=1 BGCOLOR=\"" HG_COL_INSIDE
	"\" ALIGN=CENTER HSPACE=0><TR>");
    if (wds->db)
	fprintf(fh, "<TH COLSPAN=6 ALIGN=LEFT> Database: %s </TH><TH COLSPAN=6 ALIGN=RIGHT> Table: %s </TH></TR>\n", wds->db, wds->tblName);
    if (wds->isFile)
	{
	if ( (stringIn("trash/ct_",wds->tblName)) ||
		(stringIn("trash/hgtct_",wds->tblName)))
	    fprintf(fh, "<TH COLSPAN=12 ALIGN=LEFT> custom track </TH></TR>\n" );
	else
	    fprintf(fh, "<TH COLSPAN=12 ALIGN=LEFT> from file %s </TH></TR>\n", wds->tblName);
	}
    wigStatsTableHeading(fh, htmlOut);
    }
else
    {
    if (wds->db)
	fprintf(fh, "#\t Database: %s, Table: %s\n",
		wds->db, wds->tblName);
    if (wds->isFile)
	fprintf(fh, "#\t from file, Table: %s\n", wds->tblName);

    wigStatsTableHeading(fh, htmlOut);
    }
}	/*	void wigStatsHeader()	*/

/*	strictly object methods following 	************************/
/*	PRIVATE	METHODS	************************************************/
static void addConstraint(struct wiggleDataStream *wds, char *left, char *right)
{
struct dyString *constrain = dyStringNew(256);
if (wds->sqlConstraint)
    dyStringPrintf(constrain, "%s AND ", wds->sqlConstraint);

dyStringPrintf(constrain, "%s \"%s\"", left, right);

freeMem(wds->sqlConstraint);	/*	potentially previously existing */
wds->sqlConstraint = cloneString(constrain->string);
dyStringFree(&constrain);
}

/*	*row[] is artifically one too big to allow for a potential bin
 *	column when reading files that may have it.
 */
static boolean nextRow(struct wiggleDataStream *wds, char *row[], int maxRow)
/*	read next wig row from sql query or lineFile
 *	FALSE return on no more data	*/
{
int numCols;

if (wds->isFile)
    {
    /*	the file may have a bin column, detect automatically and eliminate */
    numCols = lineFileChopNextTab(wds->lf, row, maxRow+1);
    if (numCols == maxRow+1)
	{
	int i;
	for (i = 1; i < (maxRow+1); ++i)
	    row[i-1] = row[i];
	}
    if (numCols < maxRow) return FALSE;
    verbose(VERBOSE_PER_VALUE_LEVEL, "#\tnumCols = %d, row[0]: %s, row[1]: %s, row[%d]: %s\n",
	numCols, row[0], row[1], maxRow-1, row[maxRow-1]);
    }
else
    {
    int i;
    char **sqlRow;
    sqlRow = sqlNextRow(wds->sr);
    if (sqlRow == NULL)
	return FALSE;
    /*	skip the bin column sqlRow[0]	*/
    for (i=1; i <= maxRow; ++i)
	{
	row[i-1] = sqlRow[i];
	}
    }
return TRUE;
}

static void findWibFile(struct wiggleDataStream *wds, char *file)
/* look for file in full pathname given, or in same directory */
{
wds->wibFile = cloneString(file);
wds->wibFH = open(wds->wibFile, O_RDONLY);
if (wds->wibFH == -1)
    {
    char *baseName = strrchr(wds->wibFile, '/');
    if (baseName)
	wds->wibFH = open(baseName+1, O_RDONLY);
    if ((NULL == baseName) || (wds->wibFH == -1))
	errAbort("openWibFile: failed to open %s", wds->wibFile);
    }
}

static void openWibFile(struct wiggleDataStream *wds, char *file)
{
if (wds->wibFile)
    {		/*	close and open only if different */
    if (differentString(wds->wibFile,file))
	{
	if (wds->wibFH > 0)
	    close(wds->wibFH);
	freeMem(wds->wibFile);
	findWibFile(wds, file);
	}
    }
else
    findWibFile(wds, file);

verbose(VERBOSE_HIGHEST, "#\topened wib file: %s\n", wds->wibFile);
}

static void setCompareByte(struct wiggleDataStream *wds,
	double lower, double range)
{
if (wds->limit_0 < lower)
    wds->ucLowerLimit = 0;
else if (wds->limit_0 > (lower+range))
    wds->ucLowerLimit = MAX_WIG_VALUE;
else
    wds->ucLowerLimit = MAX_WIG_VALUE * ((wds->limit_0 - lower)/range);

if (wds->limit_1 < lower)
    wds->ucUpperLimit = 0;
else if (wds->limit_1 > (lower+range))
    wds->ucUpperLimit = MAX_WIG_VALUE + 1;	/* +1 because in range is [) */
else
    wds->ucUpperLimit = MAX_WIG_VALUE * ((wds->limit_1 - lower)/range);

verbose(VERBOSE_HIGHEST, "#\twigSetCompareByte: [%g : %g] becomes [%d : %d]\n",
	lower, lower+range, wds->ucLowerLimit, wds->ucUpperLimit);
}

static void resetStats(float *lowerLimit, float *upperLimit, double *sumData,
	double *sumSquares, unsigned long *statsCount, long int *chromStart,
	long int *chromEnd)
{
*lowerLimit = INFINITY;
*upperLimit = -1.0 * INFINITY;
*sumData = 0.0;
*sumSquares = 0.0;
*statsCount = 0;
*chromStart = -1;
*chromEnd = 0;
}

static void accumStats(struct wiggleDataStream *wds, float lowerLimit,
	float upperLimit, double sumData, double sumSquares,
	unsigned statsCount, long int chromStart,
	long int chromEnd)
{
if (statsCount > 0)
    {
    struct wiggleStats *ws;

    AllocVar(ws);
    ws->chrom = cloneString(wds->currentChrom);
    ws->chromStart = chromStart;
    ws->chromEnd = chromEnd;
    ws->span = wds->currentSpan;
    ws->count = statsCount;
    ws->lowerLimit = lowerLimit;
    ws->dataRange = upperLimit - lowerLimit;
    ws->mean = sumData / (double) statsCount;
    ws->variance = 0.0;
    ws->stddev = 0.0;
    if (statsCount > 1)
	{
	ws->variance = (sumSquares -
	    ((sumData * sumData)/(double) statsCount)) /
		(double) (statsCount - 1);
	if (ws->variance > 0.0)
	    ws->stddev = sqrt(ws->variance);
	}
    slAddHead(&wds->stats, ws);
    }
}

static struct bed *bedElement(char *chrom, unsigned start, unsigned end,
        unsigned lineCount)
{
struct bed *bed;
char name[128];

AllocVar(bed);
bed->chrom = cloneString(chrom);
bed->chromStart = start;
bed->chromEnd = end;
snprintf(name, sizeof(name), "%s.%u",
    chrom, lineCount);
bed->name = cloneString(name);
return bed;
}

static void closeWibFile(struct wiggleDataStream *wds)
/*	if there is a Wib file open, close it	*/
{
if (wds->wibFH > 0)
    close(wds->wibFH);
wds->wibFH = -1;
if (wds->wibFile)
    freez(&wds->wibFile);
}

static void closeWigConn(struct wiggleDataStream *wds)
{
lineFileClose(&wds->lf);
closeWibFile(wds);	/*	closes only if it is open	*/
if (wds->conn)
    {
    sqlFreeResult(&wds->sr);
    sqlDisconnect(&wds->conn);
    }
if (wds->sqlConstraint)
    freez(&wds->sqlConstraint);	/*	always reconstructed at open time */
}

static void openWigConn(struct wiggleDataStream *wds)
/*	open connection to db or to a file, prepare SQL result for db */
{
if (!wds->tblName)
  errAbort("openWigConn: table name missing.  setDbTable before open.");

if (wds->isFile)
    {
    struct dyString *fileName = dyStringNew(256);
    lineFileClose(&wds->lf);	/*	possibly a previous file */
    /*	don't add .wig if it is already there, or use whatever filename
     *	was given	*/
    if (fileExists(wds->tblName))
	dyStringPrintf(fileName, "%s", wds->tblName);
    else
	dyStringPrintf(fileName, "%s.wig", wds->tblName);
    wds->lf = lineFileOpen(fileName->string, TRUE);
    dyStringFree(&fileName);
    }
else
    {
    struct dyString *query = dyStringNew(256);
    dyStringPrintf(query, "select * from %s", wds->tblName);
    if (wds->chrName)
	addConstraint(wds, "chrom =", wds->chrName);
    if (wds->winEnd)
	{
	char limits[256];
	safef(limits, ArraySize(limits), "%d", wds->winEnd );
	addConstraint(wds, "chromStart <", limits);
	safef(limits, ArraySize(limits), "%d", wds->winStart );
	addConstraint(wds, "chromEnd >", limits);
	}
    if (wds->spanLimit)
	{
	struct dyString *dyTmp = dyStringNew(256);
	dyStringPrintf(dyTmp, "%u", wds->spanLimit);
	addConstraint(wds, "span =", dyTmp->string);
	dyStringFree(&dyTmp);
	}
    if (wds->sqlConstraint)
	{
	dyStringPrintf(query, " where ");
	if (wds->winEnd)
	    {
	    hAddBinToQuery(wds->winStart, wds->winEnd, query);
	    }
	dyStringPrintf(query, " (%s)",
	    wds->sqlConstraint);
	}
    dyStringPrintf(query, " order by ");
    if (!wds->chrName)
	dyStringPrintf(query, " chrom ASC,");
    if (!wds->spanLimit)
	dyStringPrintf(query, " span ASC,");
    dyStringPrintf(query, " chromStart ASC");

    verbose(VERBOSE_SQL_ROW_LEVEL, "#\t%s\n", query->string);
    if (!wds->conn)
	{
	if (sameString(CUSTOM_TRASH,wds->db))
	    wds->conn = sqlConnect(CUSTOM_TRASH);
	else
	    wds->conn = sqlConnect(wds->db);
	}
    wds->sr = sqlGetResult(wds->conn,query->string);
    }
}


static void setDbTable(struct wiggleDataStream *wds, char * db, char *table)
/*	sets the DB and table name, determines if table is a file or not */
{
struct dyString *fileName = dyStringNew(256);

if (!table)
    errAbort("setDbTable: table specification missing");

/*	Check to see if there is a .wig file, or whatever name was given */
if (fileExists(table))
    dyStringPrintf(fileName, "%s", table);
else
    dyStringPrintf(fileName, "%s.wig", table);

/*	file present ignores db specification	*/
if ((db == (char *)NULL) && fileExists(fileName->string))
    wds->isFile = TRUE;
else
    {
    if (db)			/*	db can be NULL	*/
	wds->isFile = FALSE;	/*	assume it will be in the db */
    else
	errAbort("setDbTable: db is NULL and can not find file %s",
	    fileName->string);
    }
dyStringFree(&fileName);

freeMem(wds->db);		/*	potentially previously existing	*/
if (!wds->isFile && db)
	wds->db = cloneString(db);
freeMem(wds->tblName);		/*	potentially previously existing */
wds->tblName = cloneString(table);
}

static char *dateTimeStamp()
{
time_t now = time(NULL);
char *dateTime;

dateTime = sqlUnixTimeToDate(&now,TRUE);	/* TRUE == gmTime */
return dateTime;
}

static void outputIdentification(struct wiggleDataStream *wds, FILE *fh)
{
char *dateStamp = dateTimeStamp();

if (wds->db)
    fprintf (fh, "#\tdb: '%s', track: '%s', output date: %s UTC\n",
	wds->db, wds->tblName, dateStamp);
if (wds->isFile)
    fprintf (fh, "#\tfrom file input, output date: %s UTC\n",
	dateStamp);

freeMem(dateStamp);
}

static void showResolution(double resolution, FILE *fh)
{
if (resolution > 0.0)
    fprintf (fh, "#\tThis data has been compressed with a minor "
	"loss in resolution.\n" );
    fprintf (fh, "#\t(Worst case: %g)  The original source data\n",
	resolution);
    fprintf (fh, "#\t(before querying and compression) is available at \n"
	"#\t\thttp://hgdownload.cse.ucsc.edu/downloads.html\n");
}

static void showResolutionNoDownloads(double resolution, FILE *fh)
{
if (resolution > 0.0)
    fprintf (fh, "#\tThis data has been compressed with a minor "
	"loss in resolution.\n" );
    fprintf (fh, "#\t(Worst case: %g)\n", resolution);
}

static void showConstraints(struct wiggleDataStream *wds, FILE *fh)
{
if (wds->chrName)
    fprintf (fh, "#\tchrom specified: %s\n", wds->chrName);
if (wds->spanLimit)
    fprintf (fh, "#\tspan specified: %u\n", wds->spanLimit);
if (wds->winEnd)
    fprintf (fh, "#\tposition specified: %d-%d\n",
	BASE_1(wds->winStart), wds->winEnd);
if (wds->bedConstrained && !wds->chrName)
    fprintf (fh, "#\tconstrained by chr names and coordinates in bed list\n");
else if (wds->bedConstrained)
    fprintf (fh, "#\tconstrained by coordinates in bed list\n");
wigPrintDataConstraint(wds, fh);
}

static int arrayDataCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,winStart */
{
const struct wiggleArray *a = *((struct wiggleArray **)va);
const struct wiggleArray *b = *((struct wiggleArray **)vb);
int dif;
dif = chrStrippedCmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->winStart - b->winStart;
return dif;
}

static int asciiDataCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,span,chromStart. */
{
const struct wigAsciiData *a = *((struct wigAsciiData **)va);
const struct wigAsciiData *b = *((struct wigAsciiData **)vb);
int dif;
dif = chrStrippedCmp(a->chrom, b->chrom);
if (dif == 0)
    {
    dif = a->span - b->span;
    if (dif == 0)
	dif = a->data->chromStart - b->data->chromStart;
    }
return dif;
}

static int statsDataCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,span,chromStart. */
{
const struct wiggleStats *a = *((struct wiggleStats **)va);
const struct wiggleStats *b = *((struct wiggleStats **)vb);
int dif;
dif = chrStrippedCmp(a->chrom, b->chrom);
if (dif == 0)
    {
    dif = a->span - b->span;
    if (dif == 0)
	dif = a->chromStart - b->chromStart;
    }
return dif;
}

/*	currently this dataArrayOut is private, but it may become public */
static void dataArrayOut(struct wiggleDataStream *wds, char *fileName,
	boolean rawDataOut, boolean sort)
/*	print to fileName ascii data values from the first list element
 *	of the data "array" results	*/
{
FILE * fh;
fh = mustOpen(fileName, "w");
if (wds->array)
    {
    unsigned pointCount;
    unsigned chromPosition;
    unsigned inx;
    float *dataPtr;

    if (sort)
	slSort(&wds->array, arrayDataCmp);

    dataPtr = wds->array->data;
    /*	user visible coordinates are wds->offset relative/based */
    chromPosition = wds->array->winStart + wds->offset;
    pointCount = wds->array->winEnd - wds->array->winStart;
    for (inx = 0; inx < pointCount; ++inx)
	{
	if (!isnan(*dataPtr))
	    {
	    if (rawDataOut)
		fprintf (fh, "%g\n", *dataPtr);
	    else
		fprintf (fh, "%u\t%g\n", chromPosition, *dataPtr);
	    }
	++dataPtr;
	++chromPosition;
	}

    }
else
    {
    if (!rawDataOut)
	{
	showConstraints(wds, fh);
	fprintf(fh, "#\tdataArray: no data points found\n");
	}
    }
carefulClose(&fh);
}	/*	static void dataArrayOut()	*/

/*	PUBLIC	METHODS   **************************************************/
static void setMaxOutput(struct wiggleDataStream *wds,
	unsigned long long maxOut)
/*	set the maximum # of values to return */
{
wds->maxOutput = maxOut;
verbose(VERBOSE_CHR_LEVEL,"#\tsetMaxOutput: %llu\n", wds->maxOutput);
}

static void setPositionConstraint(struct wiggleDataStream *wds,
	int winStart, int winEnd)
/*	both 0 means no constraint	*/
{
if ((!wds->isFile) && wds->conn)
    {	/*	this should not happen	*/
    errAbort("setPositionConstraint: not allowed after openWigConn()");
    }
/*	keep them in proper order	*/
if (winStart > winEnd)
    {
    wds->winStart = winEnd;
    wds->winEnd = winStart;
    }
else if ((winEnd > 0) && (winStart == winEnd))
    errAbort(
	"setPositionConstraint: can not have winStart == winEnd (%d == %d)",
	    winStart, winEnd);
else
    {
    wds->winStart = winStart;
    wds->winEnd = winEnd;
    }
verbose(VERBOSE_SQL_ROW_LEVEL,"#\tsetPosition: %d - %d\n", wds->winStart, wds->winEnd);
}

static void setChromConstraint(struct wiggleDataStream *wds, char *chr)
{
freeMem(wds->chrName);		/*	potentially previously existing */
wds->chrName = cloneString(chr);
}

static void setSpanConstraint(struct wiggleDataStream *wds, unsigned span)
{
wds->spanLimit = span;
}

static void freeConstraints(struct wiggleDataStream *wds)
/*	free the position, span, chrName and data constraints */
{
wds->spanLimit = 0;
wds->setPositionConstraint(wds, 0, 0);
if (wds->chrName)
    freez(&wds->chrName);
if (wds->dataConstraint)
    freez(&wds->dataConstraint);
wds->wigCmpSwitch = wigNoOp_e;
wds->limit_0 = wds->limit_1 = 0.0;
wds->ucLowerLimit = wds->ucUpperLimit = 0;
if (wds->sqlConstraint)
    freez(&wds->sqlConstraint);
wds->useDataConstraint = FALSE;
wds->bedConstrained = FALSE;
}

static void freeAscii(struct wiggleDataStream *wds)
/*	free the wiggleAsciiData list	*/
{
if (wds->ascii)
    {
    struct wigAsciiData *el, *next;

    for (el = wds->ascii; el != NULL; el = next)
	{
	next = el->next;
	freeMem(el->chrom);
	freeMem(el->data);
	freeMem(el);
	}
    }
wds->ascii = NULL;
}

static void freeBed(struct wiggleDataStream *wds)
/*	free the wiggle bed list	*/
{
bedFreeList(&wds->bed);
}

static void freeArray(struct wiggleDataStream *wds)
/*	free the wiggleArray list	*/
{
if (wds->array)
    {
    struct wiggleArray *wa;
    struct wiggleArray *next;

    for (wa = wds->array; wa; wa = next)
	{
	next = wa->next;
	freeMem(wa->chrom);
	freeMem(wa->data);
	freeMem(wa);
	}
    }
wds->array = NULL;
}

static void freeStats(struct wiggleDataStream *wds)
/*	free the wiggleStats list	*/
{
if (wds->stats)
    {
    struct wiggleStats *el, *next;

    for (el = wds->stats; el != NULL; el = next)
	{
	next = el->next;
	freeMem(el->chrom);
	freeMem(el);
	}
    }
wds->stats = NULL;
}

/*	the double comparison functions
 *	used to check the wiggle SQL rows which are a bucket of values
 *	between *lower and *upper.  Therefore, the value to be checked
 *	which is in wds->limit_0 (and wds->limit_1 in the case of
 *	a range) needs to be compared to the bucket of values.  If it
 *	falls within the specified range, then it is considered to be in
 *	that bucket.
 */
/* InRange means the SQL row begins before the limit_1 (lower<limit_1)
 *	 and the row ends after the limit_0 (upper>=limit_0)
 *	i.e. there is at least some overlap of the range
 *	This is a half open inquiry: [ limit_0 : limit_1 )
 */
/* LessThan means:  the row begins before the limit_0 (value<limit_0)
 *	i.e. there are data values below the specified limit_0
 */
/* LessEqual means:  the row begins at or before the limit_0 (value<=limit_0)
 *	i.e. there are data values at or below the specified limit_0
 */
/* Equal means:  similar to InRange, the test value limit_0 can be found
 * in the SQL row, i.e. lower <= limit_0 <= upper
 */
/* NotEqual means:  the opposite of Equal, the test value limit_0 can not
 *	be found in the SQL row, i.e. (limit_0 < lower) or (upper < limit_0)
 */
/* GreaterEqual means:  the row ends at or after the limit_0 (limit_0<=upper)
 *	i.e. there are data values at or above the specified limit_0
 */
/* GreaterEqual means:  the row ends after the limit_0 (limit_0<upper)
 *	i.e. there are data values above the specified limit_0
 */
/*	the unsigned char comparison functions
 *	Unlike the above, these are straighforward, just compare the
 *	byte values
 */

static void wigSetCompareFunctions(struct wiggleDataStream *wds)
{
if (!wds->dataConstraint)
    return;

if (sameWord(wds->dataConstraint,"<"))
    wds->wigCmpSwitch = wigLessThan_e;
else if (sameWord(wds->dataConstraint,"<="))
    wds->wigCmpSwitch = wigLessEqual_e;
else if (sameWord(wds->dataConstraint,"="))
    wds->wigCmpSwitch = wigEqual_e;
else if (sameWord(wds->dataConstraint,"!="))
    wds->wigCmpSwitch = wigNotEqual_e;
else if (sameWord(wds->dataConstraint,">="))
    wds->wigCmpSwitch = wigGreaterEqual_e;
else if (sameWord(wds->dataConstraint,">"))
    wds->wigCmpSwitch = wigGreaterThan_e;
else if (sameWord(wds->dataConstraint,"in range"))
    wds->wigCmpSwitch = wigInRange_e;
else
    errAbort("wigSetCompareFunctions: unknown constraint: '%s'",
	wds->dataConstraint);
verbose(VERBOSE_CHR_LEVEL, "#\twigSetCompareFunctions: set to '%s'\n", wds->dataConstraint);
}

static void setDataConstraint(struct wiggleDataStream *wds,
	char *dataConstraint, double lowerLimit, double upperLimit)
{
freeMem(wds->dataConstraint);	/*	potentially previously existing	*/
wds->dataConstraint = cloneString(dataConstraint);

if (differentWord(wds->dataConstraint, "in range"))
    {
    wds->limit_0 = lowerLimit;
    }
else
    {
    if (lowerLimit < upperLimit)
	{
	wds->limit_0 = lowerLimit;
	wds->limit_1 = upperLimit;
	}
    else if (!(upperLimit < lowerLimit))
      errAbort("wigSetDataConstraint: upper and lower limits are equal: %g == %g",
	    lowerLimit, upperLimit);
    else
	{
	wds->limit_0 = upperLimit;
	wds->limit_1 = lowerLimit;
	}
    }
wigSetCompareFunctions(wds);
wds->useDataConstraint = TRUE;
}

static unsigned long long getData(struct wiggleDataStream *wds, char *db,
	char *table, int operations)
/* getData - read and return wiggle data	*/
{
char *row[WIGGLE_NUM_COLS+1];	/*	potentially a bin column too */
unsigned long long rowCount = 0;
unsigned long long validData = 0;
unsigned long long valuesMatched = 0;
unsigned long long noDataBytes = 0;
unsigned long long bytesSkipped = 0;
boolean doRawStats = FALSE;
boolean doStats = FALSE;
boolean doBed = FALSE;
boolean doAscii = FALSE;
boolean doDataArray = FALSE;
boolean doNoOp = FALSE;
boolean skipDataRead = FALSE;	/*	may need this later	*/
float lowerLimit = INFINITY;
float upperLimit = -1.0 * INFINITY;
double sumData = 0.0;
double sumSquares = 0.0;
unsigned long statsCount = 0;
long int chromStart = -1;
long int chromEnd = 0;
boolean summaryOnly = TRUE;
unsigned bedElStart = 0;
unsigned bedElEnd = 0;
unsigned bedElCount = 0;
boolean firstSpanDone = FALSE;	/*	to prevent multiple bed lists */
float *dataArrayPtr = NULL;	/*	to access the data array values */
unsigned dataArrayPosition = 0;	/*  marches thru all from beginning to end */
struct wiggle *wiggle;		/*	one SQL data read results	*/
boolean maxReached = FALSE;

doAscii = operations & wigFetchAscii;
doDataArray = operations & wigFetchDataArray;
doBed = operations & wigFetchBed;
doRawStats = operations & wigFetchRawStats;
doStats = (operations & wigFetchStats) || doRawStats;
doNoOp = operations & wigFetchNoOp;

/*	for timing purposes, allow the wigFetchNoOp to go through */
if (doNoOp)
    doDataArray = doBed = doStats = doAscii = FALSE;

if (! (doNoOp || doAscii || doDataArray || doBed || doStats) )
    {
	verbose(VERBOSE_CHR_LEVEL, "getData: no type of data fetch requested ?\n");
	return(valuesMatched);	/*	NOTHING ASKED FOR ?	*/
    }

/*	not going to do this without a range specified and a chr name */
if (doDataArray && wds->winEnd && wds->chrName)
    {
    long startTime = 0;
    struct wiggleArray *wa;
    size_t inx, size; 
    size_t winSize;
    unsigned long long hugeSize;

    AllocVar(wa);
    wa->chrom = cloneString(wds->chrName);
    wa->winStart = wds->winStart;
    wa->winEnd = wds->winEnd;
    winSize = wa->winEnd - wa->winStart;
    hugeSize = (unsigned long long)sizeof(float) * (unsigned long long)winSize;

    if (sizeof(size_t) == 4)
	{
	if (hugeSize > 2100000000)
	    {
		warn("ERROR: Can not perform requested data operation.<BR>");
		errAbort("Not enough memory (requested: %llu bytes)", hugeSize);
	    }
	}
    size = sizeof(float) * winSize;
    /*	good enough for 500,000,000 bases	*/
    setMaxAlloc((size_t)2100000000);	/*2^31 = 2,147,483,648 */
    if (verboseLevel() >= VERBOSE_CHR_LEVEL)
	startTime = clock1000();
    wa->data = (float *)needLargeMem(size);
    dataArrayPtr = wa->data;
    /*	this init loop turns out to be quite fast, even for huge arrays */
    for (inx = 0; inx < winSize; ++inx)
	*dataArrayPtr++ = NAN;
    dataArrayPtr = wa->data;
    dataArrayPosition = wa->winStart;
    slAddHead(&wds->array, wa);
    if (verboseLevel() >= VERBOSE_CHR_LEVEL)
	{
	long et = clock1000() - startTime;
	verbose(VERBOSE_CHR_LEVEL,
	    "#\tgetData: created data array for %lld values (%llu b) in %ld ms\n",
		(unsigned long long)winSize, (unsigned long long)size, et);
	verbose(VERBOSE_CHR_LEVEL,
	    "#\tdata array begins at %#lx, last at %#lx, size: %#lx\n",
		(unsigned long) dataArrayPtr,
			(unsigned long) (((unsigned long) dataArrayPtr) +
				((unsigned long) size)), (unsigned long) size);
	}
    }
else if (doDataArray && !(doNoOp || doAscii || doBed || doStats))
    {
	verbose(VERBOSE_CHR_LEVEL,
		"getData: must specify a chrom range to FetchDataArray\n");
	return(valuesMatched);	/*	NOTHING ASKED FOR ?	*/
    }

setDbTable(wds, db, table);
openWigConn(wds);

if (doStats && wds->useDataConstraint)
    summaryOnly = FALSE;
if (doDataArray)
    summaryOnly = FALSE;
if (doAscii)
    summaryOnly = FALSE;
if (doBed)
    summaryOnly = FALSE;
if (!wds->isFile && wds->winEnd)
    summaryOnly = FALSE;

/*	nextRow() produces the next SQL row from either DB or file.
 *
 *	The unusual use of wiggleFree() as the third element of this for()
 *	loop is to take care of the condition when the 'continue;'
 *	statements are used to skip the loop and not have to worry about
 *	what happens at the end of the loop.  the wiggleFree() must be
 *	done at the end of each loop even in the case of 'continue;' or
 *	not.  This is the single instance of wiggleFree() in the entire
 *	source file, as it should be.
 */
	    
for ( ; (!maxReached) && nextRow(wds, row, WIGGLE_NUM_COLS);
	wiggleFree(&wiggle) )
    {
    struct wigAsciiData *wigAscii = NULL;
    struct asciiDatum *asciiOut = NULL;	/* to address data[] in wigAsciiData */
    unsigned chromPosition;
    boolean range0TakesAll = FALSE;

    if (doRawStats)
	{
	accumStats(wds, lowerLimit, upperLimit, sumData, sumSquares,
	    statsCount, chromStart, chromEnd);
	resetStats(&lowerLimit, &upperLimit, &sumData, &sumSquares,
	    &statsCount, &chromStart, &chromEnd);
	}

    ++rowCount;
    wiggle = wiggleLoad(row);
    /*	constraints have to be done manually for files	*/
    if (wds->isFile)
	{
	/*	if chrName is not correct, or span is not correct,
	 *	or outside of window, go to next SQL row
	 */
	if (
	    ((wds->chrName) && (differentString(wds->chrName,wiggle->chrom))) ||
	    ((wds->spanLimit) && (wds->spanLimit != wiggle->span)) ||
	    ((wds->winEnd) && ((wiggle->chromStart >= wds->winEnd) ||
		(wiggle->chromEnd < wds->winStart)) )
	   )
	    {
	    bytesSkipped += wiggle->count;
	    continue;	/*	next SQL row	*/
	    }
	}

    chromPosition = wiggle->chromStart;

    /*	this will be true the very first time for both reasons	*/
    if ( (wds->currentSpan != wiggle->span) || 
	    (wds->currentChrom &&
		differentString(wds->currentChrom, wiggle->chrom)))
	{
	/*	if we have been working on one, then doBed	*/
	if (!firstSpanDone && doBed && wds->currentChrom)
	    {
	    if (bedElEnd > bedElStart)
		{
		struct bed *bedEl;
		bedEl = bedElement(wds->currentChrom,
			bedElStart, bedElEnd, ++bedElCount);
		slAddHead(&wds->bed, bedEl);
		}
	    bedElStart = 0;
	    bedElEnd = 0;
	    bedElCount = 0;
	    }
	if (wds->currentSpan && (wds->currentSpan != wiggle->span))
	    firstSpanDone = TRUE;
	if (wds->currentChrom &&
		differentString(wds->currentChrom, wiggle->chrom))
	    firstSpanDone = FALSE;
	/*	if we have been working on one, then doStats	*/
	if (doStats && wds->currentChrom)
	    {
	    accumStats(wds, lowerLimit, upperLimit, sumData, sumSquares,
		statsCount, chromStart, chromEnd);
	    resetStats(&lowerLimit, &upperLimit, &sumData, &sumSquares,
		&statsCount, &chromStart, &chromEnd);
	    }
	freeMem(wds->currentChrom);
	/*	set them whether they are changing or not, doesn't matter */
	wds->currentChrom = cloneString(wiggle->chrom);
	wds->currentSpan = wiggle->span;
	}

    /*	it may seem inefficient to make one of these for each SQL row,
     *	but the alternative would require expanding the data area for
     *	each row and thus a re-alloc - probably more expensive than
     *	just making one of these for each row.
     */
    if (doAscii)
	{
	AllocVar(wigAscii);
	wigAscii->chrom = cloneString(wiggle->chrom);
	wigAscii->span = wiggle->span;
	wigAscii->count = 0;	/* will count up as values added */
	wigAscii->dataRange = wiggle->dataRange;  /* for resolution calc */
	wigAscii->data = (struct asciiDatum *) needMem((size_t)
	    (sizeof(struct asciiDatum) * wiggle->validCount));
	asciiOut = wigAscii->data;
	slAddHead(&wds->ascii,wigAscii);
	}

    verbose(VERBOSE_SQL_ROW_LEVEL,
"#\trow: %llu, start: %u, data range: %g: [%g:%g], resolution: %g per bin\n",
	    rowCount, wiggle->chromStart, wiggle->dataRange,
	    wiggle->lowerLimit, wiggle->lowerLimit+wiggle->dataRange,
	    wiggle->dataRange/(double)MAX_WIG_VALUE);
    if (wds->useDataConstraint)
	{
	boolean takeIt = FALSE;
	switch (wds->wigCmpSwitch)
	    {
	    case wigNoOp_e:
		takeIt = TRUE;
		break;
	    case wigInRange_e:
		takeIt = (wiggle->lowerLimit < wds->limit_1) &&
		    ((wiggle->lowerLimit + wiggle->dataRange) >=
			    wds->limit_0);
		break;
	    case wigLessThan_e:
		takeIt = wiggle->lowerLimit < wds->limit_0;
		break;
	    case wigLessEqual_e:
		takeIt = wiggle->lowerLimit <= wds->limit_0;
		break;
	    case wigEqual_e:
		takeIt = (wiggle->lowerLimit <= wds->limit_0) &&
		    (wds->limit_0 <=
			(wiggle->lowerLimit + wiggle->dataRange));
		break;
	    case wigNotEqual_e:
		takeIt = (wds->limit_0 < wiggle->lowerLimit) ||
		    ((wiggle->lowerLimit + wiggle->dataRange) <
			wds->limit_0);
		break;
	    case wigGreaterEqual_e:
	       takeIt = wds->limit_0 <=
		    (wiggle->lowerLimit + wiggle->dataRange);
		break;
	    case wigGreaterThan_e:
	       takeIt = wds->limit_0 <
		    (wiggle->lowerLimit + wiggle->dataRange);
		break;
	    default:
		errAbort("wig_getData: illegal wigCmpSwitch ? %#x",
		    wds->wigCmpSwitch);
		break;
	    }	/*	switch (wds->wigCmpSwitch)	*/
	if (0.0 == wiggle->dataRange)
	    range0TakesAll = takeIt;
	if (takeIt)
	    setCompareByte(wds, wiggle->lowerLimit, wiggle->dataRange);
	else
	    {
	    verbose(VERBOSE_HIGHEST,
		"#\tthis row fails compare, next SQL row\n");
	    bytesSkipped += wiggle->count;
	    continue;	/*	next SQL row	*/
	    }
	}
    if (summaryOnly && doStats)
	{
	/*	record maximum limits of data values	*/
	if (wiggle->lowerLimit < lowerLimit)
	    lowerLimit = wiggle->lowerLimit;
	if (wiggle->lowerLimit+wiggle->dataRange > upperLimit)
	    upperLimit = wiggle->lowerLimit+wiggle->dataRange;
	sumData += wiggle->sumData;
	sumSquares += wiggle->sumSquares;
	/*	record maximum extents	*/
	if ((chromStart < 0)||(chromStart > wiggle->chromStart))
	    chromStart = wiggle->chromStart;
	if (chromEnd < wiggle->chromEnd)
	    chromEnd = wiggle->chromEnd;
	statsCount += wiggle->validCount;
	valuesMatched += wiggle->validCount;
	bytesSkipped += wiggle->count;
	continue;	/*	next SQL row	*/
	}
    if (!skipDataRead)
	{
	int j;	/*	loop counter through readData	*/
	unsigned char *datum;    /* to walk through readData bytes */
	unsigned char *readData;    /* the bytes read in from the file */
	size_t bytesToRead;	/* to check read validity */
	size_t bytesRead;	/* to check read validity */

	openWibFile(wds, wiggle->file);
		    /* possibly open a new wib file */
	readData = (unsigned char *) needMem((size_t) (wiggle->count + 1));
	wds->bytesRead += wiggle->count;
	lseek(wds->wibFH, wiggle->offset, SEEK_SET);
	bytesToRead = (size_t) wiggle->count * (size_t) sizeof(unsigned char);

	bytesRead = read(wds->wibFH, readData, bytesToRead);

	if (bytesToRead != bytesRead)
	    {
	    errAbort("wig_getData: failed to read %llu bytes from %s\n",
		(unsigned long long)bytesToRead, wiggle->file);
	    }

	verbose(VERBOSE_PER_VALUE_LEVEL,
		"#\trow: %llu, reading: %u bytes\n", rowCount, wiggle->count);

	/*	The third element of the for() statement takes care of the end
	 *	of loop operations for the case of the 'continue;'
	 *	statement as well as the normal end of loop business
	 */
	datum = readData;
	for (j = 0; j < wiggle->count;
		++datum, chromPosition += wiggle->span, ++j)
	    {
	    if (wds->maxOutput)
		{
		if (doAscii)
		    {
		    /*	previous results to this fetch are wds->valuesMatched
		     *	this result is currently at valuesMatched
		     */
		    if (wds->maxOutput <= (wds->valuesMatched + valuesMatched))
			{
			maxReached = TRUE;
			break;
			}
		    }
		if (doBed)
		    {
		    /*	previous results to this fetch are
		     *	wds->totalBedElements, this result is currently
		     *	at bedElCount.  The -1 is because there is
		     *	one more bed element to go upon exit from
		     *	this loop.
		     */
		    if ( (wds->totalBedElements + bedElCount) >=
				(wds->maxOutput - 1) )
			{
			maxReached = TRUE;
			break;
			}
		    }
		}
	    if (*datum != WIG_NO_DATA)
		{
		boolean takeIt = FALSE;

		if (wds->winEnd)  /* non-zero means a range is in effect */
		    {	/*	do not allow item (+span) to run over winEnd */
		    unsigned span = wiggle->span;

			/*	if only doing data array, span is 1 	*/
		    if (doDataArray && !(doStats || doBed || doStats))
			span = 1;

		    if ( (chromPosition < wds->winStart) ||
			((chromPosition+span) > wds->winEnd) )
			continue;	/*	next *datum	*/
		    }
		++validData;
		switch (wds->wigCmpSwitch)
		    {
		    case wigNoOp_e:
			takeIt = TRUE;
			break;
		    case wigInRange_e:
			takeIt = (*datum < wds->ucUpperLimit) &&
			    (*datum >= wds->ucLowerLimit);
			break;
		    case wigLessThan_e:
			takeIt = *datum < wds->ucLowerLimit;
			break;
		    case wigLessEqual_e:
			takeIt = *datum <= wds->ucLowerLimit;
			break;
		    case wigEqual_e:
			takeIt = *datum == wds->ucLowerLimit;
			break;
		    case wigNotEqual_e:
			takeIt = *datum != wds->ucLowerLimit;
			break;
		    case wigGreaterEqual_e:
			takeIt = *datum >= wds->ucLowerLimit;
			break;
		    case wigGreaterThan_e:
			takeIt = *datum > wds->ucLowerLimit;
			break;
		    default:
			errAbort("wig_getData: illegal wigCmpSwitch ? %#x",
			    wds->wigCmpSwitch);
			break;
		    }	/*	switch (wds->wigCmpSwitch)	*/
		if (range0TakesAll || takeIt)
		    {
		    float value = 0.0;
		    ++valuesMatched;
		    if (doAscii || doStats || doDataArray || doNoOp)
			value = BIN_TO_VALUE(*datum,
					wiggle->lowerLimit,wiggle->dataRange);
		    if (doAscii)
			{
			asciiOut->value = value;
			asciiOut->chromStart = chromPosition;
			++asciiOut;
			++wigAscii->count;
			}
		    if (!firstSpanDone && doDataArray)
			{
			unsigned span = wiggle->span;
			unsigned inx;
			long long gap = (long long) chromPosition -
				(long long) dataArrayPosition;

			if (verboseLevel() >= VERBOSE_CHR_LEVEL)
			    {
			    if (chromPosition < dataArrayPosition)
				verbose(VERBOSE_CHR_LEVEL,
		"#\tWARNING: coordinates out of order %s %u < %u\n",
			wiggle->chrom, chromPosition, dataArrayPosition);
			    }

			dataArrayPosition += gap;

			/*	special case squeezing into the end	*/
			if ((dataArrayPosition + span) > wds->winEnd)
				span = wds->winEnd - dataArrayPosition;
				
			dataArrayPtr += gap;	/* move up, leave NaN's */
			verbose(VERBOSE_PER_VALUE_LEVEL, "#\t%u-%u <- %g\n",
				dataArrayPosition,
				dataArrayPosition + span, value);
			/*	fill all chrom positions for this element */
			for (inx = 0; inx < span; ++inx)
				*dataArrayPtr++ = value;
			dataArrayPosition += span;
			}
		    /*	beware, coords must come in sequence for this to
		     *	work.  The original SQL query should be ordering
		     *	things properly.  We only do the first span
		     *	encountered, it should be the lowest due to the
		     *	SQL query.
		     */
		    if (!firstSpanDone && doBed)
			{
			/*	as long as they are continuous, keep
			 *	extending the end
			 */
			if (chromPosition == bedElEnd)
			    bedElEnd += wiggle->span;
			else
			    {	/*	if one was collected, output it */
			    if (bedElEnd > bedElStart)
				{
				struct bed *bedEl;
				bedEl = bedElement(wiggle->chrom,
					bedElStart, bedElEnd, ++bedElCount);
				slAddHead(&wds->bed, bedEl);
				}
			    /*	start a new element here	*/
			    bedElStart = chromPosition;
			    bedElEnd = bedElStart + wiggle->span;
			    }
			}
		    if (doStats)
			{
			/*	record maximum data limits	*/
			if (value < lowerLimit)
			    lowerLimit = value;
			if (value > upperLimit)
			    upperLimit = value;
			sumData += value;
			sumSquares += value * value;
			/*	record maximum extents	*/
			if ((chromStart < 0)||(chromStart > chromPosition))
			    chromStart = chromPosition;
			if (chromEnd < (chromPosition + wiggle->span))
			    chromEnd = chromPosition + wiggle->span;
			++statsCount;
			}
		    }
		}	/*	if (*datum != WIG_NO_DATA)	*/
	    else
		{
		++noDataBytes;
		}
	    }	/*	for (j = 0; j < wiggle->count; ++j)	*/
	freeMem(readData);
	}	/*	if (!skipDataRead)	*/
    }		/*	for ( ; nextRow(wds, row, WIGGLE_NUM_COLS); ... ) */

/*	there may be one last bed element to output	*/
if (!firstSpanDone && doBed)
    {
    if (bedElEnd > bedElStart)
	{
	struct bed *bedEl;
	bedEl = bedElement(wds->currentChrom,
		bedElStart, bedElEnd, ++bedElCount);
	slAddHead(&wds->bed, bedEl);
	}
    }

/*	there are accumulated stats to complete	*/
if (doStats)
    {
    accumStats(wds, lowerLimit, upperLimit, sumData, sumSquares,
	statsCount, chromStart, chromEnd);
    resetStats(&lowerLimit, &upperLimit, &sumData, &sumSquares,
	&statsCount, &chromStart, &chromEnd);
    }

wds->rowsRead += rowCount;
wds->validPoints += validData;
wds->valuesMatched += valuesMatched;
wds->noDataPoints += noDataBytes;
wds->bytesSkipped += bytesSkipped;
wds->totalBedElements += bedElCount;

closeWigConn(wds);

return(valuesMatched);
}	/*	unsigned long long getData()	*/

static float *asciiToDataArray(struct wiggleDataStream *wds,
	unsigned long long count, size_t *returned)
/*	convert the AsciiData list to a float array */ 
{
float *floatArray = NULL;
float *fptr = NULL;
unsigned long long valuesDone = 0;
struct wigAsciiData *asciiData = NULL;

if (count < 1)
    return floatArray;

/*	good enough for 500,000,000 bases	*/
setMaxAlloc((size_t)2100000000);	/*2^31 = 2,147,483,648 */

AllocArray(floatArray, count);
fptr = floatArray;

/*	the (valuesDone <= count) condition is for safety */
for (asciiData = wds->ascii; asciiData && (valuesDone < count);
	asciiData = asciiData->next)
    {
    if (asciiData->count)
	{
	struct asciiDatum *data;
	unsigned i;

	data = asciiData->data;
	for (i = 0; (i < asciiData->count)&&(valuesDone < count); ++i)
	    {
	    *fptr++ = data->value;
	    ++data;
	    ++valuesDone;
	    }
	}
    }

if (returned)
    *returned = valuesDone;

return floatArray;
}

static unsigned long long getDataViaBed(struct wiggleDataStream *wds, char *db,
	char *table, int operations, struct bed **bedList)
/* getDataViaBed - constrained by the bedList	*/
{
unsigned long long valuesMatched = 0;
unsigned long long prevValuesMatched = 0;

if (bedList && *bedList)
    {
    struct bed *filteredBed = NULL;
    boolean chromConstraint = ((char *)NULL != wds->chrName);
    boolean positionConstraints = (0 != wds->winEnd);
    int saveWinStart = 0;
    int saveWinEnd = 0;
    char * saveChrName = NULL;
    struct slName *chromList = NULL;
    struct slName *chr;
    struct hash *chromSizes = NULL;
    struct { unsigned long chrStart; unsigned long chrEnd; } *chrStartEnd;
    unsigned long long valuesFound = 0;
    boolean maxReached = FALSE;
    unsigned long long prevMaxOutput = 0;
    

    /* remember these constraints so we can reset them afterwards
     * because we are going to use them for our own internal uses here.
     */
    saveWinStart = wds->winStart;
    saveWinEnd = wds->winEnd;

    chromSizes = newHash(0);

    /*	Determine what will control which chrom to do.  If a chrom
     *	constraint has been set then that is the one to do.  If it has
     *	not been set, then allow the bedList to be the list of chroms to
     *	do.  This business finds the maximum extent for each chrom in
     *	the bedList.
     */
    if (chromConstraint)
	{
	saveChrName = cloneString(wds->chrName);
	chr = newSlName(wds->chrName);
	slAddHead(&chromList, chr);
	AllocVar(chrStartEnd);
	chrStartEnd->chrStart = wds->winStart;
	chrStartEnd->chrEnd = wds->winEnd;
	hashAdd(chromSizes, wds->chrName, chrStartEnd);
	}
    else
	{
	struct bed *bed = NULL;
	for (bed = *bedList; bed; bed = bed->next)
	    {
	    /*  out of order bed file ?  Maybe already saw this name */
	    if (hashLookup(chromSizes, bed->chrom) != NULL)
		{
		chrStartEnd = hashFindVal(chromSizes, bed->chrom); 
		chrStartEnd->chrStart = min(bed->chromStart,chrStartEnd->chrStart);
		chrStartEnd->chrEnd = max(bed->chromEnd, chrStartEnd->chrEnd);
		}
	    else
		{
		chr = newSlName(bed->chrom);
		slAddHead(&chromList, chr);
		AllocVar(chrStartEnd);
		chrStartEnd->chrStart = bed->chromStart;
		chrStartEnd->chrEnd = bed->chromEnd;
		hashAdd(chromSizes, bed->chrom, chrStartEnd);
		}
	    }
	slReverse(&chromList);
	}

    /*	OK, the chromList and chromSizes hash have been created, run
     *	through that list.  This may seem unusual, but if the caller has
     *	set position constraints but no chr constraint, allow those
     *	constraints to override
     *	the bedList coordinates, even though we may be running through a
     *	number of chroms.  That's what they wanted, that's what they get.
     *	e.g. they wanted the first 10,000 bases of each chrom,
     *		winStart=0, winEnd = 10,000, no chrName specified
     */
    for (chr = chromList; (!maxReached) && chr; chr = chr->next)
	{
	long startTime = 0;
	float *fptr;
	boolean doAscii = FALSE;
	boolean doBed = FALSE;
	boolean doStats = FALSE;
	boolean doNoOp = FALSE;
	boolean doDataArray = FALSE;
	struct bed *bed;
	unsigned long bedExtent = 0;	/* from first thru last element of bedList*/
	unsigned long bedStart = 0;
	unsigned long bedEnd = 0;
	unsigned long bedPosition = 0;
	char *bedArray;	/*	meaning is boolean but	*/
	char *boolPtr;	/*	boolean is 4 bytes, char is 1	*/
	unsigned long bedArraySize = 0;
	unsigned long winStart = 0;
	unsigned long winEnd = 0;
	unsigned long winExtent = 0;
	unsigned long bedMarked = 0;
	unsigned long asciiDataSizeLimit = 0;
	unsigned long chromPosition = 0;
	unsigned long bedElStart = 0;
	unsigned long bedElEnd = 0;
	unsigned long bedElCount = 0;
	float lowerLimit = INFINITY;		/*	stats accumulators */
	float upperLimit = -1.0 * INFINITY;	/*	stats accumulators */
	double sumData = 0.0;			/*	stats accumulators */
	double sumSquares = 0.0;		/*	stats accumulators */
	unsigned long statsCount = 0;		/*	stats accumulators */
	long int chromStart = -1;		/*	stats accumulators */
	long int chromEnd = 0;			/*	stats accumulators */
	size_t dataArraySize = 0;

	doDataArray = operations & wigFetchDataArray;

	/*	set chrom name constraint	*/
	wds->setChromConstraint(wds, (char *)&chr->name);

	chrStartEnd = hashFindVal(chromSizes, wds->chrName); 
	if (NULL == chrStartEnd)
	    errAbort("getDataViaBed: constructed hash of bed coords is broken");
	/* This may seem unusual, but if the caller has
	 *	set position constraints, allow those constraints to override
	 *	the bedList coordinates, even though we may be running through
	 *	a number of chroms.
	 */
	if (saveWinEnd)
	    {
	    winStart = saveWinStart;
	    winEnd = saveWinEnd;
	    }
	else
	    {
	    winStart = chrStartEnd->chrStart;
	    winEnd = chrStartEnd->chrEnd;
	    }
	/*	winExtent could still be 0 here	*/
	/*	remember this window extent	*/
	winExtent = winEnd - winStart;
	/*	set window position constraint	*/
	wds->setPositionConstraint(wds, winStart, winEnd);

	/*	Going to create a TRUE/FALSE marker array from the bedList.
	 *	Each chrom position will be marked TRUE if it belongs to
 	 *	a bed element.  Overlapping bed elements will be
 	 *	redundant and a waste of time, but no real problem.
	 *	Filter by the chr name and the win limits.
	 */
	for (bed = *bedList; bed; bed = bed->next)
	    {
	    if (sameString(wds->chrName, bed->chrom))
		{
		/*	if (no limits) OR (within limits) use it	*/
		if (!(winEnd) || ( (bed->chromStart < winEnd) &&
					(bed->chromEnd >= winStart)) )
		    {
		    struct bed *newBed = cloneBed(bed);
		    slAddHead(&filteredBed, newBed);
		    }
		}
	    }
	if (NULL == filteredBed)
	    {
	    verbose(VERBOSE_CHR_LEVEL,
		"#\tfilter found nothing in bed file: %s:%lu-%lu\n",
		wds->chrName, winStart, winEnd);
	    continue;	/*	next chrom */
	    }
	else
	    slSort(&filteredBed,bedCmp);	/*	in proper order */

	if (verboseLevel() >= VERBOSE_CHR_LEVEL)
	    {
	    int count=slCount(filteredBed);
	    verbose(VERBOSE_CHR_LEVEL,
		"#\tfiltered by chr,pos constraints, bed el count: %d\n",
		    count);
	    }

	/*	determine actual limits loaded by the filter	*/
	bedStart = min(winStart,filteredBed->chromStart);
	bedEnd = filteredBed->chromEnd;
	for (bed = filteredBed; bed; bed = bed->next)
	    {
	    if (bed->chromStart < bedStart)
		bedStart = bed->chromStart;
	    if (bed->chromEnd > bedEnd)
		bedEnd = bed->chromEnd;
	    }
	bedExtent = bedEnd - bedStart;

	/*	must use the larger of the two sizes	*/
	bedArraySize = max(winExtent,bedExtent);
	bedArraySize *= sizeof(char);	/*	in bytes	*/

	/*	good enough for 2,000,000,000 bases (bedArray elem 1 byte) */
	setMaxAlloc((size_t)2100000000);	/*2^31 = 2,147,483,648 */
	if (verboseLevel() >= VERBOSE_CHR_LEVEL)
	    startTime = clock1000();
	bedArray = (char *)needLargeMem(bedArraySize);

	verbose(VERBOSE_CHR_LEVEL,
	    "#\tbed array begins at %#lx, last at %#lx, size: %#lx\n",
		(unsigned long) bedArray,
		(unsigned long) (((unsigned long) bedArray) +
			((unsigned long) bedArraySize)),
				(unsigned long) bedArraySize );

	/*	set them all to FALSE to begin with	*/
	memset((void *)bedArray, FALSE, bedArraySize);
	if (verboseLevel() >= VERBOSE_CHR_LEVEL)
	    {
	    long et = clock1000() - startTime;
	    verbose(VERBOSE_CHR_LEVEL,
    "#\tgetDataViaBed: created data array for %lu bed indicators in %ld ms\n",
		bedArraySize, et);
	    }

	/*	use the bed definition to fill this marker array */
	/*	start at the lesser of the two start positions	*/
	bedPosition = min(winStart, bedStart);
	bedStart = bedPosition;
	boolPtr = bedArray;
	bedEnd = filteredBed->chromEnd;
	for (bed = filteredBed; bed; bed = bed->next)
	    {
	    unsigned long elSize = bed->chromEnd - bed->chromStart;

	    boolPtr = bedArray + (bed->chromStart - bedStart);
	    memset((void *)boolPtr, TRUE, elSize);
	    boolPtr += elSize;
	    bedPosition = (unsigned long)((void *)boolPtr - (void *)bedArray);
	    bedMarked += elSize;	/*	number of bases marked	*/
	    bedEnd = max(bedEnd, bed->chromEnd);
	    }
	bedExtent = bedEnd - bedStart;
	verbose(VERBOSE_CHR_LEVEL,
		"#\tbed range %lu = %lu - %lu, allocated %lu bytes\n",
		bedExtent, bedEnd, bedStart, bedArraySize);
	verbose(VERBOSE_CHR_LEVEL, "#\tbed marked %lu bases, winSize %lu\n",
		bedMarked, winExtent);
	/*	worst case ascii limit: bedMarked */
	asciiDataSizeLimit = bedMarked;

	/*set window position constraint, this eliminates the 0,0 possibility*/
	if (winEnd)
	    {
	    winStart = max(bedStart, winStart);
	    winEnd = min(bedEnd, winEnd);
	    }
	else
	    {
	    winStart = bedStart;
	    winEnd = bedEnd;
	    }

	/*	must have actual positions for constraints for getData
	 *	to do this wigFetchDataArray function.
	 */
	wds->setPositionConstraint(wds, winStart, winEnd);
	/*	now fetch all the wiggle data for this position,
	 *	constraints have been set at the top of this loop
	 */
	/*	POTENTIAL SPEEDUP here, if only bed results are
	 *	the requested result, then we should just ask for a bed
	 *	list from getData and simply intersect that bed list
	 *	with the given bed list filter.
	 */

	prevValuesMatched = wds->valuesMatched;
	prevMaxOutput = wds->maxOutput;
	wds->maxOutput = 0;
	valuesFound = wds->getData(wds, db, table, wigFetchDataArray);
	wds->valuesMatched = prevValuesMatched;
	wds->maxOutput = prevMaxOutput;

	verbose(VERBOSE_CHR_LEVEL,
	    "#\tback from getData, found %llu valid points\n", valuesFound);
	doAscii = operations & wigFetchAscii;
	doBed = operations & wigFetchBed;
	doStats = operations & wigFetchStats;
	doNoOp = operations & wigFetchNoOp;

	/*	for timing purposes, allow the wigFetchNoOp to go through */
	if (doNoOp)
	    doDataArray = doBed = doStats = doAscii = FALSE;

	/*	if ((something found) and (something to do))	*/
	if ((valuesFound > 0) && (doAscii || doBed || doStats || doNoOp))
	    {
	    struct wigAsciiData *wigAscii = NULL;
	    struct asciiDatum *asciiOut = NULL;
		/* to address data[] in wigAsciiData */

	    dataArraySize = wds->array->winEnd - wds->array->winStart;
	    /*	no bed constraint, then worst case ascii limit: dataArraySize */
	    if (0 == asciiDataSizeLimit)
		asciiDataSizeLimit = dataArraySize;

	    if (verboseLevel() > VERBOSE_PER_VALUE_LEVEL)
		dataArrayOut(wds, "stdout", FALSE, TRUE);

	    /*	OK, ready to scan the bedArray in concert with the data
	     *	array and pick out those values that are masked by the bed.
	     *	The starting position will be the greater of the
	     *	two different possible windows.
	     */
	    if (doAscii)
		{
		AllocVar(wigAscii);
		wigAscii->chrom = cloneString(wds->chrName);
		wigAscii->span = 1;	/* span information has been lost */
		wigAscii->count = 0;	/* will count up as values added */
		wigAscii->dataRange = 0.0;	/* to be determined */
		setMaxAlloc((size_t)( 2100000000 *
                 (((sizeof(size_t)/4)*(sizeof(size_t)/4)*(sizeof(size_t)/4)))));
                /* produces: size_t is 4 == 2100000000 ~= 2^31 = 2Gb
                 *      size_t is 8 = 16800000000 ~= 2^34 = 16 Gb
                 */
		verbose(VERBOSE_CHR_LEVEL,
		    "#\tworst case ascii array needLargeMem (%llu * %llu = %llu)\n",
			(unsigned long long)sizeof(struct asciiDatum), 
                        (unsigned long long)asciiDataSizeLimit,
                        (unsigned long long)(sizeof(struct asciiDatum) * asciiDataSizeLimit));
		wigAscii->data = (struct asciiDatum *) needLargeMem((size_t)
		    (sizeof(struct asciiDatum) * asciiDataSizeLimit));
			/*	maximum area needed, may use less 	*/
		asciiOut = wigAscii->data;	/* ptr to data area	*/
		slAddHead(&wds->ascii,wigAscii);
		}

	    chromPosition = max(bedStart, winStart);
	    boolPtr = bedArray + (chromPosition - bedStart);

	    fptr = wds->array->data + (chromPosition - winStart);

	    for ( ; (!maxReached) && (chromPosition < winEnd);
				++chromPosition, ++boolPtr, ++fptr)
		{
		if (wds->maxOutput)
		    {
		    if (doAscii)
			{
		/*	previous results to this fetch are wds->valuesMatched
		 *	this result is currently at valuesMatched
		 */
			if (wds->maxOutput <=
				(wds->valuesMatched + valuesMatched))
			    {
			    maxReached = TRUE;
			    break;
			    }
			}
		    if (doBed)
			{
			/*	previous results to this fetch are
			 *	wds->totalBedElements, this result is currently
			 *	at bedElCount.  The -1 is because there is
			 *	one more bed element to go upon exit from
			 *	this loop.
			 */
			if ( (wds->totalBedElements + bedElCount) >=
				    (wds->maxOutput - 1) )
			    {
			    maxReached = TRUE;
			    break;
			    }
			}
		    }
		if (*boolPtr && (!isnan(*fptr)))
		    {
		    float value = *fptr;

		    ++valuesMatched;
		    /*	construct output listings here	*/
		    if (doAscii)
			{
			/*	record limits when not also doing that
			 *	for stats	*/ 
			if (!doStats)
			    {
			    if (value < lowerLimit)
				lowerLimit = value;
			    if (value > upperLimit)
				upperLimit = value;
			    }
			asciiOut->value = value;
			asciiOut->chromStart = chromPosition;
			if (wigAscii->count < asciiDataSizeLimit)
			    {
			    ++asciiOut;
			    ++wigAscii->count;
			    }
			else
			    maxReached = TRUE;
			}
		    if (doBed)
			{
			/*	have we started a bed element yet ?	*/
			if (0 == bedElEnd)
			    bedElEnd = bedElStart = chromPosition;
			/*	That will get it started right here	*/

			/*	as long as they are continuous, keep
			 *	extending the end
			 */
			if (chromPosition == bedElEnd)
			    bedElEnd += 1; /* span information has been lost */
			else
			    {	/*	if one was collected, output it */
			    if (bedElEnd > bedElStart)
				{
				struct bed *bedEl;
				bedEl = bedElement(wds->chrName,
					bedElStart, bedElEnd, ++bedElCount);
				slAddHead(&wds->bed, bedEl);
				}
			    /*	start a new element here	*/
			    bedElStart = chromPosition;
			    bedElEnd = bedElStart + 1; /* span = 1	*/
			    }
			}	/*	if (doBed)	*/
		    if (doStats)
			{
			/*	record maximum data limits	*/
			if (value < lowerLimit)
			    lowerLimit = value;
			if (value > upperLimit)
			    upperLimit = value;
			sumData += value;
			sumSquares += value * value;
			/*	record maximum extents	*/
			if ((chromStart < 0)||(chromStart > chromPosition))
			    chromStart = chromPosition;
			if (chromEnd < (chromPosition + 1))	/* span = 1 */
			    chromEnd = chromPosition + 1;	/* span = 1 */
			++statsCount;
			}
		    }	/*	if (*boolPtr && (!isnan(*fptr)))	*/

		}
	    /*	if we can save more than 100,000 data locations, move
	     *	the results to a smaller area.  Could be huge savings on
	     *	very sparse result sets.  This comes about because at
	     *	the beginning we blindly allocated a full
	     *	winEnd-winStart data array and we probably did not use
	     *	all of that.
	     */
	    if (doAscii && ((asciiDataSizeLimit - wigAscii->count) > 100000))
		{
		struct asciiDatum *smallerDataArea;
		size_t newSize;
		newSize = sizeof(struct asciiDatum) * wigAscii->count;

		if (newSize > 0)
		    {
		    setMaxAlloc((size_t)( 2100000000 *
                 (((sizeof(size_t)/4)*(sizeof(size_t)/4)*(sizeof(size_t)/4)))));
		    /* produces: size_t is 4 == 2100000000 ~= 2^31 = 2Gb
		     *      size_t is 8 = 16800000000 ~= 2^34 = 16 Gb
		     */
		    verbose(VERBOSE_CHR_LEVEL,
	    "#\tmoving to smaller ascii array needLargeMem( %llu * %llu = %llu)\n",
			(unsigned long long)sizeof(struct asciiDatum),
                        (unsigned long long)wigAscii->count,
			(unsigned long long)(sizeof(struct asciiDatum) * wigAscii->count));
		    smallerDataArea=(struct asciiDatum *)needLargeMem(newSize);
		    memcpy(smallerDataArea, wigAscii->data, newSize);
		    freeMem(wigAscii->data);
		    wigAscii->data = smallerDataArea;
		    }
		else
		    {
		    freeMem(wigAscii->data);
		    wigAscii->data = NULL;
		    }
		}
	    /*	record dataRange for resolution accounting	*/
	    if (doAscii && wigAscii->count)
		wigAscii->dataRange = (upperLimit - lowerLimit);
	    if (doBed)	/*	there may be one last element	*/
		{
		if (bedElEnd > bedElStart)
		    {
		    struct bed *bedEl;
		    bedEl = bedElement(wds->chrName,
			    bedElStart, bedElEnd, ++bedElCount);
		    slAddHead(&wds->bed, bedEl);
		    }
		}
	    /*	there are accumulated stats to complete	*/
	    if (doStats)
		{
		accumStats(wds, lowerLimit, upperLimit, sumData, sumSquares,
		    statsCount, chromStart, chromEnd);
		if (wds->stats)
		    wds->stats->span = 1;  /* viaBed reduces span to 1 */
		resetStats(&lowerLimit, &upperLimit, &sumData, &sumSquares,
		    &statsCount, &chromStart, &chromEnd);
		}


	    verbose(VERBOSE_CHR_LEVEL,
		    "#\tviaBed found: %llu valid data points\n", valuesMatched);
	    }	/*	if ((something found) and (something to do))	*/

	/*	did they want the data array returned ?  No, then release it */
	if (!doDataArray)
	    wds->freeArray(wds);
	/*	we are certainly done with the bedArray	*/
	bedFreeList(&filteredBed);
	freeMem(bedArray);
	wds->totalBedElements += bedElCount;
	}	/*	for (chr = chromList; chr; chr = chr->next)	*/

    /*	restore these constraints we used here
     */
    if (chromConstraint)
	{
	wds->setChromConstraint(wds, saveChrName);
	freeMem(saveChrName);
	}
    else
	{
	freeMem(wds->chrName);
	wds->chrName = NULL;
	}
    if (positionConstraints)
	wds->setPositionConstraint(wds, saveWinStart, saveWinEnd);
    else
	wds->setPositionConstraint(wds, 0, 0);

    /*	The bedConstrained flag will provide a printout indication of
     *	what happened to the *Out() routines.
     */
    wds->bedConstrained = TRUE;	/* to signal the *Out() displays */

    freeHashAndVals(&chromSizes);
    }	/*	if (bedList && *bedList)	*/

wds->valuesMatched += valuesMatched;
return(valuesMatched);

}	/*	static unsigned long long getDataViaBed()	*/

static void sortResults(struct wiggleDataStream *wds)
/*	sort any results that exist	*/
{
if (wds->bed)
    slSort(&wds->bed, bedCmp);
if (wds->ascii)
    slSort(&wds->ascii, asciiDataCmp);
if (wds->stats)
    slSort(&wds->stats, statsDataCmp);
if (wds->array)
    slSort(&wds->array, arrayDataCmp);
}

static int bedOut(struct wiggleDataStream *wds, char *fileName, boolean sort)
/*	print to fileName the bed list */
{
int linesOut = 0;

FILE * fh;
fh = mustOpen(fileName, "w");
if (wds->bed)
    {
    struct bed *bedEl, *next;

    outputIdentification(wds, fh);
    showConstraints(wds, fh);

    if (sort)
	slSort(&wds->bed, bedCmp);

    for (bedEl = wds->bed; bedEl; bedEl = next )
	{
	fprintf(fh,"%s\t%u\t%u\t%s\n", bedEl->chrom, bedEl->chromStart,
	    bedEl->chromEnd, bedEl->name);
	next = bedEl->next;
	++linesOut;
	}
    }
else
    {
    showConstraints(wds, fh);
    fprintf(fh, "#\tbed: no data points found for bed format output (maxOutput: %llu)\n", wds->maxOutput);
    }
carefulClose(&fh);
return (linesOut);
}	/*	static void bedOut()	*/

static void statsOut(struct wiggleDataStream *wds, char *db, char *fileName,
    boolean sort, boolean htmlOut, boolean withHeader,
	boolean leaveTableOpen)
/*	print to fileName the statistics */
{
FILE * fh;
fh = mustOpen(fileName, "w");
if (wds->stats)
    {
    struct wiggleStats *stats, *next;
    int itemsDisplayed = 0;

    if (sort)
	slSort(&wds->stats, statsDataCmp);

    if (withHeader)
	wigStatsHeader(wds, fh, htmlOut);

    for (stats = wds->stats; stats; stats = next )
	{
	if (htmlOut)
	    {
	    long long chromSize = 0;

	    if (wds->winEnd)
		chromSize = wds->winEnd - wds->winStart;
	    else
		{
		if (! wds->isFile)
		    chromSize = hChromSize(db, stats->chrom);
		}


	    fprintf(fh,"<TR><TH ALIGN=LEFT> %s </TH>\n", stats->chrom);
	    fprintf(fh,"\t<TD ALIGN=RIGHT> %u </TD>\n",
			BASE_1(stats->chromStart)); /* display closed coords */
	    fprintf(fh,"\t<TD ALIGN=RIGHT> %u </TD>\n", stats->chromEnd);
	    fprintf(fh,"\t<TD ALIGN=RIGHT> ");
	    printLongWithCommas(fh, (long long)stats->count);
	    fprintf(fh,"</TD>\n");
	    fprintf(fh,"\t<TD ALIGN=RIGHT> %d </TD>\n", stats->span);
	    fprintf(fh,"\t<TD ALIGN=RIGHT> ");
	    printLongWithCommas(fh, (long long) stats->count*stats->span);
	    if (chromSize > 0)
		fprintf(fh,"&nbsp;(%.2f%%) </TD>\n",
		    100.0*(double)(stats->count*stats->span)/(double)chromSize);
	    else
		fprintf(fh,"</TD>\n");
	    fprintf(fh,"\t<TD ALIGN=RIGHT> %g </TD>\n", stats->lowerLimit);
	    fprintf(fh,"\t<TD ALIGN=RIGHT> %g </TD>\n", stats->lowerLimit+stats->dataRange);
	    fprintf(fh,"\t<TD ALIGN=RIGHT> %g </TD>\n", stats->dataRange);
	    fprintf(fh,"\t<TD ALIGN=RIGHT> %g </TD>\n", stats->mean);
	    fprintf(fh,"\t<TD ALIGN=RIGHT> %g </TD>\n", stats->variance);
	    fprintf(fh,"\t<TD ALIGN=RIGHT> %g </TD>\n", stats->stddev);
	    fprintf(fh,"</TR>\n");
	    }
	else
	    {
	    fprintf(fh,"%s", stats->chrom);
	    fprintf(fh,"\t%u", BASE_1(stats->chromStart));
					/* display closed coords */
	    fprintf(fh,"\t%u", stats->chromEnd);
	    fprintf(fh,"\t%u", stats->count);
	    fprintf(fh,"\t%d", stats->span);
	    fprintf(fh,"\t%u", stats->count*stats->span);
	    fprintf(fh,"\t%g", stats->lowerLimit);
	    fprintf(fh,"\t%g", stats->lowerLimit+stats->dataRange);
	    fprintf(fh,"\t%g", stats->dataRange);
	    fprintf(fh,"\t%g", stats->mean);
	    fprintf(fh,"\t%g", stats->variance);
	    fprintf(fh,"\t%g", stats->stddev);
	    fprintf(fh,"\n");
	    }
	++itemsDisplayed;
	next = stats->next;
	}
    if (!itemsDisplayed)
	{
	if (htmlOut)
	    fprintf(fh,"<TR><TH ALIGN=CENTER COLSPAN=12> No data found matching this request </TH></TR>\n");
	else
	    fprintf(fh,"#\tNo data found matching this request\n");
	}

    if (!leaveTableOpen && withHeader && htmlOut)
	fprintf(fh,"</TABLE></TD></TR></TABLE></P>\n");
    }
else
    {
    if (!htmlOut)
	{
	showConstraints(wds, fh);
	fprintf(fh, "#\tstats: no data points found\n");
	}
    }
carefulClose(&fh);
}	/*	static void statsOut()	*/

static int asciiOut(struct wiggleDataStream *wds, char *db, char *fileName, boolean sort,
	boolean rawDataOut)
/*	print to fileName the ascii data values	*/
{
boolean firstLine = TRUE;
int linesOut = 0;

FILE * fh;
fh = mustOpen(fileName, "w");
if (wds->ascii)
    {
    struct wigAsciiData *asciiData;
    char *chrom = NULL;
    unsigned span = 0;
    double worstCaseResolution = 0.0;

    if (sort)
	slSort(&wds->ascii, asciiDataCmp);

    /*	determine an over-all resolution measurement */
    for (asciiData = wds->ascii; asciiData; asciiData = asciiData->next)
	{
	double resolution = asciiData->dataRange / (MAX_WIG_VALUE+1);
	if (resolution > worstCaseResolution)
	    worstCaseResolution = resolution;
	}

    for (asciiData = wds->ascii; asciiData; asciiData = asciiData->next)
	{
	unsigned i;
	struct asciiDatum *data;

	if (asciiData->count)
	    {
	    /*	will be true the first time for both reasons	*/
	    if ( (span != asciiData->span) ||
		(chrom && differentString(asciiData->chrom, chrom)) )
		{
		freeMem(chrom);
		chrom = cloneString(asciiData->chrom);
		span = asciiData->span;
		if (!rawDataOut)
		    {
		    if (firstLine)
			{
			char *dateStamp = dateTimeStamp();
			fprintf (fh, "#\toutput date: %s UTC\n", dateStamp);
			firstLine = FALSE;
			freeMem(dateStamp);
			}
		    showConstraints(wds, fh);
		    if (wds->tblName && startsWith("gc5", wds->tblName))
			showResolutionNoDownloads(worstCaseResolution, fh);
		    else
			showResolution(worstCaseResolution, fh);
		    fprintf (fh, "variableStep chrom=%s span=%u\n",
			chrom, span);
		    }
		}
	    data = asciiData->data;
	    for (i = 0; i < asciiData->count; ++i)
		{
		/* user visible coordinates are wds->offset relative/based */
		if (rawDataOut)
		    fprintf (fh, "%g\n", data->value);
		else
		    fprintf (fh, "%u\t%g\n", data->chromStart + wds->offset, data->value);
		++data;
		++linesOut;
		}
	    }
	}
    }
else
    {
    if (!rawDataOut)
	{
	showConstraints(wds, fh);
	fprintf(fh, "#\tasciiOut: no data points found\n");
	}
    }
carefulClose(&fh);

return (linesOut);
}	/*	static void asciiOut()	*/

void wiggleDataStreamFree(struct wiggleDataStream **wds)
/*	free all structures and zero the callers' structure pointer	*/
{
if (wds)
    {
    struct wiggleDataStream *wdstream;
    wdstream=*wds;
    if (wdstream)
	{
	closeWigConn(wdstream);
	wdstream->freeAscii(wdstream);
	wdstream->freeBed(wdstream);
	wdstream->freeStats(wdstream);
	wdstream->freeArray(wdstream);
	wdstream->freeConstraints(wdstream);
	freeMem(wdstream->currentChrom);
	}
    freez(wds);
    }
}

struct wiggleDataStream *wiggleDataStreamNew()
{
struct wiggleDataStream *wdstream;

AllocVar(wdstream);
/*	everything is zero which is good since that is NULL for all the
 *	strings and lists.  A few items should have some initial values
 *	which are not necessarily NULL
 */
wdstream->isFile = FALSE;
wdstream->useDataConstraint = FALSE;
wdstream->wibFH = -1;
wdstream->limit_0 = -1 * INFINITY;
wdstream->limit_1 = INFINITY;
wdstream->wigCmpSwitch = wigNoOp_e;
wdstream->freeConstraints = freeConstraints;
wdstream->freeAscii = freeAscii;
wdstream->freeBed = freeBed;
wdstream->freeStats = freeStats;
wdstream->freeArray = freeArray;
wdstream->setMaxOutput = setMaxOutput;
wdstream->setPositionConstraint = setPositionConstraint;
wdstream->setChromConstraint = setChromConstraint;
wdstream->setSpanConstraint = setSpanConstraint;
wdstream->setDataConstraint = setDataConstraint;
wdstream->bedOut = bedOut;
wdstream->statsOut = statsOut;
wdstream->asciiOut = asciiOut;
wdstream->sortResults = sortResults;
wdstream->asciiToDataArray = asciiToDataArray;
wdstream->getDataViaBed = getDataViaBed;
wdstream->getData = getData;
wdstream->maxOutput = 0;
wdstream->offset = 1;	/*	default 1-relative/based output	*/
return wdstream;
}

