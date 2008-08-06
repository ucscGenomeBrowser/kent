/* Handle details pages for expression ratio tracks. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "hgc.h"
#include "hui.h"
#include "expRecord.h"
#include "obscure.h"
#include "cheapcgi.h"
#include "genePred.h"
#include "affyAllExonProbe.h"
#include "microarray.h"

static char const rcsid[] = "$Id: expClick.c,v 1.19.18.3 2008/08/06 17:39:34 markd Exp $";

static struct rgbColor getColorForExprBed(float val, float max, boolean redGreen)
/* Return the correct color for a given score */
{
float absVal = fabs(val);
struct rgbColor color; 
int colorIndex = 0;

/* if log score is -10000 data is missing */
if(val == -10000) 
    {
    color.g = color.r = color.b = 128;
    return(color);
    }

if(absVal > max) 
    absVal = max;
if (max == 0) 
    errAbort("ERROR: hgc::getColorForExprBed() maxDeviation can't be zero\n"); 
colorIndex = (int)(absVal * 255/max);


if(redGreen) 
    {
   if(val > 0) 
	{
	color.r = colorIndex; 
	color.g = 0;
	color.b = 0;
	}
    else 
	{
	color.r = 0;
	color.g = colorIndex;
	color.b = 0;
	}
    }
else
    {
    if(val > 0) 
	{
	color.r = colorIndex; 
	color.g = colorIndex;
	color.b = 0;
	}
    else 
	{
	color.r = 0;
	color.g = 0;
	color.b = colorIndex;
	}	
    }

return color;
}

static void msBedPrintTableHeader(struct bed *bedList, 
			   struct hash *erHash, char *itemName, 
			   char **headerNames, int headerCount, char *scoresHeader)
/* print out a bed with multiple scores header for a table.
   headerNames contain titles of columns up to the scores columns. scoresHeader
   is a single string that will span as many columns as there are beds.*/
{
struct bed *bed;
int featureCount = slCount(bedList);
int i=0;
printf("<tr>");
for(i=0;i<headerCount; i++)
    printf("<th align=center>%s</th>\n",headerNames[i]);
printf("<th align=center colspan=%d valign=top>%s</th>\n",featureCount, scoresHeader);
printf("</tr>\n<tr>");
for(i=0;i<headerCount; i++)
    printf("<td>&nbsp</td>\n");
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    printf("<td valign=top align=center>\n");
    printTableHeaderName(bed->name, itemName, NULL);
    printf("</td>");
    }
printf("</tr>\n");
}

static void msBedDefaultPrintHeader(struct bed *bedList, struct hash *erHash, 
			     char *itemName)
/* print out a header with names for each bed with itemName highlighted */
{
char *headerNames[] = {"Experiment"};
char *scoresHeader = "Item Name";
msBedPrintTableHeader(bedList, erHash, itemName, headerNames, ArraySize(headerNames), scoresHeader);
}

static void printExprssnColorKey(float minVal, float maxVal, float stepSize, int base,
			struct rgbColor(*getColor)(float val, float maxVal, boolean redGreen), boolean redGreen)
/* print out a little table which provides a color->score key */
{
float currentVal = -1 * maxVal;
int numColumns;
assert(stepSize != 0);

numColumns = maxVal/stepSize *2+1;
printf("<TABLE  BGCOLOR=\"#000000\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>");
printf("<TABLE  BGCOLOR=\"#fffee8\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR>");
printf("<th colspan=%d>False Color Key, all values log base %d</th></tr><tr>\n",numColumns, base);
/* have to add the stepSize/2 to account for the ability to 
   absolutely represent some numbers as floating points */
for(currentVal = minVal; currentVal <= maxVal + (stepSize/2); currentVal += stepSize)
    {
    printf("<th><b>%.1f&nbsp;&nbsp;</b></th>", currentVal);
    }
printf("</tr><tr>\n");
for(currentVal = minVal; currentVal <= maxVal + (stepSize/2); currentVal += stepSize)
    {
    struct rgbColor rgb = getColor(currentVal, maxVal, redGreen);
    printf("<td bgcolor=\"#%.2X%.2X%.2X\">&nbsp</td>\n", rgb.r, rgb.g, rgb.b);
    }
printf("</tr></table>\n");
printf("</td></tr></table>\n");
}

