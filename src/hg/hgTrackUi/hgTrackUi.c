/* hgTrackUi - Display track-specific user interface.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "cart.h"
#include "jksql.h"
#include "trackDb.h"
#include "hdb.h"
#include "hCommon.h"
#include "hui.h"
#include "sample.h"
#include "wiggle.h"

static char const rcsid[] = "$Id: hgTrackUi.c,v 1.75 2003/11/13 00:23:04 hiram Exp $";

struct cart *cart;	/* Cookie cart with UI settings */
char *database;		/* Current database. */
char *chromosome;	/* Chromosome. */


void radioButton(char *var, char *val, char *ourVal)
/* Print one radio button */
{
cgiMakeRadioButton(var, ourVal, sameString(ourVal, val));
printf("%s ", ourVal);
}

void filterButtons(char *filterTypeVar, char *filterTypeVal, boolean none)
/* Put up some filter buttons. */
{
printf("<B>Filter:</B> ");
radioButton(filterTypeVar, filterTypeVal, "red");
radioButton(filterTypeVar, filterTypeVal, "green");
radioButton(filterTypeVar, filterTypeVal, "blue");
radioButton(filterTypeVar, filterTypeVal, "exclude");
radioButton(filterTypeVar, filterTypeVal, "include");
if (none)
    radioButton(filterTypeVar, filterTypeVal, "none");
}


void stsMapUi(struct trackDb *tdb)
/* Put up UI stsMarkers. */
{
char *stsMapFilter = cartUsualString(cart, "stsMap.filter", "blue");
char *stsMapMap = cartUsualString(cart, "stsMap.type", smoeEnumToString(0));
filterButtons("stsMap.filter", stsMapFilter, TRUE);
printf(" ");
smoeDropDown("stsMap.type", stsMapMap);
}

void cbrWabaUi(struct trackDb *tdb)
/* Put up UI cbrWaba. */
{
char *cbrWabaFilter = cartUsualString(cart, "cbrWaba.filter", "red");
char *cbrWabaMap = cartUsualString(cart, "cbrWaba.type", fcoeEnumToString(0));
int start = cartInt(cart, "cbrWaba.start");
int end = cartInt(cart, "cbrWaba.end");
chromosome = cartString(cart, "c");
/*   This link is disabled in the external browser
printf(
"<P><A HREF=\"http://genome-test.cse.ucsc.edu/cgi-bin/tracks.exe?where=%s%%3A%d-%d\"> Temporary Intronerator link: %s:%d-%d</A> <I>for testing purposes only</I> \n</P>", chromosome+3, start, end, chromosome+3, start, end );
*/
}

void fishClonesUi(struct trackDb *tdb)
/* Put up UI fishClones. */
{
char *fishClonesFilter = cartUsualString(cart, "fishClones.filter", "red");
char *fishClonesMap = cartUsualString(cart, "fishClones.type", fcoeEnumToString(0));
filterButtons("fishClones.filter", fishClonesFilter, TRUE);
printf(" ");
fcoeDropDown("fishClones.type", fishClonesMap);
}

void recombRateUi(struct trackDb *tdb)
/* Put up UI recombRate. */
{
char *recombRateMap = cartUsualString(cart, "recombRate.type", rroeEnumToString(0));
printf("<b>Map Distances: </b>");
rroeDropDown("recombRate.type", recombRateMap);
}

void cghNci60Ui(struct trackDb *tdb)
/* Put up UI cghNci60. */
{
char *cghNci60Map = cartUsualString(cart, "cghNci60.type", cghoeEnumToString(0));
char *col = cartUsualString(cart, "cghNci60.color", "gr");
printf(" <b>Cell Lines: </b> ");
cghoeDropDown("cghNci60.type", cghNci60Map);
printf(" ");
printf(" <b>Color Scheme</b>: ");
cgiMakeRadioButton("cghNci60.color", "gr", sameString(col, "gr"));
printf(" green/red ");
cgiMakeRadioButton("cghNci60.color", "rg", sameString(col, "rg"));
printf(" red/green ");
cgiMakeRadioButton("cghNci60.color", "rb", sameString(col, "rb"));
printf(" red/blue ");
}

