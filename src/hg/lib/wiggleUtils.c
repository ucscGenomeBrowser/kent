/*	wiggleUtils - operations to fetch data from .wib files
 *	first used in hgText
 */
#include "common.h"
#include "jksql.h"
#include "dystring.h"
#include "hdb.h"
#include "wiggle.h"

static char const rcsid[] = "$Id: wiggleUtils.c,v 1.12 2004/04/02 23:42:07 hiram Exp $";

static char *currentFile = (char *) NULL;	/* the binary file name */
static FILE *wibFH = (FILE *) NULL;		/* file handle to binary file */

static void openWibFile(char *fileName)
/*  smart open of the .wib file - only if it isn't already open */
{
if (currentFile)
    {
    if (differentString(currentFile,fileName))
	{
	if (wibFH != (FILE *) NULL)
	    {
	    fclose(wibFH);
	    wibFH = (FILE *) NULL;
	    freeMem(currentFile);
	    currentFile = (char *) NULL;
	    }
	currentFile = cloneString(fileName);
	if (differentString(currentFile,fileName))
	    errAbort("openWibFile: currentFile != fileName %s != %s",
		currentFile, fileName );
	wibFH = mustOpen(currentFile, "r");
	}
    }
else
    {
    currentFile = cloneString(fileName);
    if (differentString(currentFile,fileName))
	errAbort("openWibFile: currentFile != fileName %s != %s",
	    currentFile, fileName );
    wibFH = mustOpen(currentFile, "r");
    }
}

static void closeWibFile()
/*	smart close of .wib file - close only if open	*/
{
if (wibFH != (FILE *) NULL)
    {
    fclose(wibFH);
    wibFH = (FILE *) NULL;
    freeMem(currentFile);
    currentFile = (char *) NULL;
    }
}

static struct wiggleData * wigReadDataRow(struct wiggle *wiggle,
    int chrStart, int winEnd, int tableId, boolean summaryOnly,
	boolean (*wiggleCompare)(int tableId, double value,
	    boolean summaryOnly, struct wiggle *wiggle))
/*  read one row of wiggle data, return data values between chrStart, winEnd */
{
unsigned char *readData = (unsigned char *) NULL;
size_t itemsRead = 0;
int dataOffset = 0;
int noData = 0;
struct wiggleData *ret = (struct wiggleData *) NULL;
struct wiggleDatum *data = (struct wiggleDatum *)NULL;
struct wiggleDatum *dataPtr = (struct wiggleDatum *)NULL;
unsigned chromPosition = 0;
unsigned validCount = 0;
long int chromStart = -1;
unsigned chromEnd = 0;
double lowerLimit = 1.0e+300;
double upperLimit = -1.0e+300;
double sumData = 0.0;
double sumSquares = 0.0;

if (summaryOnly)
    {
    boolean takeIt = TRUE;
    /*	the 0.0 argument is unused in this case of
     *	summaryOnly in wiggleCompare
     */
    if (wiggleCompare)
	takeIt = (*wiggleCompare)(tableId, 0.0, summaryOnly, wiggle);
    if (takeIt)
	{ 
	upperLimit = wiggle->lowerLimit + wiggle->dataRange;
	lowerLimit = wiggle->lowerLimit;
	chromStart = wiggle->chromStart;
	chromEnd = wiggle->chromEnd;
	validCount = wiggle->validCount;
	sumData = wiggle->sumData;
	sumSquares = wiggle->sumSquares;
	} 
    }
else
    {
    openWibFile(wiggle->file);
    fseek(wibFH, wiggle->offset, SEEK_SET);
    readData = (unsigned char *) needMem((size_t) (wiggle->count + 1));
    itemsRead = fread(readData, (size_t) wiggle->count,
	    (size_t) sizeof(unsigned char), wibFH);
    if (itemsRead != sizeof(unsigned char))
	errAbort("wigReadDataRow: can not read %u bytes from %s at offset %u",
	    wiggle->count, wiggle->file, wiggle->offset);

    /*	need at most this amount, perhaps less	*/
    /*	this data area goes with the result, must be freed by wigFreeData */
    data = (struct wiggleDatum *) needMem((size_t)
	(sizeof(struct wiggleDatum)*wiggle->validCount));

    dataPtr = data;
    chromPosition = wiggle->chromStart;

    for (dataOffset = 0; dataOffset < wiggle->count; ++dataOffset)
	{
	unsigned char datum = readData[dataOffset];
	if (datum == WIG_NO_DATA)
	    ++noData;
	else
	    {
	    if (chromPosition >= chrStart && chromPosition < winEnd)
		{
		double value =
		    BIN_TO_VALUE(datum,wiggle->lowerLimit,wiggle->dataRange);
		boolean takeIt = TRUE;
		if (wiggleCompare)
		    takeIt = (*wiggleCompare)(tableId, value, summaryOnly,
			    wiggle);
		if (takeIt)
		    { 
		    dataPtr->chromStart = chromPosition;
		    dataPtr->value = value;
		    ++validCount;
		    if (validCount > wiggle->count)
		errAbort("wigReadDataRow: validCount > wiggle->count %u > %u",
				validCount, wiggle->count);

		    if (chromStart < 0)
			chromStart = chromPosition;
		    chromEnd = chromPosition + wiggle->span;
		    if (lowerLimit > dataPtr->value)
			lowerLimit = dataPtr->value;
		    if (upperLimit < dataPtr->value)
			upperLimit = dataPtr->value;
		    sumData += dataPtr->value;
		    sumSquares += dataPtr->value * dataPtr->value;
		    ++dataPtr;
		    }
		}
	    }
	chromPosition += wiggle->span;
	}

    freeMem(readData);
    }

if (validCount)
    {
    double dataRange = upperLimit - lowerLimit;
    AllocVar(ret);
    ret->next = (struct wiggleData *) NULL;
    ret->chrom = wiggle->chrom;
    ret->chromStart = chromStart;
    ret->chromEnd = chromEnd;
    ret->span = wiggle->span;
    ret->count = validCount;
    ret->lowerLimit = lowerLimit;
    ret->dataRange = dataRange;
    ret->sumData = sumData;
    ret->sumSquares = sumSquares;
    ret->data = data;
    }

return (ret);	/* may be null if validCount < 1	*/
}	/*	static struct wiggleData * wigReadDataRow()	*/