static void msBedExpressionPrintRow(struct bed *bedList, struct hash *erHash, 
			     int expIndex, char *expName, float maxScore, boolean redGreen)
/* print the name of the experiment and color the 
   background of individual cells using the score to 
   create false two color display */
{
char buff[32];
struct bed *bed = bedList;
struct expRecord *er = NULL;
int square = 10;
snprintf(buff, sizeof(buff), "%d", expIndex);
er = hashMustFindVal(erHash, buff);

printf("<tr>\n");
if(strstr(er->name, expName))
    printf("<td align=left bgcolor=\"D9E4F8\"> %s</td>\n",er->name);
else
    printf("<td align=left> %s</td>\n", er->name);

for(bed = bedList;bed != NULL; bed = bed->next)
    {
    /* use the background colors to creat patterns */
    struct rgbColor rgb = getColorForExprBed(bed->expScores[expIndex], maxScore, redGreen);
    printf("<td height=%d width=%d bgcolor=\"#%.2X%.2X%.2X\">&nbsp</td>\n", square, square, rgb.r, rgb.g, rgb.b);
    }
printf("</tr>\n");
}

static void msBedPrintTable(struct bed *bedList, struct hash *erHash, char *itemName, 
		     char *expName, float minScore, float maxScore, float stepSize, int base,
		     void(*printHeader)(struct bed *bedList, struct hash *erHash, char *item),
		     void(*printRow)(struct bed *bedList,struct hash *erHash, int expIndex, char *expName, float maxScore, boolean redGreen),
		     void(*printKey)(float minVal, float maxVal, float size, int base, struct rgbColor(*getColor)(float val, float max, boolean redGreen), boolean redGreen),
		     struct rgbColor(*getColor)(float val, float max, boolean redGreen), boolean redGreen)
/* prints out a table from the data present in the bedList */
{
int i,featureCount=0;
if(bedList == NULL)
    errAbort("hgc::msBedPrintTable() - bedList is NULL");
featureCount = slCount(bedList);
/* time to write out some html, first the table and header */
if(printKey != NULL)
    printKey(minScore, maxScore, stepSize, base, getColor, redGreen);
printf("<p>\n");
printf("<basefont size=-1>\n");
printf("<table  bgcolor=\"#000000\" border=\"0\" cellspacing=\"0\" cellpadding=\"1\"><tr><td>");
printf("<table  bgcolor=\"#fffee8\" border=\"0\" cellspacing=\"0\" cellpadding=\"1\">");
printHeader(bedList, erHash, itemName);
for(i=0; i<bedList->expCount; i++)
    {
    printRow(bedList, erHash, i, expName, maxScore, redGreen);
    }
printf("</table>");
printf("</td></tr></table>");
printf("</basefont>");
}

static struct bed *loadMsBed(struct trackDb *tdb, char *table, char *chrom, uint start, uint end)
/* load every thing from a bed 15 table in the given range */
{
struct sqlConnection *conn = hAllocConnTrack(database, tdb);
struct sqlResult *sr;
char **row;
int rowOffset;
struct bed *bedList = NULL, *bed;
sr = hRangeQuery(conn, table, chrom, start, end, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+rowOffset, 15);
    slAddHead(&bedList, bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&bedList);
return bedList;
}

static struct bed * loadMsBedAll(char *table)
/* load every thing from a bed 15 table */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
struct bed *bedList = NULL, *bed;
char query[512];
sprintf(query, "select * from %s", table); 
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row, 15);
    slAddHead(&bedList, bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&bedList);
return bedList;
}

static struct expRecord * loadExpRecord(char *table, char *database)
/* load everything from an expRecord table in the
   specified database, usually hgFixed instead of hg7, hg8, etc. */
{
struct sqlConnection *conn = sqlConnect(database);
char query[256];
struct expRecord *erList = NULL;
snprintf(query, sizeof(query), "select * from %s", table);
erList = expRecordLoadByQuery(conn, query);
sqlDisconnect(&conn);
return erList;
}

