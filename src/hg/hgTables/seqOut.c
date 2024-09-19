/* seqOut - Output sequence. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

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
#include "genbank.h"


static char *genePredMenu[] =
    {
    "genomic",
    "protein",
    "mRNA",
    };

static void genePredTypeButton(char *val, char *selVal)
/* Make region radio button including a little Javascript
 * to save selection state. */
{
cgiMakeRadioButton(hgtaGeneSeqType, val, sameString(val, selVal));
}

static boolean isRefGeneTrack(char *table)
/* Return TRUE if it's a refGene track. */
{
return sameString("refGene", table) || sameString("xenoRefGene", table);
}

static void genePredOptions(struct trackDb *track, char *type,
	struct sqlConnection *conn)
/* Put up sequence type options for gene prediction tracks. */
{
char *predType = cartUsualString(cart, hgtaGeneSeqType, genePredMenu[0]);
char *dupType = cloneString(type);
char *typeWords[3];
int typeWordCount, typeIx;

/* Type field has 1-3 words which are in order:
 *     genePred pepTable mrnaTable */
typeWordCount = chopLine(dupType, typeWords);
/* TypeIx will be 0 (genomic) 1 (protein) 2(mrna). */
typeIx = stringArrayIx(predType, genePredMenu, typeWordCount);
if (typeIx < 0)
    predType = genePredMenu[0];
htmlOpen("Select sequence type for %s", track->shortLabel);
hPrintf("<FORM ACTION=\"%s\" METHOD=GET>\n", getScriptName());
cartSaveSession(cart);

if (isRefGeneTrack(track->table))
    {
    /* RefGene covers all 3 types, but in it's own way. */
    for (typeIx = 0; typeIx < 3; ++typeIx)
	{
	genePredTypeButton(genePredMenu[typeIx], predType);
	hPrintf(" %s<BR>\n", genePredMenu[typeIx]);
	}
    }
else
    {
    /* Otherwise we always have genomic, and we have
     * peptide/mrna only if there are corresponding table
     * in the type field. */
    for (typeIx = 0; typeIx < typeWordCount; ++typeIx)
	{
	if (typeIx == 0 || sqlTableExists(conn, typeWords[typeIx]))
	    {
	    genePredTypeButton(genePredMenu[typeIx], predType);
	    hPrintf(" %s<BR>\n", genePredMenu[typeIx]);
	    }
	}
    }
cgiMakeButton(hgtaDoGenePredSequence, "submit");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "cancel");
hPrintf("</FORM>\n");
cgiDown(0.9);
htmlClose();
freez(&dupType);
}


void doRefGeneProteinSequence(struct sqlConnection *conn, struct bed *bedList)
/* Fetch refGene proteins corresponding to names in bedList. */
{
struct hash *uniqHash = newHash(18);
struct hash *protHash = newHash(18);
struct sqlResult *sr;
char **row;
struct bed *bed;


/* Get translation from mRNA to protein from refLink table. */
char query[2048];
sqlSafef(query, sizeof query, "select mrnaAcc,protAcc from %s",refLinkTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *protAcc = row[1];
    if (protAcc != NULL && protAcc[0] != 0)
        hashAdd(protHash, row[0], lmCloneString(protHash->lm, protAcc));
    }
sqlFreeResult(&sr);

boolean gotResults = FALSE;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    char *protAcc = hashFindVal(protHash, bed->name);
    if (protAcc != NULL && !hashLookup(uniqHash, protAcc))
        {
	char *fa = hGetSeqAndId(conn, protAcc, NULL);
	hashAdd(uniqHash, protAcc, NULL);
	if (fa != NULL)
	    hPrintf("%s", fa);
	freez(&fa);
	gotResults = TRUE;
	}
    }
if (!gotResults)
    explainWhyNoResults(stdout);

hashFree(&protHash);
hashFree(&uniqHash);
}

void doRefGeneMrnaSequence(struct sqlConnection *conn, struct bed *bedList)
/* Fetch refGene mRNA sequence. */
{
struct hash *uniqHash = newHash(18);
struct bed *bed;
boolean gotResults = FALSE;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (!hashLookup(uniqHash, bed->name))
        {
	char *fa = hGetSeqAndId(conn, bed->name, NULL);
	hashAdd(uniqHash, bed->name, NULL);
	if (fa != NULL)
	    hPrintf("%s", fa);
	freez(&fa);
	gotResults = TRUE;
	}
    }
if (!gotResults)
    explainWhyNoResults(stdout);
hashFree(&uniqHash);
}

