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
char *col = cartUsualString(cart, "nci60.color", "rg");
printf("<p><b>Cell Lines: </b> ");
nci60DropDown("nci60.type", nci60Map);
printf(" ");
printf(" <b>Color Scheme</b>: ");
cgiMakeRadioButton("nci60.color", "rg", sameString(col, "rg"));
printf(" red/green ");
cgiMakeRadioButton("nci60.color", "rb", sameString(col, "rb"));
printf(" red/blue ");
}

void rosettaUi(struct trackDb *tdb)
/* put up UI for the rosetta track */
{
char *rosettaMap = cartUsualString(cart, "rosetta.type", rosettaEnumToString(0));
char *col = cartUsualString(cart, "rosetta.color", "rg");
char *exonTypesOpts[] = {
    "Confirmed Only",
    "Predicted Only",
    "All",
};
char *exonTypes = cartUsualString(cart, "rosetta.et",  exonTypesOpts[0]);

printf("<p><b>Cell Lines: </b> ");
rosettaDropDown("rosetta.type", rosettaMap);
printf("  ");
printf("<b>Exons Shown:</b> ");
cgiMakeDropList("rosetta.et", exonTypesOpts, ArraySize(exonTypesOpts), exonTypes);
printf(" <b>Color Scheme</b>: ");
cgiMakeRadioButton("rosetta.color", "rg", sameString(col, "rg"));
printf(" red/green ");
cgiMakeRadioButton("rosetta.color", "rb", sameString(col, "rb"));
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


void specificUi(struct trackDb *tdb)
/* Draw track specific parts of UI. */
{
char *track = tdb->tableName;
if (sameString(track, "stsMap"))
    stsMapUi(tdb);
else if (sameString(track, "fishClones"))
    fishClonesUi(tdb);
else if (sameString(track, "nci60"))
    nci60Ui(tdb);
else if (sameString(track, "cghNci60"))
    cghNci60Ui(tdb);
else if (sameString(track, "mrna"))
    mrnaUi(tdb, FALSE);
else if (sameString(track, "est"))
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

cart = theCart;
track = cartString(cart, "g");
database = cartUsualString(cart, "db", hGetDb());
hSetDb(database);
chromosome = cartString(cart, "c");
conn = hAllocConn();
sprintf(where, "tableName = '%s'", track);
tdb = trackDbLoadWhere(conn, "trackDb", where);
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
cartHtmlShell("Track Settings", doMiddle, hUserCookie(), excludeVars);
return 0;
}
