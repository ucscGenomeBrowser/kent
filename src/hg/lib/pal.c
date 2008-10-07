#include "common.h"
#include "hash.h"
#include "cart.h"
#include "genePred.h"
#include "genePredReader.h"
#include "mafGene.h"
#include "trackDb.h"
#include "hdb.h"
#include "hgMaf.h"
#include "jsHelper.h"
#include "hPrint.h"
#include "hdb.h"

static char const rcsid[] = "$Id: pal.c,v 1.11 2008/10/07 19:25:19 braney Exp $";

#define hgtaCGIGeneMafTable "hgta_mafGeneMafTable" 
#define hgtaJSGeneMafTable  "mafGeneMafTable" 
#define hgtaCGIGeneExons "hgta_mafGeneExons" 
#define hgtaJSGeneExons  "mafGeneExons" 
#define hgtaCGIGeneNoTrans "hgta_mafGeneNoTrans" 
#define hgtaJSGeneNoTrans  "mafGeneNoTrans" 
#define hgtaCGIGeneOutBlank "hgta_mafGeneOutBlank" 
#define hgtaJSGeneOutBlank  "mafGeneOutBlank" 
#define hgtaCGIOutTable "hgta_mafOutTable" 
#define hgtaJSOutTable  "mafOutTable" 
#define hgtaCGINumColumns "hgta_mafNumColumns"
#define hgtaJSNumColumns  "mafNumColumns"
#define hgtaCGITruncHeader "hgta_mafTruncHeader"
#define hgtaJSTruncHeader  "mafTruncHeader"

static char *curVars[] = {
	hgtaCGIGeneMafTable, hgtaCGIGeneExons,
	hgtaCGIGeneNoTrans, hgtaCGIGeneOutBlank,
	hgtaCGIOutTable, hgtaCGINumColumns,
	hgtaCGITruncHeader,
	};

int palOutPredList(struct sqlConnection *conn, struct cart *cart,
    struct genePred *list)
/* output a list of genePreds in pal format */
{
if (list == NULL)
    return 0;

char *mafTable = cartString(cart, hgtaCGIGeneMafTable); 
char *database = sqlGetDatabase(conn);
struct trackDb *maftdb = hTrackDbForTrack(database, mafTable);
struct wigMafSpecies *wmSpecies;
int groupCnt;

/* this queries the state of the getSpecies dialog */
wigMafGetSpecies(cart, maftdb, database, &wmSpecies, &groupCnt);

/* since the species selection dialog doesn't list
 * the reference species, we just automatically include
 * it */
struct slName *includeList = slNameNew(database);

/* now make a list of all species that are on */
for(; wmSpecies; wmSpecies = wmSpecies->next)
    {
    if (wmSpecies->on)
	{
	struct slName *newName = slNameNew(wmSpecies->name);
	slAddHead(&includeList, newName);
	}
    }
slReverse(&includeList);

boolean inExons = cartUsualBoolean(cart, hgtaCGIGeneExons , FALSE); 
boolean noTrans = cartUsualBoolean(cart, hgtaCGIGeneNoTrans, FALSE); 
boolean outBlank = cartUsualBoolean(cart, hgtaCGIGeneOutBlank, FALSE); 
boolean outTable = cartUsualBoolean(cart, hgtaCGIOutTable, FALSE); 
boolean truncHeader = cartUsualBoolean(cart, hgtaCGITruncHeader, FALSE); 
int numCols = cartUsualInt(cart, hgtaCGINumColumns, 20);
unsigned options = 0;

if (inExons)  options |= MAFGENE_EXONS;
if (noTrans)  options |= MAFGENE_NOTRANS;
if (outBlank) options |= MAFGENE_OUTBLANK;
if (outTable) options |= MAFGENE_OUTTABLE;

if (!truncHeader)
    numCols = -1;

/* send out the alignments */
int outCount = 0;
for( ; list ; list = list->next)
    {
    if (list->cdsStart != list->cdsEnd)
	{
	outCount++;
	mafGeneOutPred(stdout, list, database, mafTable, 
	    includeList, options, numCols);
	}
    }

slNameFreeList(&includeList);
return outCount;
}

int palOutPredsInBeds(struct sqlConnection *conn, struct cart *cart,
    struct bed *beds, char *table )
/* output the alignments whose names and coords match a bed*/
{
struct genePred *list = NULL;

for(; beds; beds = beds->next)
    {
    char where[10 * 1024];

    safef(where, sizeof where, 
	"name = '%s' and chrom='%s' and txEnd > %d and txStart <= %d",
	beds->name, beds->chrom, beds->chromStart, beds->chromEnd);

    struct genePredReader *reader = genePredReaderQuery( conn, table, where);
    struct genePred *pred;
    while ((pred = genePredReaderNext(reader)) != NULL)
	slAddHead(&list, pred);

    genePredReaderFree(&reader);
    }

int outCount = 0;
if (list != NULL)
    {
    slReverse(&list);
    outCount = palOutPredList( conn, cart, list);
    genePredFreeList(&list);
    }

return outCount;
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
jsTrackedVarCarryOver(dy, hgtaCGIOutTable, hgtaJSOutTable); 
jsTrackedVarCarryOver(dy, hgtaCGITruncHeader, hgtaJSTruncHeader); 
jsTextCarryOver(dy, hgtaCGINumColumns);
return dy;
}