void nci60Ui(struct trackDb *tdb)
/* put up UI for the nci60 track from stanford track */
{
char *nci60Map = cartUsualString(cart, "nci60.type", nci60EnumToString(0));
char *col = cartUsualString(cart, "exprssn.color", "rg");
printf("<p><b>Cell Lines: </b> ");
nci60DropDown("nci60.type", nci60Map);
printf(" ");
printf(" <b>Color Scheme</b>: ");
cgiMakeRadioButton("exprssn.color", "rg", sameString(col, "rg"));
printf(" red/green ");
cgiMakeRadioButton("exprssn.color", "rb", sameString(col, "rb"));
printf(" red/blue ");
}

void affyUi(struct trackDb *tdb)
/* put up UI for the affy track from stanford track */
{
char *affyMap = cartUsualString(cart, "affy.type", affyEnumToString(affyTissue));
char *col = cartUsualString(cart, "exprssn.color", "rg");
printf("<p><b>Experiment Display: </b> ");
affyDropDown("affy.type", affyMap);
printf(" <b>Color Scheme</b>: ");
cgiMakeRadioButton("exprssn.color", "rg", sameString(col, "rg"));
printf(" red/green ");
cgiMakeRadioButton("exprssn.color", "rb", sameString(col, "rb"));
printf(" red/blue ");
}

void rosettaUi(struct trackDb *tdb)
/* put up UI for the rosetta track */
{
char *rosettaMap = cartUsualString(cart, "rosetta.type", rosettaEnumToString(0));
char *col = cartUsualString(cart, "exprssn.color", "rg");
char *exonTypes = cartUsualString(cart, "rosetta.et",  rosettaExonEnumToString(0));

printf("<p><b>Reference Sample: </b> ");
rosettaDropDown("rosetta.type", rosettaMap);
printf("  ");
printf("<b>Exons Shown:</b> ");
rosettaExonDropDown("rosetta.et", exonTypes);
printf(" <b>Color Scheme</b>: ");
cgiMakeRadioButton("exprssn.color", "rg", sameString(col, "rg"));
printf(" red/green ");
cgiMakeRadioButton("exprssn.color", "rb", sameString(col, "rb"));
printf(" red/blue ");
}

void refGeneUI(struct trackDb *tdb)
/* Put up refGene-specifc controls */
{
char *refGeneLabel = cartUsualString(cart, "refGene.label", "gene");
printf("<B>Label:</B> ");
radioButton("refGene.label", refGeneLabel, "gene");
radioButton("refGene.label", refGeneLabel, "accession");
radioButton("refGene.label", refGeneLabel, "both");
radioButton("refGene.label", refGeneLabel, "none");
}

void oneMrnaFilterUi(struct controlGrid *cg, char *text, char *var)
/* Print out user interface for one type of mrna filter. */
{
controlGridStartCell(cg);
printf("%s:<BR>", text);
cgiMakeTextVar(var, cartUsualString(cart, var, ""), 19);
controlGridEndCell(cg);
}

void mrnaUi(struct trackDb *tdb, boolean isXeno)
/* Put up UI for an mRNA (or EST) track. */
{
struct mrnaUiData *mud = newMrnaUiData(tdb->tableName, isXeno);
struct mrnaFilter *fil;
struct controlGrid *cg = NULL;
char *filterTypeVar = mud->filterTypeVar;
char *filterTypeVal = cartUsualString(cart, filterTypeVar, "red");
char *logicTypeVar = mud->logicTypeVar;
char *logicTypeVal = cartUsualString(cart, logicTypeVar, "and");

/* Define type of filter. */
filterButtons(filterTypeVar, filterTypeVal, FALSE);
printf("  <B>Combination Logic:</B> ");
radioButton(logicTypeVar, logicTypeVal, "and");
radioButton(logicTypeVar, logicTypeVal, "or");
printf("<BR>\n");

/* List various fields you can filter on. */
printf("<table border=0 cellspacing=1 cellpadding=1 width=%d><tr>\n", CONTROL_TABLE_WIDTH);
cg = startControlGrid(4, NULL);
for (fil = mud->filterList; fil != NULL; fil = fil->next)
     oneMrnaFilterUi(cg, fil->label, fil->key);
endControlGrid(&cg);
}

