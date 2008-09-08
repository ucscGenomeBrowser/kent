#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "hdb.h"
#include "web.h"
#include "hui.h"
#include "hdb.h"
#include "trackDb.h"
#include "customTrack.h"
#include "hgSeq.h"
#include "hgTables.h"
#include "hgMaf.h"
#include "mafGene.h"

#define MAX_SP_SIZE 2000

#define STATIC_ARRAY_SIZE   1024
static char *mafTrackNames[STATIC_ARRAY_SIZE];
static char *mafTrackExist[STATIC_ARRAY_SIZE];
//static char *speciesNames[STATIC_ARRAY_SIZE];

#ifdef NTONOW
static struct slName *getSpecies(char *mafTable, boolean doPrint)
{
char option[MAX_SP_SIZE];
struct slName *speciesList = NULL;
int ii;
//errAbort("Looking for %s in %s\n", mafTable, database);
struct trackDb *tdb = hTrackDbForTrack(database, mafTable);
//errAbort("tdb %p\n",tdb);
char *speciesOrder = trackDbSetting(tdb, SPECIES_ORDER_VAR);
char *speciesGroup = trackDbSetting(tdb, SPECIES_GROUP_VAR);
char *speciesUseFile = trackDbSetting(tdb, SPECIES_USE_FILE);
char *groups[20];
int speciesCt = 0, groupCt = 1;
int group;
char sGroup[24];
char *species[MAX_SP_SIZE];
/* determine species and groups for pairwise -- create checkboxes */
if (speciesOrder == NULL && speciesGroup == NULL && speciesUseFile == NULL)
    {
//    if (isCustomTrack(tdb->tableName))
//	return;
    errAbort("Track %s missing required trackDb setting: speciesOrder, speciesGroups, or speciesUseFile", tdb->tableName);
    }

slAddHead(&speciesList, slNameNew(database));

if (speciesGroup)
    groupCt = chopLine(speciesGroup, groups);

if (speciesUseFile)
    {
    if ((speciesGroup != NULL) || (speciesOrder != NULL))
	errAbort("Can't specify speciesUseFile and speciesGroup or speciesOrder");
    speciesOrder = cartGetOrderFromFile(database, cart, speciesUseFile);
    }

for (group = 0; group < groupCt; group++)
    {
    if (groupCt != 1 || !speciesOrder)
        {
        safef(sGroup, sizeof sGroup, "%s%s", 
                                SPECIES_GROUP_PREFIX, groups[group]);
        speciesOrder = trackDbRequiredSetting(tdb, sGroup);
        }
    speciesCt = chopLine(speciesOrder, species);
    for (ii = 0; ii < speciesCt; ii++)
        {
//	printf("looking at %s\n", species[ii]);
        slAddTail(&speciesList, slNameNew(species[ii]));
        }
    }
struct slName *sl;

if (doPrint)
    {
    printf("<B>Species to include:</B><BR>\n");
    puts("\n<TABLE><TR>");
    }

ii=0;
struct slName *includeList = NULL;
struct slName *next;

for(sl = speciesList; sl; sl = next)
    {
    next = sl->next;
    sl->next = NULL;
    safef(option, sizeof(option), "%s.%s", tdb->tableName, sl->name);
    int on = cartUsualBoolean(cart, option, TRUE);
    
    if (on)
	slAddHead(&includeList, sl);

    if (doPrint)
	{
	puts("<TD>");
	cgiMakeCheckBox(option, on);
	char *label = hOrganism(sl->name);
	if (label == NULL)
		label = sl->name;
	printf ("%s<BR>", label);
	puts("</TD>");
	if (++ii ==  5)
	    {
	    puts("</TR><TR>");
	    ii=0;
	    }
	}
    }
if (doPrint)
    puts("</TR></TABLE><BR>");

if(includeList)
    slReverse(&includeList);

return includeList;
}
#endif