static char *onChangeGenome()
/* Get group-changing javascript. */
{
struct dyString *dy = onChangeStart();
return onChangeEnd(&dy);
}

static char * getConservationTrackName( struct sqlConnection *conn)
{
struct slName *dbList = hTrackDbList();
struct slName *dbl = dbList;
char *ret = NULL;

for(; dbl; dbl = dbl->next)
    {
    char query[512];
    safef(query, sizeof query, 
	"select tableName from %s where shortLabel='Conservation'", dbl->name);

    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;
    struct slName *tableList = NULL;
    while ((row = sqlNextRow(sr)) != NULL)
	{
	struct slName *name = newSlName(row[0]);
	slAddHead(&tableList, name);
	}
    sqlFreeResult(&sr);

    struct slName *l = tableList;

    for(; l; l = l->next)
	if (sqlTableExists(conn, l->name))
	    ret = cloneString(l->name);

    slFreeList(&tableList);

    if (ret != NULL)
	break;
    }
slFreeList(&dbList);

return ret;
}

static char * outMafTableDrop(struct cart *cart, struct sqlConnection *conn)
{
struct slName *list = hTrackTablesOfType(conn, "wigMaf%%");
int count = slCount(list);

if (count == 0)
    errAbort("There are no multiple alignments available for this genome.");

char **tables = needMem(sizeof(char *) * count);
char **tb = tables;
char *mafTable = cartOptionalString(cart, hgtaCGIGeneMafTable);

if (mafTable != NULL)
    {
    struct slName *l = list;
    for(; l; l=l->next)
	if (sameString(l->name, mafTable))
	    break;

    /* didn't find mafTable in list, reset it */
    if (l == NULL)
	mafTable = NULL;
    }

if (mafTable == NULL)
    {
    if ((mafTable = getConservationTrackName(conn)) == NULL)
	mafTable = list->name;

    cartSetString(cart, hgtaCGIGeneMafTable, mafTable);
    }

for(; list; list = list->next)
    *tb++ = list->name;

printf("<B>MAF table: </B>\n");
cgiMakeDropListFull(hgtaCGIGeneMafTable, tables, tables, 
    count , mafTable, onChangeGenome());

return mafTable;
}

void palOptions(struct cart *cart, 
	struct sqlConnection *conn, void (*addButtons)(), 
	char *extraVar)
/* output the options dialog (select MAF file, output options */
{
char *database = sqlGetDatabase(conn);

hPrintf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=GET>\n", cgiScriptName());
cartSaveSession(cart);

char *mafTable = outMafTableDrop(cart, conn);
char *numColumns = cartUsualString(cart, hgtaCGINumColumns, "");

printf("<BR><BR><B>Formatting options:</B><BR>\n");
jsMakeTrackingCheckBox(cart, hgtaCGIGeneExons, hgtaJSGeneExons, FALSE);
printf("Separate into exons<BR>");
jsMakeTrackingCheckBox(cart, hgtaCGIGeneNoTrans, hgtaJSGeneNoTrans, FALSE);
printf("Show nucelotides<BR>");
jsMakeTrackingCheckBox(cart, hgtaCGIGeneOutBlank, hgtaJSGeneOutBlank, FALSE);
printf("Output lines with just dashes<BR>");
jsMakeTrackingCheckBox(cart, hgtaCGIOutTable, hgtaJSOutTable, FALSE);
printf("Format output as table ");
jsMakeTrackingCheckBox(cart, hgtaCGITruncHeader, hgtaJSTruncHeader, FALSE);
printf("Truncate headers at ");
cgiMakeTextVar(hgtaCGINumColumns, numColumns, 2);
printf("characters (enter zero for no headers)<BR>");

printf("<BR>");
struct trackDb *maftdb = hTrackDbForTrack(database, mafTable);
wigMafSpeciesTable(cart, maftdb, mafTable, database);

addButtons();

hPrintf("</FORM>\n");

/* Hidden form - for benefit of javascript. */
    {
    static char *saveVars[32];
    int varCount = ArraySize(curVars);

    assert(varCount  < (sizeof saveVars / sizeof(char *)));
    memcpy(saveVars, curVars, varCount * sizeof(saveVars[0]));
    if (extraVar != NULL)
	{
	assert(varCount + 1 < (sizeof saveVars / sizeof(char *)));
	saveVars[varCount] = extraVar;
	varCount++;
	}
    jsCreateHiddenForm(cart, cgiScriptName(), saveVars, varCount);
    }

cartSaveSession(cart);
}