static void wigFree(struct wiggleData **pEl)
/* Free a single dynamically allocated wiggle data item */
{
struct wiggleData *el;
if ((el = *pEl) == NULL) return;
freeMem(el->data);
freez(pEl);
}

void wigFreeData(struct wiggleData **pList)
/* free a list of dynamically allocated wiggle data items */
{
struct wiggleData *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    wigFree(&el);
    }
*pList = NULL;
}

struct wiggleData *wigFetchData(char *db, char *tableName, char *chromName,
    int winStart, int winEnd, boolean summaryOnly, boolean freeData,
	int tableId, boolean (*wiggleCompare)(int tableId, double value,
	    boolean summaryOnly, struct wiggle *wiggle),
	    char *constraints)
/*  return linked list of wiggle data between winStart, winEnd */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
int chrStart = winStart - 1;	/* from user closed to DB open coords */
char **row;
int rowOffset;
int rowCount = 0;
struct wiggle *wiggle;
struct hash *spans = NULL;      /* List of spans encountered during load */
char spanName[128];
char whereSpan[128];
char query[256];
struct hashEl *el;
int leastSpan = BIGNUM;
int mostSpan = 0;
int spanCount = 0;
struct hashCookie cookie;
struct wiggleData *wigData = (struct wiggleData *) NULL;
struct wiggleData *wdList = (struct wiggleData *) NULL;
boolean bewareConstraints = FALSE;

spans = newHash(0);	/*	a listing of all spans found here	*/

/*	Are the constraints going to interfere with our span search ? */
if (constraints)
    {
    char *c = cloneString(constraints);
    tolowers(c);
    if (stringIn("span",c))
	bewareConstraints = TRUE;
    }

if (bewareConstraints)
    snprintf(query, sizeof(query),
	"SELECT span from %s where chrom = '%s' AND %s group by span",
	tableName, chromName, constraints );
else
    snprintf(query, sizeof(query),
	"SELECT span from %s where chrom = '%s' group by span",
	tableName, chromName );

/*	make sure table exists before we try to talk to it
 *	If it does not exist, we return a null result
 */
if (! sqlTableExists(conn, tableName))
    {
    hFreeConn(&conn);
    return((struct wiggleData *)NULL);
    }

/*	Survey the spans to see what the story is here */

sr = sqlMustGetResult(conn,query);
while ((row = sqlNextRow(sr)) != NULL)
{
    unsigned span = sqlUnsigned(row[0]);

    ++rowCount;

    snprintf(spanName, sizeof(spanName), "%u", span);
    el = hashLookup(spans, spanName);
    if ( el == NULL)
	{
	if (span > mostSpan) mostSpan = span;
	if (span < leastSpan) leastSpan = span;
	++spanCount;
	hashAddInt(spans, spanName, span);
	}

    }
sqlFreeResult(&sr);

/*	Now, using that span list, go through each span, fetching data	*/
cookie = hashFirst(spans);
while ((el = hashNext(&cookie)) != NULL)
    {
    if (bewareConstraints)
	{
	snprintf(whereSpan, sizeof(whereSpan), "((span = %s) AND %s)", el->name,
	    constraints);
	}
    else
	snprintf(whereSpan, sizeof(whereSpan), "span = %s", el->name);

    sr = hOrderedRangeQuery(conn, tableName, chromName, chrStart, winEnd,
        whereSpan, &rowOffset);
    rowCount = 0;
    while ((row = sqlNextRow(sr)) != NULL)
	{
	++rowCount;
	wiggle = wiggleLoad(row + rowOffset);
	if (wiggle->count > 0)
	    {
	    wigData = wigReadDataRow(wiggle, chrStart, winEnd,
		    tableId, summaryOnly, wiggleCompare );
	    if (wigData)
		{
		if (freeData)
		    {
		    freeMem(wigData->data); /* and mark it gone */
		    wigData->data = (struct wiggleDatum *)NULL;
		    }
		slAddHead(&wdList,wigData);
		}
	    }
	}
    sqlFreeResult(&sr);
    }
closeWibFile();

hFreeConn(&conn);

if (wdList != (struct wiggleData *)NULL)
	slReverse(&wdList);

return(wdList);
}	/*	struct wiggleData *wigFetchData()	*/
