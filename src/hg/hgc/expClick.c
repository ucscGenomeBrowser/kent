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

static char const rcsid[] = "$Id: expClick.c,v 1.12 2006/07/21 16:14:04 aamp Exp $";

static struct rgbColor getColorForExprBed(float val, float max)
/* Return the correct color for a given score */
{
char *colorScheme = cartUsualString(cart, "exprssn.color", "rg");
boolean redGreen = sameString(colorScheme, "rg");
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
	color.g = 0;
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

static struct rgbColor getColorForLoweExprBed(float val, float max)
/* Return the correct color for a given score */
{
char *colorScheme = cartUsualString(cart, "exprssn.color", "rg");
boolean redGreen = sameString(colorScheme, "rg");
float absVal = fabs(val);
struct rgbColor color; 
int colorIndex = 0;

/* if log score is -10000 data is missing */
if(val == -10000) 
    {
    color.g = color.r = color.b = 128;
    return(color);
    }
if(val>5000){ absVal=fabs(absVal-10000.0); max=1;}
if(absVal > max) 
    absVal = max;
if (max == 0) 
    errAbort("ERROR: hgc::getColorForExprBed() maxDeviation can't be zero\n"); 
colorIndex = (int)(absVal * 255/max);
if(val>5000){
	color.r=0;
	color.g=0;
	if(val == 10001){
		color.b=255;
		color.r=120;
		color.g=255;
	}
	else if(val == 10002){
		color.b=255;
		color.r=80;
		color.g=200;
	}
	else {
		color.b=255;
		color.g=60;
	}	
}
else{
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
		color.g = 0;
		color.b = 0;
		}
	    else 
		{
		color.r = 0;
		color.g = 0;
		color.b = colorIndex;
		}	
   	 }
}
return color;
}

static struct rgbColor getColorForAffyBed(float val, float max)
/* Return the correct color for a given score */
{
struct rgbColor color; 
int colorIndex = 0;
int offset = 0;

/* if log score is -10000 data is missing */
if(val == -10000) 
    {
    color.g = color.r = color.b = 128;
    return(color);
    }

val = fabs(val);
/* take the log for visualization */
if(val > 0)
    val = logBase2(val);
else
    val = 0;

/* scale offset down to 0 */
if(val > offset) 
    val -= offset;
else
    val = 0;

if (max <= 0) 
    errAbort("ERROR: hgc::getColorForAffyBed() maxDeviation can't be zero\n"); 
max = logBase2(max);
max -= offset;
if(max < 0)
    errAbort("hgc::getColorForAffyBed() - Max val should be greater than 0 but it is: %g", max);
    
if(val > max) 
    val = max;

colorIndex = (int)(val * 255/max);
color.r = 0;
color.g = 0;
color.b = colorIndex;
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

static char *abbrevExprBedName(char *name)
/* chops part off rosetta exon identifiers, returns pointer to 
   local static char* */
{
static char abbrev[32];
strncpy(abbrev, name, sizeof(abbrev));
abbr(abbrev, "LINK_Em:");
return abbrev;
}

static void rosettaPrintHeader(struct bed *bedList, 
			struct hash *erHash, char *itemName)
/* print out the header for the rosetta details table */
{
char *headerNames[] = {"&nbsp", "Hybridization"};
char *scoresHeader = "Exon Number";
msBedPrintTableHeader(bedList, erHash, itemName, headerNames, ArraySize(headerNames), scoresHeader);
}

static void cghNci60PrintHeader(struct bed *bedList, 
			 struct hash *erHash, char *itemName)
/* print out the header for the CGH NCI 60 details table */
{
char *headerNames[] = {"Cell Line", "Tissue"};
char *scoresHeader ="CGH Log Ratio";
msBedPrintTableHeader(bedList, erHash, itemName, headerNames, ArraySize(headerNames), scoresHeader);
}

static void printExprssnColorKey(float minVal, float maxVal, float stepSize, int base,
			  struct rgbColor(*getColor)(float val, float maxVal))
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
    struct rgbColor rgb = getColor(currentVal, maxVal);
    printf("<td bgcolor=\"#%.2X%.2X%.2X\">&nbsp</td>\n", rgb.r, rgb.g, rgb.b);
    }
