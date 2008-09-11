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
#include "genePredReader.h"
#include "jsHelper.h"

boolean isGenePredTable(struct trackDb *track, char *table)
/* Return TRUE if table is genePred. */
{
char setting[128], *p = setting;

if (track == NULL)
    return FALSE;
if (isEmpty(track->type))
    return FALSE;
safecpy(setting, sizeof setting, track->type);
char *type = nextWord(&p);

if (sameString(type, "genePred"))
    if (sameString(track->tableName, table))
        return TRUE;
return FALSE;
}

#define MAX_SP_SIZE 2000

#define STATIC_ARRAY_SIZE   1024
static char *mafTrackNames[STATIC_ARRAY_SIZE];
static char *mafTrackExist[STATIC_ARRAY_SIZE];
//static char *speciesNames[STATIC_ARRAY_SIZE];

#define hgtaCGIGeneMafTable "hgta_mafGeneMafTable" 
#define hgtaJSGeneMafTable  "mafGeneMafTable" 
#define hgtaCGIGeneExons "hgta_mafGeneExons" 
#define hgtaJSGeneExons  "mafGeneExons" 
#define hgtaCGIGeneNoTrans "hgta_mafGeneNoTrans" 
#define hgtaJSGeneNoTrans  "mafGeneNoTrans" 
#define hgtaCGIGeneOutBlank "hgta_mafGeneOutBlank" 
#define hgtaJSGeneOutBlank  "mafGeneOutBlank" 

static char *curVars[] = {
	hgtaCGIGeneMafTable, hgtaCGIGeneExons,
	hgtaCGIGeneNoTrans, hgtaCGIGeneOutBlank
	};

struct genePredReader *regionGenePredQuery(struct sqlConnection *conn, 
	char *table,  struct region *region, char *extraWhere)
/* Construct and execute query for table on region. Returns NULL if 
 * table doesn't exist (e.g. missing split table for region->chrom). */
{
struct genePredReader *reader;

/* Check for missing split tables before querying: */
char *db = sqlGetDatabase(conn);
struct hTableInfo *hti = hFindTableInfo(db, region->chrom, table);
if (hti == NULL)
    return NULL;
else if (hti->isSplit)
    {
    char fullTableName[256];
    safef(fullTableName, sizeof(fullTableName),
	  "%s_%s", region->chrom, table);
    if (!sqlTableExists(conn, fullTableName))
	return NULL;
    }

if (region->fullChrom) /* Full chromosome. */
    {
    char extra[2048];

    if (extraWhere)
	safef(extra, sizeof extra, "chrom='%s' AND %s",region->chrom,extraWhere);
    else
	safef(extra, sizeof extra, "chrom='%s'",region->chrom);
    reader = genePredReaderQuery( conn, table, extra);
    }
else
    {
    reader = genePredReaderRangeQuery( conn,  table, region->chrom, 
	region->start, region->end,  extraWhere);
    }

return reader;
}

void doGenePredPal(struct sqlConnection *conn)
/* Output genePred protein alignment. */
{
cartRemove(cart, hgtaDoPal);
cartRemove(cart, hgtaDoPalOut);
cartSaveSession(cart);
struct genePredReader *reader = NULL;
if (anySubtrackMerge(database, curTable))
    errAbort("Can't do protein alignment output when subtrack merge is on. "
    "Please go back and select another output type (BED or custom track is good), or clear the subtrack merge.");
if (anyIntersection())
    errAbort("Can't do protein alignment output when intersection is on. "
    "Please go back and select another output type (BED or custom track is good), or clear the intersection.");

textOpen();

char *mafTable = cartString(cart, hgtaCGIGeneMafTable); 
boolean inExons = cartUsualBoolean(cart, hgtaCGIGeneExons , TRUE); 
boolean noTrans = cartUsualBoolean(cart, hgtaCGIGeneNoTrans, FALSE); 
boolean outBlank = cartUsualBoolean(cart, hgtaCGIGeneOutBlank, FALSE); 
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
    struct genePred *pred;
    char *filter = filterClause(database, curTable, region->chrom);

    reader = regionGenePredQuery(conn, curTable, region, filter);
    if (reader == NULL)
	continue;

    //while ((row = sqlNextRow(sr)) != NULL)
    while ((pred  = genePredReaderNext(reader)) != NULL)
	{
	if (idHash == NULL || hashLookup(idHash, pred->name))
	    {
	    slAddHead(&list, pred);
//	    hPrintf("%s\n", pred->name);
	    ++outCount;
	    }
	}
    freez(&filter);
    }