void crossSpeciesUi(struct trackDb *tdb)
/* Put up UI for selecting rainbow chromosome color or intensity score. */
{
char colorVar[256];
char filterVar[256];
char *colorSetting, *filterSetting;
char *filterVal = "";
/* initial value of chromosome coloring option is "on", unless
 * overridden by the colorChromDefault setting in the track */
char *colorDefault = trackDbSettingOrDefault(tdb, "colorChromDefault", "on");
printf("<p><b>Color track based on chromosome</b>: ");
snprintf(colorVar, sizeof(colorVar), "%s.color", tdb->tableName);
colorSetting = cartUsualString(cart, colorVar, colorDefault);
cgiMakeRadioButton(colorVar, "on", sameString(colorSetting, "on"));
printf(" on ");
cgiMakeRadioButton(colorVar, "off", sameString(colorSetting, "off"));
printf(" off ");
printf("<br><br>");
printf("<p><b>Filter by chromosome (eg. chr10) </b>: ");
snprintf(filterVar, sizeof(filterVar), "%s.chromFilter", tdb->tableName);
filterSetting = cartUsualString(cart, filterVar, filterVal);
cgiMakeTextVar(filterVar, cartUsualString(cart, filterVar, ""), 5);
}

void zooWiggleUi(struct trackDb *tdb )
/* put up UI for zoo track with one species on each line
 * and checkboxes to toggle each of them on/off*/
{

char options[7][256];
int thisHeightPer, thisLineGap;
float thisMinYRange, thisMaxYRange;
char *interpolate, *aa, *fill;

char **row;
int rowOffset;
struct sample *sample;
struct sqlResult *sr;

char option[64];
struct sqlConnection *conn = hAllocConn();

char *chromName = cartString(cart, "c");
char newRow = 0;

snprintf( &options[0][0], 256, "%s.heightPer", tdb->tableName );
snprintf( &options[1][0], 256, "%s.linear.interp", tdb->tableName );
snprintf( &options[3][0], 256, "%s.fill", tdb->tableName );
snprintf( &options[4][0], 256, "%s.min.cutoff", tdb->tableName );
snprintf( &options[5][0], 256, "%s.max.cutoff", tdb->tableName );
snprintf( &options[6][0], 256, "%s.interp.gap", tdb->tableName );

thisHeightPer = atoi(cartUsualString(cart, &options[0][0], "50"));
interpolate = cartUsualString(cart, &options[1][0], "Linear Interpolation");
fill = cartUsualString(cart, &options[3][0], "1");
thisMinYRange = atof(cartUsualString(cart, &options[4][0], "0.0"));
thisMaxYRange = atof(cartUsualString(cart, &options[5][0], "1000.0"));
thisLineGap = atoi(cartUsualString(cart, &options[6][0], "200"));

printf("<p><b>Interpolation: </b> ");
wiggleDropDown(&options[1][0], interpolate );
printf(" ");

printf("<br><br>");
printf(" <b>Fill Blocks</b>: ");
cgiMakeRadioButton(&options[3][0], "1", sameString(fill, "1"));
printf(" on ");

cgiMakeRadioButton(&options[3][0], "0", sameString(fill, "0"));
printf(" off ");

printf("<p><b>Track Height</b>:&nbsp;&nbsp;");
cgiMakeIntVar(&options[0][0], thisHeightPer, 5 );
printf("&nbsp;pixels");

printf("<p><b>Vertical Range</b>:&nbsp;&nbsp;\nmin:");
cgiMakeDoubleVar(&options[4][0], thisMinYRange, 6 );
printf("&nbsp;&nbsp;&nbsp;&nbsp;max:");
cgiMakeDoubleVar(&options[5][0], thisMaxYRange, 6 );

printf("<p><b>Toggle Species on/off</b><br>" );
sr = hRangeQuery(conn, tdb->tableName, chromName, 0, 1877426, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    sample = sampleLoad(row + rowOffset);
    snprintf( option, sizeof(option), "zooSpecies.%s", sample->name );
    if( cartUsualBoolean(cart, option, TRUE ) )
	cgiMakeCheckBox(option, TRUE );
    else 
	cgiMakeCheckBox(option, FALSE );
    printf("%s&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;", sample->name );

    newRow++;
    if( newRow % 6 == 0 ) printf("<br>");
		    
    sampleFree(&sample);
    }

}