printf("</tr></table>\n");
printf("</td></tr></table>\n");
}

static void printAffyExprssnColorKey(float minVal, float maxVal, float stepSize, int base,
			      struct rgbColor(*getColor)(float val, float maxVal))
/* print out a little table which provides a color->score key */
{
float currentVal = -1 * maxVal;
int numColumns;

assert(maxVal != 0);
if(minVal != 0)
    numColumns = logBase2(maxVal) - logBase2(minVal);
else 
    numColumns = logBase2(maxVal);
printf("<TABLE  BGCOLOR=\"#000000\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>\n");
printf("<TABLE  BGCOLOR=\"#fffee8\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\">\n<tr>");
printf("<th colspan=%d>False Color Key</th></tr>\n<tr>",numColumns);
printf("<th width=55><b> NA </b></th>");
for(currentVal = minVal; currentVal <= maxVal; currentVal = (2*currentVal))
    {
    printf("<th width=55><b> %7.2g </b></th>", currentVal);
    }
printf("</tr>\n<tr>");
printf("<td bgcolor=\"#%.2X%.2X%.2X\">&nbsp</td>\n", 128,128,128);
for(currentVal = minVal; currentVal <= maxVal; currentVal = (2*currentVal))
    {
    struct rgbColor rgb = getColor(currentVal, maxVal);
    printf("<td bgcolor=\"#%.2X%.2X%.2X\">&nbsp</td>\n", rgb.r, rgb.g, rgb.b);
    }
printf("</tr></table>\n");
printf("</td></tr></table>\n");
}

static void msBedExpressionPrintRow(struct bed *bedList, struct hash *erHash, 
			     int expIndex, char *expName, float maxScore)
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
    struct rgbColor rgb = getColorForExprBed(bed->expScores[expIndex], maxScore);
    printf("<td height=%d width=%d bgcolor=\"#%.2X%.2X%.2X\">&nbsp</td>\n", square, square, rgb.r, rgb.g, rgb.b);
    }
printf("</tr>\n");
}

static void msBedExpressionPrintLoweRow(struct bed *bedList, struct hash *erHash, 
			     int expIndex, char *expName, float maxScore)
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
    struct rgbColor rgb = getColorForLoweExprBed(bed->expScores[expIndex], maxScore);
    printf("<td height=%d width=%d bgcolor=\"#%.2X%.2X%.2X\">&nbsp</td>\n", square, square, rgb.r, rgb.g, rgb.b);
    }
printf("</tr>\n");
}

static void msBedAffyPrintRow(struct bed *bedList, struct hash *erHash, int expIndex, char *expName, float maxScore)
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
    struct rgbColor rgb = getColorForAffyBed(bed->expScores[expIndex], maxScore);
    printf("<td height=%d width=%d bgcolor=\"#%.2X%.2X%.2X\">&nbsp</td>\n", square, square, rgb.r, rgb.g, rgb.b);
    }
printf("</tr>\n");
}


static void msBedPrintTable(struct bed *bedList, struct hash *erHash, char *itemName, 
		     char *expName, float minScore, float maxScore, float stepSize, int base,
		     void(*printHeader)(struct bed *bedList, struct hash *erHash, char *item),
		     void(*printRow)(struct bed *bedList,struct hash *erHash, int expIndex, char *expName, float maxScore),
		     void(*printKey)(float minVal, float maxVal, float size, int base, struct rgbColor(*getColor)(float val, float max)),
		     struct rgbColor(*getColor)(float val, float max))