genePredReaderFree(&reader);

if (list != NULL)
    {
    slReverse(&list);

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
	mafGeneOutPred(stdout, pred, database, mafTable, includeList, options);
    }

/* Do some error diagnostics for user. */
if (outCount == 0)
    explainWhyNoResults(NULL);
hashFree(&idHash);
}



static char *onChangeEnd(struct dyString **pDy)
/* Finish up javascript onChange command. */
{
dyStringAppend(*pDy, "document.hiddenForm.submit();\"");
return dyStringCannibalize(pDy);
}

static struct dyString *onChangeStart()
/* Start up a javascript onChange command */
{
struct dyString *dy = dyStringNew(1024);
dyStringAppend(dy, "onChange=\"");
jsDropDownCarryOver(dy, hgtaCGIGeneMafTable);
jsTrackedVarCarryOver(dy, hgtaCGIGeneExons, hgtaJSGeneExons); 
jsTrackedVarCarryOver(dy, hgtaCGIGeneNoTrans, hgtaJSGeneNoTrans); 
jsTrackedVarCarryOver(dy, hgtaCGIGeneOutBlank, hgtaJSGeneOutBlank); 
return dy;
}

static char *onChangeGenome()
/* Get group-changing javascript. */
{
struct dyString *dy = onChangeStart();
return onChangeEnd(&dy);
}

static char * outMafTableDrop(struct cart *cart, struct sqlConnection *conn)
{
char *mafTable = cartOptionalString(cart, hgtaCGIGeneMafTable);
char query[512];
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

printf("<B>MAF table: </B>\n");
//cgiMakeDropList(hgtaCGIGeneMafTable, mafTrackExist, count2,  checked);
cgiMakeDropListFull(hgtaCGIGeneMafTable, mafTrackExist, mafTrackExist, count2,  checked, onChangeGenome());

return mafTable;
}

static void palOptions(struct trackDb *track, char *type, 
	struct sqlConnection *conn)
/* Put up sequence type options for gene prediction tracks. */
{
htmlOpen("Select multiple alignment and options for %s", track->shortLabel);

hPrintf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=GET>\n", getScriptName());
cartSaveSession(cart);

char *mafTable = outMafTableDrop(cart, conn);

/*
boolean inExons = cartUsualBoolean(cart, "mafGeneExons", TRUE); 
boolean noTrans = cartUsualBoolean(cart, "mafGeneNoTrans", FALSE); 
boolean outBlank = cartUsualBoolean(cart, "mafGeneOutBlank", FALSE); 
*/

printf("<BR><BR><B>Formatting options:</B><BR>\n");
jsMakeTrackingCheckBox(cart, hgtaCGIGeneExons, hgtaJSGeneExons, FALSE);
printf("Separate into exons<BR>");
jsMakeTrackingCheckBox(cart, hgtaCGIGeneNoTrans, hgtaJSGeneNoTrans, FALSE);
printf("Show nucelotides<BR>");
jsMakeTrackingCheckBox(cart, hgtaCGIGeneOutBlank, hgtaJSGeneOutBlank, FALSE);
printf("Output lines with just dashes<BR>");


printf("<BR>");
struct trackDb *maftdb = hTrackDbForTrack(database, mafTable);
wigMafSpeciesTable(cart, maftdb, mafTable, database);

//getSpecies(mafTable, TRUE);
cgiMakeButton(hgtaDoPalOut, "submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "cancel");
hPrintf("<input type=\"hidden\" name=\"%s\" value=\"\">\n", hgtaDoPal);
hPrintf("</FORM>\n");
/* Hidden form - for benefit of javascript. */
    {
    static char *saveVars[32];
    int varCount = ArraySize(curVars);
    assert(varCount + 1 < (sizeof saveVars / sizeof(char *)));
    memcpy(saveVars, curVars, varCount * sizeof(saveVars[0]));
    saveVars[varCount] = hgtaDoPal;
    jsCreateHiddenForm(cart, getScriptName(), saveVars, varCount+1);
    }

cartSaveSession(cart);
cartSetString(cart, hgtaDoPal, "on");
htmlClose();
}

void doOutPalOptions(struct sqlConnection *conn)
/* Output fasta page. */
{
palOptions(curTrack, curTrack->type, conn);
}