void wigUi(struct trackDb *tdb)
/* UI for the wiggle track */
{
char *typeLine = NULL;	/*	to clone the tdb->type string	*/
char *words[8];		/*	chopping the typeLine	*/
int wordCount = 0;	/*	number of words found in typeLine */
char options[10][256];	/*	our option strings here	*/
char *heightPer;	/*	string from cart	*/
char *minY_str;	/*	string from cart	*/
char *maxY_str;	/*	string from cart	*/
double minYc;	/*	from cart */
double maxYc;	/*	from cart */
double minY;	/*	from the typeLine	*/
double maxY;	/*	from the typeLine	*/
int thisHeightPer;	/*	pixels per item	*/
char *interpolate;	/*	points only, or interpolate	*/
char *horizontalGrid;	/*	Grid lines, off by default */
char *lineBar;	/*	Line or Bar graph */
char *autoScale;	/*	Auto scaling on or off */
int maxHeightPixels = atoi(trackDbSettingOrDefault(tdb,
				"maxHeightPixels", DEFAULT_HEIGHT_PER));

minY = DEFAULT_MIN_Yv;
maxY = DEFAULT_MAX_Yv;

/*	Our min/max values are in the type field of the tdb */
/*	We need to pick those out from there just to be safe	*/
typeLine = cloneString(tdb->type);
wordCount = chopLine(typeLine, words);
/*	Possibly fetch values from the trackDb.ra file	*/
if( wordCount > 1 )
	minY = atof(words[1]);
if( wordCount > 2 )
	maxY = atof(words[2]);

snprintf( &options[0][0], 256, "%s.heightPer", tdb->tableName );
snprintf( &options[1][0], 256, "%s.linear.interp", tdb->tableName );
snprintf( &options[4][0], 256, "%s.minY", tdb->tableName );
snprintf( &options[5][0], 256, "%s.maxY", tdb->tableName );
snprintf( &options[7][0], 256, "%s.horizGrid", tdb->tableName );
snprintf( &options[8][0], 256, "%s.lineBar", tdb->tableName );
snprintf( &options[9][0], 256, "%s.autoScale", tdb->tableName );
minY_str = cartOptionalString(cart, &options[4][0]);
maxY_str = cartOptionalString(cart, &options[5][0]);

heightPer = cartOptionalString(cart, &options[0][0]);
thisHeightPer = maxHeightPixels;
if( heightPer ) thisHeightPer = min( atoi(heightPer), maxHeightPixels );
if( thisHeightPer < MIN_HEIGHT_PER ) thisHeightPer = MIN_HEIGHT_PER;

if( minY_str ) minYc = max( minY, atof(minY_str));
else minYc = minY;
if( maxY_str ) maxYc = min( maxY, atof(maxY_str));
else maxYc = maxY;

interpolate = cartUsualString(cart, &options[1][0], "Linear Interpolation");

/*
printf("<p><b>Interpolation: </b> ");
wiggleDropDown(&options[1][0], interpolate );
printf(" ");
*/

horizontalGrid = cartUsualString(cart, &options[7][0], "OFF");
lineBar = cartUsualString(cart, &options[8][0], "Bar");
autoScale = cartUsualString(cart, &options[9][0], "Auto-Scale to data view");

printf("<TABLE BORDER=0><TR><TD>\n");

printf("<b>Type of graph: </b> ");
wiggleGraphDropDown(&options[8][0], lineBar );
printf("</TD><TD>\n");

printf("<b>Horizontal Grid Lines: </b> ");
wiggleGridDropDown(&options[7][0], horizontalGrid );
printf(" (currently meaningless)</TD></TR><TR><TD COLSPAN=2>\n");

printf("<b>Track Height</b>:&nbsp;&nbsp;");
cgiMakeIntVar(&options[0][0], thisHeightPer, 5 );
printf("&nbsp;pixels&nbsp;(range:&nbsp;%d-%d)",
	MIN_HEIGHT_PER, maxHeightPixels);
printf("</TD></TR><TR><TD COLSPAN=2>\n");

printf("<b>Vertical Viewing Range</b>:&nbsp;&nbsp;\nmin:");
cgiMakeDoubleVar(&options[4][0], minYc, 6 );
printf("&nbsp;&nbsp;&nbsp;&nbsp;max:");
cgiMakeDoubleVar(&options[5][0], maxYc, 6 );
printf("&nbsp;&nbsp;&nbsp;&nbsp;Data view scaling:");
wiggleScaleDropDown(&options[9][0], autoScale );
printf("<BR>(data range limits:&nbsp;%.1f-%.1f)", minY, maxY);
printf("</TD></TR></TABLE>\n");

freeMem(typeLine);
}