void doGenePredPal(struct sqlConnection *conn)
/* Output genePred protein alignment. */
{
char *mafTable = cartString(cart, "mafTable");
if (anySubtrackMerge(database, curTable))
    errAbort("Can't do protein alignment output when subtrack merge is on. "
    "Please go back and select another output type (BED or custom track is good), or clear the subtrack merge.");
if (anyIntersection())
    errAbort("Can't do protein alignment output when intersection is on. "
    "Please go back and select another output type (BED or custom track is good), or clear the intersection.");
textOpen();

boolean inExons = cartUsualBoolean(cart, "mafGeneExons", TRUE); 
boolean noTrans = cartUsualBoolean(cart, "mafGeneNoTrans", FALSE); 
boolean outBlank = cartUsualBoolean(cart, "mafGeneOutBlank", FALSE); 
struct region *regionList = getRegions();
struct region *region;
struct hTableInfo *hti = NULL;
//struct dyString *fieldSpec = newDyString(256);
struct hash *idHash = NULL;
int outCount = 0;
boolean isPositional;
//int fieldCount;
char *idField;
//boolean showItemRgb = FALSE;
//int itemRgbCol = -1;	/*	-1 means not found	*/
//boolean printedColumns = FALSE;

hti = getHti(database, curTable);
idField = getIdField(database, curTrack, curTable, hti);
//showItemRgb=bedItemRgb(curTrack);	/* should we expect itemRgb */
					/*	instead of "reserved" */


#ifdef NOTNOW
/* If they didn't pass in a field list assume they want all fields. */
if (fields != NULL)
    {
    dyStringAppend(fieldSpec, fields);
    fieldCount = countChars(fields, ',') + 1;
    }
else
    {
    dyStringAppend(fieldSpec, "*");
    fieldCount = countTableColumns(conn, table);
    }

dyStringAppend(fieldSpec, fields);
fieldCount = countChars(fields, ',') + 1;
#endif

/* If can find id field for table then get
 * uploaded list of identifiers, create identifier hash
 * and add identifier column to end of result set. */
if (idField != NULL)
    {
    idHash = identifierHash(database, curTable);
    /*
    if (idHash != NULL)
	{
	dyStringAppendC(fieldSpec, ',');
	dyStringAppend(fieldSpec, idField);
	}
	*/
    }
isPositional = htiIsPositional(hti);

struct genePred *list = NULL;

/* Loop through each region. */
for (region = regionList; region != NULL; region = region->next)
    {
    struct sqlResult *sr;
    char **row;
    //int colIx, lastCol = fieldCount-1;
    char *filter = filterClause(database, curTable, region->chrom);

    sr = regionQuery(conn, curTable, "*", 
    	region, isPositional, filter);
    if (sr == NULL)
	continue;

    while ((row = sqlNextRow(sr)) != NULL)
	{
	struct genePred *pred;
	if (hti->hasBin)
	    pred = genePredExtLoad(&row[1], GENEPREDX_NUM_COLS);
	else
	    pred = genePredExtLoad(row, GENEPREDX_NUM_COLS);
	if (idHash == NULL || hashLookup(idHash, pred->name))
	    {
	    slAddHead(&list, pred);
//	    hPrintf("%s\n", pred->name);
	    ++outCount;
	    }
	}
    sqlFreeResult(&sr);
    freez(&filter);
    }

if (list != NULL)
    {
    struct genePred *pred;
    unsigned options = 0;

    if (inExons)
	options |= MAFGENE_EXONS;
    if (noTrans)
	options |= MAFGENE_NOTRANS;
    if (outBlank)
	options |= MAFGENE_OUTBLANK;

    struct trackDb *maftdb = hTrackDbForTrack(database, mafTable);
    struct wigMafSpecies *wmSpecies;
    int groupCnt;

    wigMafGetSpecies(cart, maftdb, database, &wmSpecies, &groupCnt);
    struct slName *includeList = slNameNew(database);
    for(; wmSpecies; wmSpecies = wmSpecies->next)
	{
	if (wmSpecies->on)
	    {
	    struct slName *newName = slNameNew(wmSpecies->name);
	    slAddHead(&includeList, newName);
	    }
	}
    slReverse(&includeList);
    for(pred = list; pred ; pred = pred->next)
	{
	mafGeneOutPred(stdout, pred, database, mafTable, includeList, options);
	}
    }

/* Do some error diagnostics for user. */
if (outCount == 0)
    explainWhyNoResults(NULL);
hashFree(&idHash);
}