void getMsBedExpDetails(struct trackDb *tdb, char *expName, boolean all)
/* Create tab-delimited output to download */
{
char *expTable = cartString(cart, "i");
char *bedTable = cartString(cart, "o");
struct expRecord *er, *erList=NULL;
struct bed *b, *bedList=NULL;
int i;

/* Get all of the expression record details */
erList = loadExpRecord(expTable, "hgFixed");

/* Get either all of the data, or only that data in the range */
if (all) 
    bedList = loadMsBedAll(bedTable);
else 
    bedList = loadMsBed(tdb, bedTable, seqName, winStart, winEnd); 

/* Print out a header row */
printf("<HTML><BODY><PRE>\n");
printf("Name\tChr\tChrStart\tChrEnd\tTallChrStart\tTallChrEnd");
for (er = erList; er != NULL; er = er->next)
    if (sameString(bedTable, "cghNci60"))
	printf("\t%s(%s)",er->name, er->extras[1]);
    else 
	printf("\t%s",er->name);
printf("\n");

/* Print out a row for each of the record in the bedList */
for (b = bedList; b != NULL; b = b->next)
    {
    printf("%s\t%s\t%d\t%d\t%d\t%d",b->name, b->chrom, b->chromStart, b->chromEnd, b->thickStart, b->thickEnd);
    for (i = 0; i < b->expCount; i++)
	if (i == b->expIds[i])
	    printf("\t%f",b->expScores[i]);
	else
	    printf("\t");
    printf("\n");
    }
printf("</PRE>");
}

struct expRecord *basicExpRecord(char *expName, int id, int extrasIndex)
/* This returns a stripped-down new expRecord, probably the result of an average. */
{
int i;
struct expRecord *er = NULL;
AllocVar(er);
er->id = id;
er->name = cloneString(expName);
er->description = cloneString(expName);
er->url = cloneString("n/a");
er->ref = cloneString("n/a");
er->credit = cloneString("n/a");
er->numExtras = extrasIndex+1;
AllocArray(er->extras, er->numExtras);
for (i = 0; i < er->numExtras; i++)
    er->extras[i] = cloneString(expName);
return er;
}

void bedListExpRecordAverage(struct bed **pBedList, struct expRecord **pERList, int extrasIndex)
/* This is a mildy complicated function to make the details page have the */
/* same data as the track when the UI option "Tissue averages" is selected. */
/* This is done by hacking the bed and expRecord lists in place and keeping */
/* the original code for the most part. */
{
struct bed *bed = NULL;
struct expRecord *er, *newERList = NULL;
struct slName *extras = NULL, *oneSlName;
int M, N, i, columns;
int *mapping;
if (!pBedList || !pERList || !*pBedList || !*pERList)
    return;
er = *pERList;
if ((extrasIndex < 0) || (extrasIndex >= er->numExtras))
    return;
/* Build up a unique list of words from the specific "extras" column. */
for (er = *pERList; er != NULL; er = er->next)
    slNameStore(&extras, er->extras[extrasIndex]);
slReverse(&extras);
M = slCount(extras);
N = slCount(*pERList);
columns = N + 1;
/* M rows, reserve first column for counts. */
mapping = needMem(sizeof(int) * M * columns);
/* Create the mapping array: */
/*   each row corresponds to one of the groupings.  The first column is the number of */
/*   things in the original list in the group (k things), and the next k elements  on */
/*   that row are indeces. */
for (er = *pERList, i = 0; er != NULL && i < N; er = er->next, i++)
    {    
    int ix = slNameFindIx(extras, er->extras[extrasIndex]) * columns;
    mapping[ix + ++mapping[ix]] = er->id; 
    }
/* Make a new expRecord list. */
for (oneSlName = extras, i = 0; oneSlName != NULL && i < M; oneSlName = oneSlName->next, i++)
    {
    struct expRecord *newER = basicExpRecord(oneSlName->name, i, extrasIndex);
    slAddHead(&newERList, newER);
    }
slReverse(&newERList);
expRecordFreeList(pERList);
*pERList = newERList;
/* Go through each bed and change it. */
for (bed = *pBedList; bed != NULL; bed = bed->next)
    {
    float *newExpScores = needMem(sizeof(float) * M);
    int *newExpIds = needMem(sizeof(int) * M);
    /* Calculate averages. */
    for (i = 0; i < M; i++)
	{
	int ix = i * columns;
	int size = mapping[ix];
	int j;
	double sum = 0;
	for (j = 1; j < size + 1; j++)
	    sum += (double)bed->expScores[mapping[ix + j]];
	newExpScores[i] = (float)(sum/size);
	newExpIds[i] = i;
	}
    bed->expCount = M;
    freeMem(bed->expIds);
    bed->expIds = newExpIds;
    freeMem(bed->expScores);
    bed->expScores = newExpScores;    
    }
/* Free stuff. */
slNameFreeList(&extras);
freez(&mapping);
}