void genericWiggleUi(struct trackDb *tdb, int optionNum )
/* put up UI for any standard wiggle track (a.k.a. sample track)*/
{

	char options[7][256];
	int thisHeightPer, thisLineGap;
	float thisMinYRange, thisMaxYRange;
	char *interpolate, *aa, *fill;

snprintf( &options[0][0], 256, "%s.heightPer", tdb->tableName );
snprintf( &options[1][0], 256, "%s.linear.interp", tdb->tableName );
snprintf( &options[3][0], 256, "%s.fill", tdb->tableName );
snprintf( &options[4][0], 256, "%s.min.cutoff", tdb->tableName );
snprintf( &options[5][0], 256, "%s.max.cutoff", tdb->tableName );
snprintf( &options[6][0], 256, "%s.interp.gap", tdb->tableName );

thisHeightPer = atoi(cartUsualString(cart, &options[0][0], "50"));
interpolate = cartUsualString(cart, &options[1][0], "Linear Interpolation");
fill = cartUsualString(cart, &options[3][0], "1");
thisMinYRange = atof(cartUsualString(cart, &options[4][0], "0.0"));
thisMaxYRange = atof(cartUsualString(cart, &options[5][0], "1000.0"));
thisLineGap = atoi(cartUsualString(cart, &options[6][0], "200"));

printf("<p><b>Interpolation: </b> ");
wiggleDropDown(&options[1][0], interpolate );
printf(" ");

printf("<br><br>");
printf(" <b>Fill Blocks</b>: ");
cgiMakeRadioButton(&options[3][0], "1", sameString(fill, "1"));
printf(" on ");

cgiMakeRadioButton(&options[3][0], "0", sameString(fill, "0"));
printf(" off ");

printf("<p><b>Track Height</b>:&nbsp;&nbsp;");
cgiMakeIntVar(&options[0][0], thisHeightPer, 5 );
printf("&nbsp;pixels");

printf("<p><b>Vertical Range</b>:&nbsp;&nbsp;\nmin:");
cgiMakeDoubleVar(&options[4][0], thisMinYRange, 6 );
printf("&nbsp;&nbsp;&nbsp;&nbsp;max:");
cgiMakeDoubleVar(&options[5][0], thisMaxYRange, 6 );

if( optionNum >= 7 )
    {
    printf("<p><b>Maximum Interval to Interpolate Across</b>:&nbsp;&nbsp;");
    cgiMakeIntVar(&options[6][0], thisLineGap, 10 );
    printf("&nbsp;bases");
    }
}



