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

void doGenePredPal(struct sqlConnection *conn)
/* Output genePred protein alignment. */
{
cartRemove(cart, hgtaDoPal);
cartRemove(cart, hgtaDoPalOut);
cartSaveSession(cart);

struct lm *lm = lmInit(64*1024);
int fieldCount;
struct bed *bed, *bedList = cookedBedsOnRegions(conn, curTable, getRegions(),
	lm, &fieldCount);

/* Make hash of all id's passing filters. */
struct hash *hash = newHash(18);
for (bed = bedList; bed != NULL; bed = bed->next)
    hashAdd(hash, bed->name, NULL);

//lmCleanup(&lm);

struct genePredReader *reader = genePredReaderQuery( conn, curTable, NULL);
int outCount = 0;
struct genePred *list = NULL, *pred;
while ((pred  = genePredReaderNext(reader)) != NULL)
    {
    if (hashLookup(hash, pred->name))
	{
	slAddHead(&list, pred);
	++outCount;
	}
    }
genePredReaderFree(&reader);

textOpen();

if (list != NULL)
    {
    slReverse(&list);

    struct genePred *pred;
    unsigned options = 0;
    char *mafTable = cartString(cart, hgtaCGIGeneMafTable); 
    boolean inExons = cartUsualBoolean(cart, hgtaCGIGeneExons , TRUE); 
    boolean noTrans = cartUsualBoolean(cart, hgtaCGIGeneNoTrans, FALSE); 
    boolean outBlank = cartUsualBoolean(cart, hgtaCGIGeneOutBlank, FALSE); 

    if (inExons)  options |= MAFGENE_EXONS;
    if (noTrans)  options |= MAFGENE_NOTRANS;
    if (outBlank) options |= MAFGENE_OUTBLANK;

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
hashFree(&hash);
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
