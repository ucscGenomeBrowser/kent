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

void colorCrossSpeciesUi(struct trackDb *tdb, boolean colorDefaultOn)
/* Put up UI for selecting rainbow chromosome color or intensity score. */
{
char colorVar[256];
char filterVar[256];
char *aa, *bb;
char *colorVal = (colorDefaultOn ? "on" : "off");
char *filterVal = "";

printf("<p><b>Color track based on chromosome</b>: ");
snprintf(colorVar, sizeof(colorVar), "%s.color", tdb->tableName);
aa = cartUsualString(cart, colorVar, colorVal);
cgiMakeRadioButton(colorVar, "on", sameString(aa, "on"));
printf(" on ");
cgiMakeRadioButton(colorVar, "off", sameString(aa, "off"));
printf(" off ");
printf("<br><br>");
printf("<p><b>Filter by chromosome (eg. chr10) </b>: ");
snprintf(filterVar, sizeof(filterVar), "%s.chromFilter", tdb->tableName);
bb = cartUsualString(cart, filterVar, filterVal);
cgiMakeTextVar(filterVar, cartUsualString(cart, filterVar, ""), 5);
}

void genericWiggleUi(struct trackDb *tdb, int optionNum )
/* put up UI for any standard wiggle track (a.k.a. sample track)*/
{

char options[5][256];
int thisHeightPer, thisLineGap;
char *interpolate, *aa, *fill;

snprintf( &options[0][0], 256, "%s.heightPer", tdb->tableName );
snprintf( &options[1][0], 256, "%s.linear.interp", tdb->tableName );
snprintf( &options[3][0], 256, "%s.fill", tdb->tableName );
snprintf( &options[4][0], 256, "%s.interp.gap", tdb->tableName );
    
thisHeightPer = atoi(cartUsualString(cart, &options[0][0], "50"));
interpolate = cartUsualString(cart, &options[1][0], "Linear Interpolation");
fill = cartUsualString(cart, &options[3][0], "1");
thisLineGap = atoi(cartUsualString(cart, &options[4][0], "200"));

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

if( optionNum >= 5 )
    {
    printf("<p><b>Maximum Interval to Interpolate Across</b>:&nbsp;&nbsp;");
    cgiMakeIntVar(&options[4][0], thisLineGap, 10 );
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

if (sameString(track, "stsMap"))
    stsMapUi(tdb);
else if (sameString(track, "fishClones"))
    fishClonesUi(tdb);
else if (sameString(track, "recombRate"))
    recombRateUi(tdb);
else if (sameString(track, "nci60"))
    nci60Ui(tdb);
else if (sameString(track, "cghNci60"))
    cghNci60Ui(tdb);
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
else if (sameString(track, "xenoBestMrna"))
    mrnaUi(tdb, TRUE);
else if (sameString(track, "xenoEst"))
    mrnaUi(tdb, TRUE);
else if (sameString(track, "rosetta"))
    rosettaUi(tdb);
else if (sameString(track, "affyRatio"))
    affyUi(tdb);
else if (sameString(track, "ancientR"))
    ancientRUi(tdb);
else if (sameString(track, "wiggle"))
    genericWiggleUi(tdb,5);
else if (sameString(track, "chimp"))
    genericWiggleUi(tdb,5);
else if (sameString(track, "GCwiggle"))
    genericWiggleUi(tdb,5);
else if (sameString(track, "pGCwiggle"))
    genericWiggleUi(tdb,5);
else if (sameString(track, "zoo"))
    genericWiggleUi(tdb,4);
else if (sameString(track, "binomialCons"))
    genericWiggleUi(tdb,5);
else if (sameString(track, "binomialCons2"))
    genericWiggleUi(tdb,5);
else if (sameString(track, "binomialCons3"))
    genericWiggleUi(tdb,5);
else if (sameString(track, "footPrinter"))
    genericWiggleUi(tdb,5);
else if (sameString(track, "footPrinterHMR"))
    genericWiggleUi(tdb,5);
else if (sameString(track, "humMus"))
    genericWiggleUi(tdb,5);
else if (sameString(track, "humMusL"))
    genericWiggleUi(tdb,5);
else if (sameString(track, "musHumL"))
    genericWiggleUi(tdb,5);
else if (sameString(track, "zooCons"))
    genericWiggleUi(tdb,5);
else if (sameString(track, "blastzMm2"))
    colorCrossSpeciesUi(tdb, FALSE);
else if (sameString(track, "blastzHuman"))
    colorCrossSpeciesUi(tdb, FALSE);
else if (sameString(track, "blastzMm2Sc"))
    colorCrossSpeciesUi(tdb, FALSE);
else if (sameString(track, "blastzMm2Ref"))
    colorCrossSpeciesUi(tdb, FALSE);
else if (sameString(track, "blastzHgRef"))
    colorCrossSpeciesUi(tdb, FALSE);
else if (sameString(track, "blastzRecipBest"))
    colorCrossSpeciesUi(tdb, FALSE);
else if (sameString(track, "blastzHg"))
    colorCrossSpeciesUi(tdb, FALSE);
else if (sameString(track, "blastzBestMouse"))
    colorCrossSpeciesUi(tdb, FALSE);
else if (sameString(track, "blastzMouse"))
    colorCrossSpeciesUi(tdb, FALSE);
else if (sameString(track, "blastzBestHuman"))
    colorCrossSpeciesUi(tdb, FALSE);
else if (sameString(track, "blastzMmHg"))
    colorCrossSpeciesUi(tdb, FALSE);
else if (sameString(track, "blastzMmHg12"))
    colorCrossSpeciesUi(tdb, FALSE);
else if (sameString(track, "blastzMmHg12Best"))
    colorCrossSpeciesUi(tdb, FALSE);
else if (sameString(track, "blastzMmHgRef"))
    colorCrossSpeciesUi(tdb, FALSE);
else if (sameString(track, "blastzAllHuman"))
    colorCrossSpeciesUi(tdb, FALSE);
else if (sameString(track, "aarMm2"))
    colorCrossSpeciesUi(tdb, FALSE);
else if (sameString(track, "orthoTop4"))
    colorCrossSpeciesUi(tdb, TRUE);
else if (sameString(track, "mouseOrtho"))
    colorCrossSpeciesUi(tdb, TRUE);
else if (sameString(track, "mouseSyn"))
    colorCrossSpeciesUi(tdb, TRUE);
else if (sameString(track, "blatMus"))
    colorCrossSpeciesUi(tdb, FALSE);
else if (sameString(track, "blatHuman"))
    colorCrossSpeciesUi(tdb, FALSE);
else if (sameString(track, "affyTranscriptome"))
    affyTranscriptomeUi(tdb);
else if (sameString(tdb->type, "sample 9"))
    genericWiggleUi(tdb,5);
}

void trackUi(struct trackDb *tdb)
/* Put up track-specific user interface. */
{
char *vis = hStringFromTv(tdb->visibility);
printf("<H1>%s</H1>\n", tdb->longLabel);
printf("<B>Display mode:</B>");
hTvDropDown(tdb->tableName, hTvFromString(cartUsualString(cart, tdb->tableName, vis)));
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