void humMusUi(struct trackDb *tdb, int optionNum )
/* put up UI for human/mouse conservation sample tracks (humMusL and musHumL)*/
{

	char options[7][256];
	int thisHeightPer, thisLineGap;
	float thisMinYRange, thisMaxYRange;
	char *interpolate, *aa, *fill;

snprintf( &options[0][0], 256, "%s.heightPer", tdb->tableName );
snprintf( &options[1][0], 256, "%s.linear.interp", tdb->tableName );
snprintf( &options[3][0], 256, "%s.fill", tdb->tableName );
snprintf( &options[4][0], 256, "%s.min.cutoff", tdb->tableName );
snprintf( &options[5][0], 256, "%s.max.cutoff", tdb->tableName );
snprintf( &options[6][0], 256, "%s.interp.gap", tdb->tableName );

thisHeightPer = atoi(cartUsualString(cart, &options[0][0], "50"));
interpolate = cartUsualString(cart, &options[1][0], "Linear Interpolation");
fill = cartUsualString(cart, &options[3][0], "1");
thisMinYRange = atof(cartUsualString(cart, &options[4][0], "0.0"));
thisMaxYRange = atof(cartUsualString(cart, &options[5][0], "8.0"));
thisLineGap = atoi(cartUsualString(cart, &options[6][0], "200"));

printf("<p><b>Interpolation: </b> ");
wiggleDropDown(&options[1][0], interpolate );
printf(" ");

printf("<br><br>");
printf(" <b>Fill Blocks</b>: ");
cgiMakeRadioButton(&options[3][0], "1", sameString(fill, "1"));
printf(" on ");

cgiMakeRadioButton(&options[3][0], "0", sameString(fill, "0"));
printf(" off ");

printf("<p><b>Track Height</b>:&nbsp;&nbsp;");
cgiMakeIntVar(&options[0][0], thisHeightPer, 5 );
printf("&nbsp;pixels");

printf("<p><b>Vertical Range</b>:&nbsp;&nbsp;\nmin:");
cgiMakeDoubleVar(&options[4][0], thisMinYRange, 6 );
printf("&nbsp;&nbsp;&nbsp;&nbsp;max:");
cgiMakeDoubleVar(&options[5][0], thisMaxYRange, 6 );

if( optionNum >= 7 )
{
	printf("<p><b>Maximum Interval to Interpolate Across</b>:&nbsp;&nbsp;");
	cgiMakeIntVar(&options[6][0], thisLineGap, 10 );
	printf("&nbsp;bases");
}
}




void affyTranscriptomeUi(struct trackDb *tdb)
	/* put up UI for the GC percent track (a sample track)*/
{
	int affyTranscriptomeHeightPer = atoi(cartUsualString(cart, "affyTranscriptome.heightPer", "100"));
	char *interpolate = cartUsualString(cart, "affyTranscriptome.linear.interp", "Only samples");
	char *aa = cartUsualString(cart, "affyTranscriptome.anti.alias", "on");
	char *fill = cartUsualString(cart, "affyTranscriptome.fill", "1");

	printf("<br><br>");
	printf(" <b>Fill Blocks</b>: ");
	cgiMakeRadioButton("affyTranscriptome.fill", "1", sameString(fill, "1"));
	printf(" on ");

	cgiMakeRadioButton("affyTranscriptome.fill", "0", sameString(fill, "0"));
	printf(" off ");

	printf("<p><b>Track Height</b>:&nbsp;&nbsp;");
	cgiMakeIntVar("affyTranscriptome.heightPer", affyTranscriptomeHeightPer, 5 );
	printf("&nbsp;pixels");

}


void ancientRUi(struct trackDb *tdb)
	/* put up UI for the ancient repeats track to let user enter an
	 * integer to filter out those repeats with less aligned bases.*/
{
	int ancientRMinLength = atoi(cartUsualString(cart, "ancientR.minLength", "50"));
	printf("<p><b>Length Filter</b><br>Exclude aligned repeats with less than ");
	cgiMakeIntVar("ancientR.minLength", ancientRMinLength, 4 );
	printf("aligned bases (not necessarily identical). Enter 0 for no filtering.");
}