/* prints out a table from the data present in the bedList */
{
int i,featureCount=0;
if(bedList == NULL)
    errAbort("hgc::msBedPrintTable() - bedList is NULL");

featureCount = slCount(bedList);
/* time to write out some html, first the table and header */
if(printKey != NULL)
    printKey(minScore, maxScore, stepSize, base, getColor);
   
printf("<p>\n");
printf("<basefont size=-1>\n");
printf("<table  bgcolor=\"#000000\" border=\"0\" cellspacing=\"0\" cellpadding=\"1\"><tr><td>");
printf("<table  bgcolor=\"#fffee8\" border=\"0\" cellspacing=\"0\" cellpadding=\"1\">");
printHeader(bedList, erHash, itemName);
for(i=0; i<bedList->expCount; i++)
    {
     
    printRow(bedList, erHash, i, expName, maxScore);
    }
printf("</table>");
printf("</td></tr></table>");
printf("</basefont>");
}

static struct bed * loadMsBed(char *table, char *chrom, uint start, uint end)
/* load every thing from a bed 15 table in the given range */
{
struct sqlConnection *conn = hAllocConn();
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
struct sqlConnection *conn = hAllocConn();
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

static void msBedGetExpDetailsLink(char *expName, struct trackDb *tdb, char *expTable)
/* Create link to download data from a MsBed track */
{
char *msBedTable = tdb->tableName;

printf("<H3>Download raw data for experiments:</H3>\n");
printf("<UL>\n");
hgcAnchorSomewhere("getMsBedAll", expTable, msBedTable, seqName);
printf("<LI>All data</A>\n");
hgcAnchorSomewhere("getMsBedRange", expTable, msBedTable, seqName);
printf("<LI>Data in range</A>\n");
printf("</UL>\n");
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
    bedList = loadMsBed(bedTable, seqName, winStart, winEnd); 

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

static struct bed *rosettaFilterByExonType(struct bed *bedList)
/* remove beds from list depending on user preference for 
   seeing confirmed and/or predicted exons */
{
struct bed *bed=NULL, *tmp=NULL, *tmpList=NULL;
char *exonTypes = cartUsualString(cart, "rosetta.et", rosettaExonEnumToString(0));
enum rosettaExonOptEnum et = rosettaStringToExonEnum(exonTypes);

if(et == rosettaAllEx)
    return bedList;
/* go through and remove appropriate beds */
for(bed = bedList; bed != NULL; )
    {
    if(et == rosettaConfEx)
	{
	tmp = bed->next;
	if(bed->name[strlen(bed->name) -2] == 't')
	    slSafeAddHead(&tmpList, bed);
	else
	    bedFree(&bed);
	bed = tmp;
	}
    else if(et == rosettaPredEx)
	{
	tmp = bed->next;
	if(bed->name[strlen(bed->name) -2] == 'p')
	    slSafeAddHead(&tmpList, bed);
	else
	    bedFree(&bed);
	bed = tmp;
	}
    }
slReverse(&tmpList);
return tmpList;
}

static void rosettaPrintRow(struct bed *bedList, struct hash *erHash, int expIndex, char *expName, float maxScore)
/* print a row in the details table for rosetta track, designed for
   use msBedPrintTable */
{
char buff[32];
struct bed *bed = bedList;
struct expRecord *er = NULL;
int square = 10;
snprintf(buff, sizeof(buff), "%d", expIndex);
er = hashMustFindVal(erHash, buff);

sprintf(buff,"e%d",er->id);
printf("<tr>\n");
printf("<td align=left>");
cgiMakeCheckBox(buff,FALSE);
printf("</td>");
if(strstr(er->name, expName))
    printf("<td align=left bgcolor=\"D9E4F8\"> %s</td>\n",er->name);
else
    printf("<td align=left> %s</td>\n", er->name);
for(bed = bedList;bed != NULL; bed = bed->next)
    {
    /* use the background colors to creat patterns */
    struct rgbColor rgb = getColorForExprBed(bed->expScores[expIndex], maxScore);
    printf("<td height=%d width=%d bgcolor=\"#%.2X%.2X%.2X\">&nbsp</td>\n", square, square, rgb.r, rgb.g, rgb.b);
    }
printf("</tr>\n");
}

static void rosettaPrintDataTable(struct bed *bedList, char *itemName, char *expName, float maxScore, char *tableName)
/* creates a false color table of the data in the bedList */
{
struct expRecord *erList = NULL, *er;
struct hash *erHash;
float stepSize = 0.2;
char buff[32];
if(bedList == NULL)
    printf("<b>No Expression Data in this Range.</b>\n");
else 
    {
    erHash = newHash(2);
    erList = loadExpRecord(tableName, "hgFixed");
    for(er = erList; er != NULL; er=er->next)
	{
	snprintf(buff, sizeof(buff), "%d", er->id);
	hashAddUnique(erHash, buff, er);
	}
    msBedPrintTable(bedList, erHash, itemName, expName, -1*maxScore, maxScore, stepSize, 2,
		    rosettaPrintHeader, rosettaPrintRow, printExprssnColorKey, getColorForExprBed);
    expRecordFreeList(&erList);
    hashFree(&erHash);
    bedFreeList(&bedList);
    }
}

void rosettaDetails(struct trackDb *tdb, char *expName)
/* print out a page for the rosetta data track */
{
struct bed *bedList, *bed=NULL;
char *tableName = "rosettaExps";
char *itemName = cgiUsualString("i2","none");
char *nameTmp=NULL;
char buff[256];
char *plotType = NULL;
float maxScore = 2.0;
char *maxIntensity[] = { "100", "20", "15", "10", "5" ,"4","3","2","1" };
char *exonTypes = cartUsualString(cart, "rosetta.et", rosettaExonEnumToString(0));
enum rosettaExonOptEnum et = rosettaStringToExonEnum(exonTypes);

/* get data from database and filter it */
bedList = loadMsBed(tdb->tableName, seqName, winStart, winEnd);
bedList = rosettaFilterByExonType(bedList);


/* abbreviate the names */
for(bed=bedList; bed != NULL; bed = bed->next)
    {
    nameTmp = abbrevExprBedName(bed->name);
    freez(&bed->name);
    bed->name = cloneString(nameTmp);
    }

/* start html */
snprintf(buff, sizeof(buff), "Rosetta Expression Data For: %s %d-%d</h2>", seqName, winStart, winEnd);
cartWebStart(cart, buff);
printf("%s", tdb->html);
printf("<br><br>");
printf("<form action=\"../cgi-bin/rosChr22VisCGI\" method=get>\n");
rosettaPrintDataTable(bedList, itemName, expName, maxScore, tableName);

/* other info needed for plotting program */
cgiMakeHiddenVar("table", tdb->tableName);
cgiMakeHiddenVar("db", database);
sprintf(buff,"%d",winStart);
cgiMakeHiddenVar("winStart", buff);
zeroBytes(buff,64);
sprintf(buff,"%d",winEnd);

/* plot type is passed to graphing program to tell it which exons to use */
if(et == rosettaConfEx)
    plotType = "te";
else if(et == rosettaPredEx)
    plotType = "pe";
else if(et == rosettaAllEx) 
    plotType = "e";
else 
    errAbort("hgc::rosettaDetails() - don't recognize rosettaExonOptEnum %d", et);
cgiMakeHiddenVar("t",plotType);
cgiMakeHiddenVar("winEnd", buff);
printf("<br>\n");
printf("<table width=\"100%%\" cellpadding=0 cellspacing=0>\n");
printf("<tr><th align=left><h3>Plot Options:</h3></th></tr><tr><td><p><br>");
cgiMakeDropList("mi",maxIntensity, 9, "20");
printf(" Maximum Intensity value to allow.\n");
printf("</td></tr><tr><td align=center><br>\n");
printf("<b>Press Here to View Detailed Plots</b><br><input type=submit name=Submit value=submit>\n");
printf("<br><br><br><b>Clear Values</b><br><input type=reset name=Reset></form>\n");
printf("</td></tr></table>");
}

void nci60Details(struct trackDb *tdb, char *expName) 
/* print out a page for the nci60 data from stanford */
{
struct bed *bedList;
char *tableName = "nci60Exps";
char *itemName = cgiUsualString("i2","none");
struct expRecord *erList = NULL, *er;
char buff[32];
struct hash *erHash;
float stepSize = 0.2;
float maxScore = 2.0;
bedList = loadMsBed(tdb->tableName, seqName, winStart, winEnd);
genericHeader(tdb, itemName);

printf("%s", tdb->html);
printf("<br><br>");
if(bedList == NULL)
    printf("<b>No Expression Data in this Range.</b>\n");
else 
    {
    erHash = newHash(2);
    erList = loadExpRecord(tableName, "hgFixed");
    for(er = erList; er != NULL; er=er->next)
	{
	snprintf(buff, sizeof(buff), "%d", er->id);
	hashAddUnique(erHash, buff, er);
	}
    msBedPrintTable(bedList, erHash, itemName, expName, -1*maxScore, maxScore, stepSize, 2,
		    msBedDefaultPrintHeader, msBedExpressionPrintRow, printExprssnColorKey, getColorForExprBed);
    expRecordFreeList(&erList);
    hashFree(&erHash);
    bedFreeList(&bedList);
    }
}

static void printAffyGnfLinks(char *name, char *chip)
/* print out links to affymetrix's netaffx website */
{
char *netaffx = "https://www.affymetrix.com/LinkServlet?array=";
/*char *netaffx = "https://www.netaffx.com/LinkServlet?array=;";*/
char *netaffxDisp = "https://www.affymetrix.com/svghtml?query=";
/*char *netaffxDisp = "https://www.netaffx.com/svghtml?query=";*/
char *gnfDetailed = "http://expression.gnf.org/cgi-bin/index.cgi?text=";
/* char *gnf = "http://expression.gnf.org/promoter/tissue/images/"; */
if(name != NULL)
    {
    printf("<p>More information about individual probes and probe sets is available ");
    printf("at Affymetrix's <a href=\"https://www.netaffx.com/index2.jsp\" TARGET=\"_blank\">netaffx.com</a> website.<BR>[Registration at Affymetrix is required to activate the following links]\n"); 
    printf("<ul>\n");
    printf("<li> Information about probe sequences is <a href=\"%s%s&probeset=%s\" TARGET=\"_blank\">available there</a></li>\n",
	   netaffx, chip, name);
    printf("<li> A graphical representation is also <a href=\"%s%s\" TARGET=\"_blank\">available</a> ",netaffxDisp, name);
    printf("&nbsp;[SVG viewer required]</li>\n");
    printf("</ul>\n");
    printf("<p>A <a href=\"%s%s&chip=%sA#Q\" TARGET=\"_blank\">histogram</a> of the data for the selected probe set (%s) used against all ",gnfDetailed, name, chip, name);
    printf("tissues is available at the <a href=\"http://expression.gnf.org/cgi-bin/index.cgi\" TARGET=\"_blank\">GNF web supplement</a>.\n");
    }
}

static void printAffyUclaLinks(char *name, char *chip)
/* print out links to affymetrix's netaffx website */
{
char *netaffx = "https://www.netaffx.com/LinkServlet?array=";
char *netaffxDisp = "https://www.netaffx.com/svghtml?query=";
if(name != NULL)
    {
    printf("<p>More information about individual probes and probe sets is available ");
    printf("at Affymetrix's <a href=\"https://www.netaffx.com/index2.jsp\">netaffx.com</a> website. [registration required]\n");
    printf("<ul>\n");
    printf("<li> Information about probe sequences is <a href=\"%s%s&probeset=%s\">available there</a></li>\n",
	   netaffx, chip, name);
    printf("<li> A graphical representation is also <a href=\"%s%s\">available</a> ",netaffxDisp, name);
    printf("<basefont size=-2>[svg viewer required]</basefont></li>\n");
    printf("</ul>\n");
    }
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

void printAffyHumanExonProbeInfo(struct trackDb *tdb, char *item, char *probeTable)
/* Get information on a probe and print that out.*/
{
struct sqlConnection *conn = sqlConnect("hgFixed");
struct affyAllExonProbe *oneProbe;
char query[200];
safef(query, ArraySize(query), "select * from %s where name = \'%s\'", probeTable, item);
oneProbe = affyAllExonProbeLoadByQuery(conn, query);
if (oneProbe != NULL)
    {
    printf("<h3>Details for Probe %s</h3>\n", item);
    printf("<B>Exon evidence:</B> %s<BR>\n", oneProbe->evidence); 
    printf("<B>Exon level:</B> %s<BR>\n", oneProbe->level); 
    affyAllExonProbeFreeList(&oneProbe);
    }
sqlDisconnect(&conn);
}

void affyAllExonDetails(struct trackDb *tdb, char *expName, char *tableName, char *expTable,
	char *chip, float stepSize, float maxScore) 
/* print out a page for the affy data from gnf based on ratio of
 * measurements to the median of the measurements. */
{
struct bed *bedList;
char *itemName = cgiUsualString("i2","none");
struct expRecord *erList = NULL, *er;
char buff[32];
struct hash *erHash;
char *expProbes = trackDbRequiredSetting(tdb, "expProbeTable");
genericHeader(tdb, itemName);
bedList = loadMsBed(tableName, seqName, winStart, winEnd);
if(bedList == NULL)
    printf("<b>No Expression Data in this Range.</b>\n");
else 
    {
    char varName[128];
    char *affyAllExonMap;
    enum affyAllExonOptEnum affyAllExonType;
    erHash = newHash(2);
    erList = loadExpRecord(expTable, "hgFixed");
    safef(varName, sizeof(varName), "%s.%s", tdb->tableName, "type");
    affyAllExonMap = cartUsualString(cart, varName, affyAllExonEnumToString(affyAllExonTissue));
    affyAllExonType = affyAllExonStringToEnum(affyAllExonMap);
    if (affyAllExonType == affyAllExonTissue)
	bedListExpRecordAverage(&bedList, &erList, 2);
    for(er = erList; er != NULL; er=er->next)
	{
	snprintf(buff, sizeof(buff), "%d", er->id);
	hashAddUnique(erHash, buff, er);
	}
    printf("<h2></h2><p>\n");
    msBedPrintTable(bedList, erHash, itemName, expName, -1*maxScore, maxScore, stepSize, 2,
		    msBedDefaultPrintHeader, msBedExpressionPrintRow, printExprssnColorKey, getColorForExprBed);
    expRecordFreeList(&erList);
    hashFree(&erHash);
    bedFreeList(&bedList);
    }
printf("<h2></h2><p>\n");
printAffyHumanExonProbeInfo(tdb, expName, expProbes);
printf("%s", tdb->html);
}

void affyDetails(struct trackDb *tdb, char *expName) 
/* print out a page for the affy data from gnf */
{
struct bed *bedList;
char *tableName = "affyExps";
char *itemName = cgiUsualString("i2","none");
int stepSize = 1;
struct expRecord *erList = NULL, *er;
char buff[32];
struct hash *erHash;
float maxScore = 262144/16; /* 2^18/2^4 */
float minScore = 2;
bedList = loadMsBed(tdb->tableName, seqName, winStart, winEnd);
genericHeader(tdb, itemName);
printf("<h2></h2><p>\n");
printf("%s", tdb->html);

printAffyGnfLinks(itemName, "U95");
if(bedList == NULL)
    printf("<b>No Expression Data in this Range.</b>\n");
else 
    {
    erHash = newHash(2);
    erList = loadExpRecord(tableName, "hgFixed");
    for(er = erList; er != NULL; er=er->next)
	{
	snprintf(buff, sizeof(buff), "%d", er->id);
	hashAddUnique(erHash, buff, er);
	}
    printf("<h2></h2><p>\n");
    msBedPrintTable(bedList, erHash, itemName, expName, minScore, maxScore, stepSize, 2,
		    msBedDefaultPrintHeader, msBedAffyPrintRow, printAffyExprssnColorKey, getColorForAffyBed);
    expRecordFreeList(&erList);
    hashFree(&erHash);
    bedFreeList(&bedList);
    }
}

static void genericRatioDetails(struct trackDb *tdb, char *expName, char *expTable,
	char *chip, float stepSize, float maxScore,
	void (*printLinks)(char *item, char *chip), boolean printHtml) 
/* print out a page for the affy data from gnf based on ratio of
 * measurements to the median of the measurements. */
{
struct bed *bedList;
char *itemName = cgiUsualString("i2","none");
struct expRecord *erList = NULL, *er;
char buff[32];
struct hash *erHash;

genericHeader(tdb, itemName);
bedList = loadMsBed(tdb->tableName, seqName, winStart, winEnd);
if(bedList == NULL)
    printf("<b>No Expression Data in this Range.</b>\n");
else 
    {
    erHash = newHash(2);
    erList = loadExpRecord(expTable, "hgFixed");
    for(er = erList; er != NULL; er=er->next)
	{
	snprintf(buff, sizeof(buff), "%d", er->id);
	hashAddUnique(erHash, buff, er);
	}
    printf("<h2></h2><p>\n");

    msBedPrintTable(bedList, erHash, itemName, expName, -1*maxScore, maxScore, stepSize, 2,
		    msBedDefaultPrintHeader, msBedExpressionPrintRow, printExprssnColorKey, getColorForExprBed);
    expRecordFreeList(&erList);
    hashFree(&erHash);
    bedFreeList(&bedList);
    }
printf("<h2></h2><p>\n");

if (printHtml)
    printf("%s", tdb->html);

if (printLinks)
    printLinks(itemName, chip);
}

static void loweRatioDetails(struct trackDb *tdb, char *expName, char *expTable,
	char *chip, float stepSize, float maxScore,
	void (*printLinks)(char *item, char *chip), boolean printHtml) 
/* print out a page for the affy data from gnf based on ratio of
 * measurements to the median of the measurements. */
{
struct bed *bedList;
char *itemName = cgiUsualString("i2","none");
struct expRecord *erList = NULL, *er;
char buff[32];
struct hash *erHash;

genericHeader(tdb, itemName);
bedList = loadMsBed(tdb->tableName, seqName, winStart, winEnd);
if(bedList == NULL)
    printf("<b>No Expression Data in this Range.</b>\n");
else 
    {
    erHash = newHash(2);
    erList = loadExpRecord(expTable, "hgFixed");
    for(er = erList; er != NULL; er=er->next)
	{
	snprintf(buff, sizeof(buff), "%d", er->id);
	hashAddUnique(erHash, buff, er);
	}
    printf("<h2></h2><p>\n");

    msBedPrintTable(bedList, erHash, itemName, expName, -1*maxScore, maxScore, stepSize, 2,
		    msBedDefaultPrintHeader, msBedExpressionPrintLoweRow, printExprssnColorKey, getColorForLoweExprBed);
    expRecordFreeList(&erList);
    hashFree(&erHash);
    bedFreeList(&bedList);
    }
printf("<h2></h2><p>\n");

if (printHtml)
    printf("%s", tdb->html);

if (printLinks)
    printLinks(itemName, chip);
}

void affyUclaDetails(struct trackDb *tdb, char *expName) 
/* print out a page for the affy data from gnf based on ratio of
 * measurements to the median of the measurements. */
{
genericRatioDetails(tdb, expName, "affyUclaExps", "U133", 0.25, 1.5,
	printAffyUclaLinks, TRUE);
}

static struct rgbColor getColorForCghBed(float val, float max)
/* Return the correct color for a given score */
{
char *colorScheme = cartUsualString(cart, "cghNci60.color", "gr");
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
    errAbort("ERROR: hgc::getColorForCghBed() maxDeviation can't be zero\n"); 
colorIndex = (int)(absVal * 255/max);
if(sameString(colorScheme, "gr")) 
    {
    if(val < 0) 
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
else if(sameString(colorScheme, "rg")) 
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
	color.g = 0;
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

static void msBedCghPrintRow(struct bed *bedList, struct hash *erHash, int expIndex, char *expName, float maxScore)
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

printf("<td align=left>  %s</td>\n", er->extras[1]);
for(bed = bedList;bed != NULL; bed = bed->next)
    {
    /* use the background colors to creat patterns */
    struct rgbColor rgb = getColorForCghBed(bed->expScores[expIndex], maxScore);
    printf("<td height=%d width=%d bgcolor=\"#%.2X%.2X%.2X\">&nbsp</td>\n", square, square, rgb.r, rgb.g, rgb.b);
    }
printf("</tr>\n");
}

void cghNci60Details(struct trackDb *tdb, char *expName) 
/* print out a page for the nci60 data from stanford */
{
struct bed *bedList;
char *tableName = "cghNci60Exps";
char *itemName = cgiUsualString("i2","none");
struct expRecord *erList = NULL, *er;
char buff[32];
struct hash *erHash;
float stepSize = 0.2;
float maxScore = 1.6;
bedList = loadMsBed(tdb->tableName, seqName, winStart, winEnd);
genericHeader(tdb, itemName);

printf("%s", tdb->html);
printf("<br><br>");
msBedGetExpDetailsLink(expName, tdb, tableName);
printf("<br><br>");
if(bedList == NULL)
    printf("<b>No CGH Data in this Range.</b>\n");
else 
    {
    erHash = newHash(2);
    erList = loadExpRecord(tableName, "hgFixed");
    for(er = erList; er != NULL; er=er->next)
	{
	snprintf(buff, sizeof(buff), "%d", er->id);
	hashAddUnique(erHash, buff, er);
	}
    msBedPrintTable(bedList, erHash, itemName, expName, 
    	-1 * maxScore,  maxScore, stepSize, 2,
	cghNci60PrintHeader, msBedCghPrintRow, 
	printExprssnColorKey, getColorForCghBed);
    expRecordFreeList(&erList);
    hashFree(&erHash);
    bedFreeList(&bedList);
    }
}

static void grcExpRatio(struct trackDb *tdb, char *item,
	void (*printLinks)(char *item, char *chip), boolean printHtml)
/* Print display for expRatio tracks with given link. */
{
char *expTable = trackDbRequiredSetting(tdb, "expTable");
char *expScale = trackDbRequiredSetting(tdb, "expScale");
char *expStep = trackDbRequiredSetting(tdb, "expStep");
char *chip = trackDbRequiredSetting(tdb, "chip");
double maxScore = atof(expScale);
double stepSize = atof(expStep);
genericRatioDetails(tdb, item, expTable, chip, stepSize, maxScore, 
	printLinks, printHtml);
}

static void loweExpRatio(struct trackDb *tdb, char *item,
	void (*printLinks)(char *item, char *chip), boolean printHtml)
/* Print display for expRatio tracks with given link. */
{
char *expTable = trackDbRequiredSetting(tdb, "expTable");
char *expScale = trackDbRequiredSetting(tdb, "expScale");
char *expStep = trackDbRequiredSetting(tdb, "expStep");
char *chip = trackDbRequiredSetting(tdb, "chip");
double maxScore = atof(expScale);
double stepSize = atof(expStep);
loweRatioDetails(tdb, item, expTable, chip, stepSize, maxScore, 
	printLinks, printHtml);
}


void loweExpRatioDetails(struct trackDb *tdb, char *item)
/* Put up details on some gnf track. */
{
loweExpRatio(tdb, item, NULL, FALSE);
}

void gnfExpRatioDetails(struct trackDb *tdb, char *item)
/* Put up details on some gnf track. */
{
grcExpRatio(tdb, item, printAffyGnfLinks, TRUE);
}

void genericExpRatio(struct sqlConnection *conn, struct trackDb *tdb, 
	char *item, int start)
/* Display details for expRatio tracks. */
{
grcExpRatio(tdb, item, NULL, FALSE);
}

void doAffyHumanExon(struct trackDb *tdb, char *item)
/* Display details for the affy all human exon track. */
/* It's approximately a expRatio track, without the required */
/* "chip" setting, and with additional probe details displayed. */
{
char *expTable = trackDbRequiredSetting(tdb, "expTable");
char *expScale = trackDbRequiredSetting(tdb, "expScale");
char *expStep = trackDbRequiredSetting(tdb, "expStep");
double maxScore = atof(expScale);
double stepSize = atof(expStep);
affyAllExonDetails(tdb, item, tdb->tableName, expTable, NULL, stepSize, maxScore);
}
