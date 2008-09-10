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
#include "genePredReader.h"

static char const rcsid[] = "$Id: hgPal.c,v 1.6 2008/09/10 18:41:25 braney Exp $";

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
struct sqlConnection *conn = hAllocConn(db);
struct genePred *list = NULL;
struct genePred *gene;
//char query[1024];
//struct sqlResult *sr = NULL;
//char **row;
char splitTable[HDB_MAX_TABLE_STRING];
boolean hasBin;
struct genePredReader *reader;

boolean found =  hFindSplitTable(db, NULL, geneTable,
	splitTable, &hasBin);

if (!found)
    errAbort("can't find table %s\n", geneTable);

char extra[2048];
if (onlyChrom != NULL)
    safef(extra, sizeof extra, "name='%s' and chrom='%s'", geneName, onlyChrom);
else
    safef(extra, sizeof extra, "name='%s'", geneName);

reader = genePredReaderQuery( conn, splitTable, extra);

//sr = sqlGetResult(conn, query);

//while ((row = sqlNextRow(sr)) != NULL)
while ((gene  = genePredReaderNext(reader)) != NULL)
    {
    //if (gene->exonFrames == NULL)
	//genePredAddExonFrames(gene);

    verbose(2, "got gene %s\n",gene->name);
    slAddHead(&list, gene);
    }

if (list == NULL)
    errAbort("no genePred for gene %s in %s\n",geneName, geneTable);

slReverse(&list);

genePredReaderFree(&reader);
//sqlFreeResult(&sr);
hFreeConn(&conn);

return list;
}

void showMafTracks(struct cart *cart, char *mafTable, char *geneTable, char *gene)
{
char query[512];
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
safef(query, sizeof query, "select tableName from trackDb where type like 'wigMaf%%'");
char *checked = NULL;
struct slName *speciesList = NULL;
int ii;

slAddHead(&speciesList, slNameNew(database));

printf("<B>MAF table:</B> \n");
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

char *onChangeDB = "onchange=\" document.mainForm.submit();\"";
cgiMakeDropListFull("mafTable", mafTrackExist, mafTrackExist, count2,  checked,onChangeDB);
printf("<BR><BR>");

struct trackDb *tdb = hTrackDbForTrack(database, mafTable);
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


struct slName *includeList = newSlName(database);
struct wigMafSpecies *wmSpeciesList = wigMafSpeciesTable(cart, tdb, tdb->tableName, database);
struct wigMafSpecies *wmSpecies = wmSpeciesList;

for(; wmSpecies; wmSpecies = wmSpecies->next)
    {
    if (wmSpecies->on)
	{
	struct slName *newName = slNameNew(wmSpecies->name);
	slAddHead(&includeList, newName);
	}
    }
slReverse(&includeList);


cgiMakeButton("Submit", "Submit");
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
hFreeConn(&conn);
}

void doMiddle(struct cart *cart)
/* Set up globals and make web page */
{

char *track = cartString(cart, "g");
char *item = cartOptionalString(cart, "i");
char *mafTable = cartOptionalString(cart, "mafTable");
//struct trackDb *tdb = NULL;
 //       tdb = hashFindVal(trackHash, track);

    //trackHash = makeTrackHashWithComposites(database, seqName, TRUE);
//printf("tdb is %p\n",tdb);
/*	database and organism are global variables used in many places	*/
getDbAndGenome(cart, &database, &genome, NULL);
organism = hOrganism(database);
scientificName = hScientificName(database);

//hDefaultConnect(); 	/* set up default connection settings */
//hSetDb(database);

boolean dbIsActive = hDbIsActive(database); 

if (dbIsActive)
    seqName = hgOfficialChromName(database, cartString(cart, "c"));
else 
    seqName = cartString(cart, "c");

winStart = cartIntExp(cart, "l");
winEnd = cartIntExp(cart, "r");

if (seqName == NULL)
    {
    if (winStart != 0 || winEnd != 0)
	webAbort("CGI variable error",
		 "hgPal: bad input variables c=%s l=%d r=%d",
		 cartString(cart, "c"), winStart, winEnd);
    else
	seqName = hDefaultChrom(database);
    }



cartWebStart(cart, database, "Protein Alignments for %s %s",track,item);
//printf("track is %s item %s\n", track,item);
#define MAIN_FORM "mainForm"
printf("<FORM ACTION=\"%s\" NAME=\""MAIN_FORM"\" METHOD=%s>\n\n",
       hgPalName(), cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);

showMafTracks(cart, mafTable, track, item);
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