void erHashElFree(struct hashEl *el)
/* Frees up expRecord hash head. */
{
struct expRecord *er = el->val;
expRecordFree(&er);
}

void doExpRatio(struct trackDb *tdb, char *item, struct customTrack *ct)
/* Generic expression ratio deatils using microarrayGroups.ra file */
/* and not the expRecord tables. */
{
char *expScale = trackDbRequiredSetting(tdb, "expScale");
char *expStep = trackDbRequiredSetting(tdb, "expStep");
double maxScore = atof(expScale);
double stepSize = atof(expStep);
struct bed *bedList;
char *itemName = cgiUsualString("i2","none");
char *expName = (item == NULL) ? itemName : item;
boolean redGreen = TRUE;
char colorVarName[256];

safef(colorVarName, sizeof(colorVarName), "%s.color", tdb->tableName);
if (!sameString(cartUsualString(cart, colorVarName, "redGreen"), "redGreen"))
    redGreen = FALSE;
if (!ct)
    {
    genericHeader(tdb, itemName);
    bedList = loadMsBed(tdb, tdb->tableName, seqName, winStart, winEnd);
    }
else if (ct->dbTrack)
    bedList = ctLoadMultScoresBedDb(ct, seqName, winStart, winEnd);
else
    bedList = bedFilterListInRange(ct->bedList, NULL, seqName, winStart, winEnd);
if (bedList == NULL)
    printf("<b>No Expression Data in this Range.</b>\n");
else if (expName && sameString(expName, "zoomInMore"))
    printf("<b>Too much data to display in detail in this range.</b>\n");
else
    {
    struct microarrayGroups *groupings = NULL;
    struct maGrouping *combineGroup;
    struct hash *erHash = newHash(6);
    int i;
    if (!ct)
	{
	groupings = maGetTrackGroupings(database, tdb);
	combineGroup = maCombineGroupingFromCart(groupings, cart, tdb->tableName);
	}
    else
	combineGroup = maGetGroupingFromCt(ct);
    maBedClumpGivenGrouping(bedList, combineGroup);
    for (i = 0; i < combineGroup->numGroups; i++)
	{
	/* make stupid exprecord hash.perhaps eventually this won't be needed */
	char id[16];
	struct expRecord *er = basicExpRecord(combineGroup->names[i], i, 2);
	safef(id, sizeof(id), "%d", i);
	hashAdd(erHash, id, er);
	}
    puts("<h2></h2><p>\n");
    msBedPrintTable(bedList, erHash, itemName, expName, -1*maxScore, maxScore, stepSize, 2,
		     msBedDefaultPrintHeader, msBedExpressionPrintRow, printExprssnColorKey, getColorForExprBed, redGreen);
    hashTraverseEls(erHash, erHashElFree);
    hashFree(&erHash);
    microarrayGroupsFree(&groupings);
    }
puts("<h2></h2><p>\n");
bedFreeList(&bedList);
}