#ifdef NOTNOW
static char *onChangeMafTable()
/* Return javascript executed when they change clade. */
{
//struct dyString *dy = onChangeStart();
struct dyString *dy = jsOnChangeStart();
jsDropDownCarryOver(dy, "mafTable");
//jsDropDownCarryOver(dy, "mafTable");
dyStringAppend(dy, " document.hiddenForm.org.value=0;");
dyStringAppend(dy, " document.hiddenForm.db.value=0;");
dyStringAppend(dy, " document.hiddenForm.position.value='';");
return jsOnChangeEnd(&dy);
}
#endif


static void palOptions(struct trackDb *track, char *type, 
	struct sqlConnection *conn)
/* Put up sequence type options for gene prediction tracks. */
{
htmlOpen("Select multiple alignment and options for %s", track->shortLabel);

char *mafTable = cartOptionalString(cart, "mafTable");
//char option[MAX_SP_SIZE];
char query[512];
//struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
safef(query, sizeof query, "select tableName from trackDb where type like 'wigMaf%%'");
char *checked = NULL;
int ii;

sr = sqlGetResult(conn, query);
int count = 0;
while ((row = sqlNextRow(sr)) != NULL)
    {
    mafTrackNames[count] = cloneString(row[0]);
    if (count == STATIC_ARRAY_SIZE)
	break;
    count++;
    }

sqlFreeResult(&sr);

int count2 = 0;
for(ii=0; ii < count; ii++)
    {
    if (sqlTableExists(conn, mafTrackNames[ii]))
	{
	mafTrackExist[count2] = mafTrackNames[ii];
	if ((mafTable != NULL) && sameString(mafTable, mafTrackExist[count2]))
	    checked = mafTrackExist[count2];
	count2++;
	}
    }

if ((mafTable == NULL) || (checked == NULL))
    checked = mafTable = mafTrackExist[0];

hPrintf("<FORM ACTION=\"%s\" METHOD=GET>\n", getScriptName());
//cgiMakeDropListFull("mafTable", mafTrackExist, NULL,  count2,  checked, onChangeMafTable);
printf("MAF table: \n");
cgiMakeDropList("mafTable", mafTrackExist, count2,  checked);
//cartSetString(cart, "mafTable", mafTable);
cgiMakeButton(hgtaDoTopSubmit, "submit");
hPrintf("</FORM>\n");

#define MAIN_FORM "mainForm"
hPrintf("<FORM ACTION=\"%s\" NAME=\""MAIN_FORM"\" METHOD=%s>\n\n",
       getScriptName(), "POST");
//hPrintf("<FORM ACTION=\"%s\" METHOD=POST>\n", getScriptName());
boolean inExons = cartUsualBoolean(cart, "mafGeneExons", TRUE); 
boolean noTrans = cartUsualBoolean(cart, "mafGeneNoTrans", FALSE); 
boolean outBlank = cartUsualBoolean(cart, "mafGeneOutBlank", FALSE); 

printf("<BR><B>Formatting options:</B><BR>\n");
cgiMakeCheckBox("mafGeneExons", inExons);
printf("Separate into exons<BR>");
cgiMakeCheckBox("mafGeneNoTrans", noTrans);
printf("Don't translate into amino acids<BR>");
cgiMakeCheckBox("mafGeneOutBlank", outBlank);
printf("Output lines with just dashes<BR>");


printf("<BR>");
struct trackDb *maftdb = hTrackDbForTrack(database, mafTable);
wigMafSpeciesTable(cart, maftdb, mafTable, database);

//getSpecies(mafTable, TRUE);
cgiMakeButton(hgtaDoPalOut, "submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "cancel");
hPrintf("</FORM>\n");
cartSaveSession(cart);
htmlClose();
}

void doOutPal(struct sqlConnection *conn)
/* Output fasta page. */
{
palOptions(curTrack, curTrack->type, conn);
}