void doGenePredNongenomic(struct sqlConnection *conn, int typeIx)
/* Get mrna or protein associated with selected genes. */
{
/* Note this does do the whole genome at once rather than one
 * chromosome at a time, but that's ok because the gene prediction
 * tracks this serves are on the small side. */
char *typeWords[3];
char *table;
struct lm *lm = lmInit(64*1024);
int fieldCount;
struct bed *bed, *bedList = cookedBedsOnRegions(conn, curTable, getRegions(),
	lm, &fieldCount);
int typeWordCount;

textOpen();

/* Figure out which table to use. */
if (isRefGeneTrack(curTable))
    {
    if (typeIx == 1) /* Protein */
        doRefGeneProteinSequence(conn, bedList);
    else
        doRefGeneMrnaSequence(conn, bedList);
    }
else
    {
    char *dupType = cloneString(findTypeForTable(database, curTrack, curTable, ctLookupName));
    typeWordCount = chopLine(dupType, typeWords);
    if (typeIx >= typeWordCount)
	internalErr();
    table = typeWords[typeIx];
    if (sqlTableExists(conn, table))
	{
	struct sqlResult *sr;
	char **row;
	char query[256];
	struct hash *hash = newHash(18);
	boolean gotResults = FALSE;

	/* Make hash of all id's passing filters. */
	for (bed = bedList; bed != NULL; bed = bed->next)
	    hashAdd(hash, bed->name, NULL);

	/* Scan through table, outputting ones that match. */
	sqlSafef(query, sizeof(query), "select name, seq from %s", table);
	sr = sqlGetResult(conn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    if (hashLookup(hash, row[0]))
		{
		hPrintf(">%s\n", row[0]);
		writeSeqWithBreaks(stdout, row[1], strlen(row[1]), 60);
		gotResults = TRUE;
		}
	    }
	sqlFreeResult(&sr);
	hashFree(&hash);
	if (!gotResults)
            explainWhyNoResults(stdout);
	}
    else
	{
	internalErr();
	}
    freez(&dupType);
    }
lmCleanup(&lm);
}


void genomicFormatPage(struct sqlConnection *conn)
/* Put up page asking for what sort of genomic sequence. */
{
struct hTableInfo *hti = getHti(database, curTable, conn);
htmlOpen("%s Genomic Sequence", curTableLabel());
if (doGalaxy())
    startGalaxyForm();
else
    hPrintf("<FORM ACTION=\"%s\" METHOD=GET>\n", getScriptName());
cartSaveSession(cart);
hgSeqOptionsHtiCart(hti, cart);
hPrintf("<BR>\n");
if (doGalaxy())
    {
    /* pass parameter to get sequence to Galaxy */
    cgiMakeHiddenVar(hgtaDoGenomicDna, "get sequence");
    printGalaxySubmitButtons();
    }
else
    {
    cgiMakeButton(hgtaDoGenomicDna, "get sequence");
    hPrintf(" ");
    cgiMakeButton(hgtaDoMainPage, "cancel");
    hPrintf("</FORM>");
    }
cgiDown(0.9);
htmlClose();
}

void doGenomicDna(struct sqlConnection *conn)
/* Get genomic sequence (UI has already told us how). */
{
struct region *region, *regionList = getRegions();
struct hTableInfo *hti = getHti(database, curTable, conn);
int fieldCount;
textOpen();
int resultCount = 0;
for (region = regionList; region != NULL; region = region->next)
    {
    struct lm *lm = lmInit(64*1024);
    struct bed *bedList = cookedBedList(conn, curTable, region, lm, &fieldCount);
    if (bedList != NULL)
    	resultCount += hgSeqBed(database, hti, bedList);
    lmCleanup(&lm);
    }
if (!resultCount)
    explainWhyNoResults(stdout);
}

void doGenePredSequence(struct sqlConnection *conn)
/* Output genePred sequence. */
{
char *type = cartString(cart, hgtaGeneSeqType);

if (sameWord(type, "protein"))
    {
    if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
        sendParamsToGalaxy(hgtaDoGenePredSequence, "submit");
    else
        doGenePredNongenomic(conn, 1);
    }
else if (sameWord(type, "mRNA"))
    {
    if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
        sendParamsToGalaxy(hgtaDoGenePredSequence, "submit");
    else
        doGenePredNongenomic(conn, 2);
    }
else
    genomicFormatPage(conn);
}

void doOutSequence(struct sqlConnection *conn)
/* Output sequence page. */
{
struct trackDb *tdb = findTdbForTable(database, curTrack, curTable, ctLookupName);
if (tdb != NULL && (startsWith("genePred", tdb->type) || startsWith("bigGenePred", tdb->type)))
    genePredOptions(tdb, curTrack->type, conn);
else
    genomicFormatPage(conn);
}