void specificUi(struct trackDb *tdb)
	/* Draw track specific parts of UI. */
{
char *track = tdb->tableName;
char *typeLine = NULL;
char *words[8];
int wordCount = 0;

if (sameString(track, "stsMap"))
        stsMapUi(tdb);
else if (sameString(track, "cbr_waba"))
        cbrWabaUi(tdb);
else if (sameString(track, "fishClones"))
        fishClonesUi(tdb);
else if (sameString(track, "recombRate"))
        recombRateUi(tdb);
else if (sameString(track, "nci60"))
        nci60Ui(tdb);
else if (sameString(track, "cghNci60"))
        cghNci60Ui(tdb);
else if (sameString(track, "xenoRefGene"))
        refGeneUI(tdb);
else if (sameString(track, "refGene"))
        refGeneUI(tdb);
else if (sameString(track, "mrna"))
        mrnaUi(tdb, FALSE);
else if (sameString(track, "est"))
        mrnaUi(tdb, FALSE);
else if (sameString(track, "tightMrna"))
        mrnaUi(tdb, FALSE);
else if (sameString(track, "tightEst"))
        mrnaUi(tdb, FALSE);
else if (sameString(track, "intronEst"))
        mrnaUi(tdb, FALSE);
else if (sameString(track, "xenoMrna"))
        mrnaUi(tdb, TRUE);
else if (sameString(track, "xenoEst"))
        mrnaUi(tdb, TRUE);
else if (sameString(track, "rosetta"))
        rosettaUi(tdb);
else if (startsWith("wig", tdb->type))
        wigUi(tdb);
else if (sameString(track, "affyRatio"))
        affyUi(tdb);
else if (sameString(track, "ancientR"))
        ancientRUi(tdb);
else if (sameString(track, "zoo") || sameString(track, "zooNew" ))
         zooWiggleUi(tdb);
else if (sameString(track, "humMusL") ||
         sameString( track, "musHumL") ||
         sameString( track, "regpotent") ||
         sameString( track, "mm3Rn2L" )	 ||
         sameString( track, "mm3Hg15L" ) ||
         sameString( track, "hg15Mm3L" ))
            humMusUi(tdb,7);
/* NOTE: type psl xeno <otherDb> tracks use crossSpeciesUi, so
 * add explicitly here only if track has another type (bed, chain).
 * For crossSpeciesUi, the
 * default for chrom coloring is "on", unless track setting
 * colorChromDefault is set to "off" */
else if (startsWith("selfChain", track))
    crossSpeciesUi(tdb);
else if (sameString(track, "orthoTop4"))
    /* still used ?? */
    crossSpeciesUi(tdb);
else if (sameString(track, "mouseOrtho"))
    crossSpeciesUi(tdb);
else if (sameString(track, "mouseSyn"))
    crossSpeciesUi(tdb);
else if (sameString(track, "affyTranscriptome"))
    affyTranscriptomeUi(tdb);
else if (startsWith("sample", tdb->type))
    genericWiggleUi(tdb,7);
else 
    {
    /* handle all tracks with type "psl xeno <otherDb>" */
    if (tdb->type != NULL)
        typeLine = cloneString(tdb->type);
        wordCount = chopLine(typeLine, words);
        if (wordCount == 3)
            {
            if (sameWord(words[0], "psl") && sameWord(words[1], "xeno"))
                crossSpeciesUi(tdb);
            }
        freeMem(typeLine);
    }
}

void trackUi(struct trackDb *tdb)
/* Put up track-specific user interface. */
{
char *vis = hStringFromTv(tdb->visibility);
printf("<H1>%s</H1>\n", tdb->longLabel);
printf("<B>Display mode:</B>");
hTvDropDown(tdb->tableName, hTvFromString(cartUsualString(cart, tdb->tableName, vis)),
	tdb->canPack);
printf(" ");
cgiMakeButton("Submit", "Submit");
printf("<BR>\n");

specificUi(tdb);

if (tdb->html != NULL && tdb->html[0] != 0)
    {
    htmlHorizontalLine();
    puts(tdb->html);
    }
}

void doMiddle(struct cart *theCart)
/* Write body of web page. */
{
struct trackDb *tdb;
struct sqlConnection *conn;
char where[256];
char *track;
char *trackDb = hTrackDbName();
cart = theCart;
track = cartString(cart, "g");
database = cartUsualString(cart, "db", hGetDb());
hSetDb(database);
chromosome = cartString(cart, "c");
conn = hAllocConn();
sprintf(where, "tableName = '%s'", track);
tdb = trackDbLoadWhere(conn, trackDb, where);
hLookupStringsInTdb(tdb, database);
if (tdb == NULL)
   errAbort("Can't find %s in track database %s chromosome %s", track, database, chromosome);
printf("<FORM ACTION=\"%s\">\n\n", hgTracksName());
cartSaveSession(cart);
trackUi(tdb);
printf("</FORM>");
}

char *excludeVars[] = { "submit", "Submit", "g", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
htmlSetBackground("../images/floret.jpg");
cartHtmlShell("Track Settings", doMiddle, hUserCookie(), excludeVars, NULL);
return 0;
}
