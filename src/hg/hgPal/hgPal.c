/* hgPal - a cgi that generates fasta protein alignments from multiple alignments and genePreds (or psls or bed12s maybe).. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "hdb.h"
#include "hgMaf.h"
#include "mafGene.h"
#include "hCommon.h"

static char const rcsid[] = "$Id: hgPal.c,v 1.2 2008/08/26 22:56:45 braney Exp $";

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;
char *database;
char *genome;
char *organism;
char *scientificName;
char *seqName;
char *onlyChrom;
#define MAX_SP_SIZE 2000

int winStart, winEnd;
struct hash *trackHash;	/* A hash of all tracks - trackDb valued */

#define STATIC_ARRAY_SIZE   1024
char *mafTrackNames[STATIC_ARRAY_SIZE];
char *mafTrackExist[STATIC_ARRAY_SIZE];
char *speciesNames[STATIC_ARRAY_SIZE];

/* load one or more genePreds from the database */
struct genePred *getPreds(char *geneName, char *geneTable, char *db)
{
struct sqlConnection *conn = hAllocConn();
struct genePred *list = NULL;
char query[1024];
struct sqlResult *sr = NULL;
char **row;
char splitTable[HDB_MAX_TABLE_STRING];
boolean hasBin;

boolean found =  hFindSplitTableDb(db, NULL, geneTable,
	splitTable, &hasBin);

if (!found)
    errAbort("can't find table %s\n", geneTable);

if (onlyChrom != NULL)
    safef(query, sizeof query, 
	    "select * from %s where name='%s' and chrom='%s' \n",
	    splitTable,  geneName, onlyChrom);
else
    safef(query, sizeof query, 
	    "select * from %s where name='%s' \n",
	    splitTable,  geneName);

sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct genePred *gene = genePredExtLoad(hasBin ? &row[1] : row,
	GENEPREDX_NUM_COLS );

    if (gene->exonFrames == NULL)
	genePredAddExonFrames(gene);

    verbose(2, "got gene %s\n",gene->name);
    slAddHead(&list, gene);
    }

if (list == NULL)
    errAbort("no genePred for gene %s in %s\n",geneName, geneTable);

slReverse(&list);

sqlFreeResult(&sr);
hFreeConn(&conn);

return list;
}

void showMafTracks(struct cart *cart, char *mafTable, char *geneTable, char *gene)
{
char option[MAX_SP_SIZE];
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
sprintf(query, "select tableName from trackDb where type like 'wigMaf%%'");
char *checked = NULL;
struct slName *speciesList = NULL;
int ii;

slAddHead(&speciesList, slNameNew(database));

printf("MAF table: \n");
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

cgiMakeDropList("mafTrack", mafTrackExist, count2,  checked);
printf("<BR>");

struct trackDb *tdb = hTrackDbForTrack(mafTable);
char *speciesOrder = trackDbSetting(tdb, SPECIES_ORDER_VAR);
char *speciesGroup = trackDbSetting(tdb, SPECIES_GROUP_VAR);
char *speciesUseFile = trackDbSetting(tdb, SPECIES_USE_FILE);
char *groups[20];
int speciesCt = 0, groupCt = 1;
int group;
char sGroup[24];
char *species[MAX_SP_SIZE];
boolean inExons = cartUsualBoolean(cart, "mafGeneExons", TRUE); 
boolean noTrans = cartUsualBoolean(cart, "mafGeneNoTrans", FALSE); 
boolean outBlank = cartUsualBoolean(cart, "mafGeneOutBlank", FALSE); 

printf("<B>Formatting options:</B><BR>\n");
cgiMakeCheckBox("mafGeneExons", inExons);
printf("Separate into exons<BR>");
cgiMakeCheckBox("mafGeneNoTrans", noTrans);
printf("Don't translate into amino acids<BR>");
cgiMakeCheckBox("mafGeneOutBlank", outBlank);
printf("Output lines with just dashes<BR>");


/* determine species and groups for pairwise -- create checkboxes */
if (speciesOrder == NULL && speciesGroup == NULL && speciesUseFile == NULL)
    {
//    if (isCustomTrack(tdb->tableName))
//	return;
    errAbort("Track %s missing required trackDb setting: speciesOrder, speciesGroups, or speciesUseFile", tdb->tableName);
    }

if (speciesGroup)
    groupCt = chopLine(speciesGroup, groups);

if (speciesUseFile)
    {
    if ((speciesGroup != NULL) || (speciesOrder != NULL))
	errAbort("Can't specify speciesUseFile and speciesGroup or speciesOrder");
    speciesOrder = cartGetOrderFromFile(cart, speciesUseFile);
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

printf("<B>Species to include:</B><BR>\n");
puts("\n<TABLE><TR>");
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
puts("</TR></TABLE><BR>");

if(includeList)
    slReverse(&includeList);

//for(sl = includeList; sl; sl = sl->next)
    //printf("include %s\n<BR>", sl->name);
cgiMakeButton("Submit", "Submit");
//slReverse(&wmSpeciesList);
struct genePred *pred = getPreds(gene, geneTable, database);

printf("<pre>");
unsigned options = 0;
if (inExons)
    options |= MAFGENE_EXONS;
if (noTrans)
    options |= MAFGENE_NOTRANS;
if (outBlank)
    options |= MAFGENE_OUTBLANK;

for(; pred; pred = pred->next)
    mafGeneOutPred(stdout, pred, database, mafTable, includeList, options);

for(ii=0; ii < count; ii++)
    freez(&mafTrackNames[count]);
hgFreeConn(&conn);
}

void doMiddle(struct cart *cart)
/* Set up globals and make web page */
{

char *track = cartString(cart, "g");
char *item = cartOptionalString(cart, "i");
char *mafTrack = cartOptionalString(cart, "mafTrack");
//struct trackDb *tdb = NULL;
 //       tdb = hashFindVal(trackHash, track);

    //trackHash = makeTrackHashWithComposites(database, seqName, TRUE);
//printf("tdb is %p\n",tdb);
/*	database and organism are global variables used in many places	*/
getDbAndGenome(cart, &database, &genome, NULL);
organism = hOrganism(database);
scientificName = hScientificName(database);

hDefaultConnect(); 	/* set up default connection settings */
hSetDb(database);

boolean dbIsActive = hDbIsActive(database); 

if (dbIsActive)
    seqName = hgOfficialChromName(cartString(cart, "c"));
else 
    seqName = cartString(cart, "c");

winStart = cartIntExp(cart, "l");
winEnd = cartIntExp(cart, "r");

if (seqName == NULL)
    {
    if (winStart != 0 || winEnd != 0)
	webAbort("CGI variable error",
		 "hgc: bad input variables c=%s l=%d r=%d",
		 cartString(cart, "c"), winStart, winEnd);
    else
	seqName = hDefaultChrom();
    }



cartWebStart(cart, "Protein Alignments for %s",item);
//printf("track is %s item %s\n", track,item);
#define MAIN_FORM "mainForm"
printf("<FORM ACTION=\"%s\" NAME=\""MAIN_FORM"\" METHOD=%s>\n\n",
       hgPalName(), cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);

showMafTracks(cart, mafTrack, track, item);
cartHtmlEnd();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
