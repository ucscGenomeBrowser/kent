/* Upload - put up upload pages and sub-pages. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "ra.h"
#include "portable.h"
#include "cheapcgi.h"
#include "localmem.h"
#include "cart.h"
#include "web.h"
#include "chromInfo.h"
#include "chromGraph.h"
#include "errCatch.h"
#include "hgGenome.h"

/* Symbolic defines for types of markers we support. */
#define hggUpGenomic "chromosome base"
#define hggUpSts "STS marker"
#define hggUpSnp "dbSNP rsID"
#define hggUpAffy100 "(Affymetrix 100K Gene Chip)"
#define hggUpAffy500 "(Affymetrix 500k Gene Chip)"
#define hggUpHumanHap300 "(Illumina HumanHap300 BeadChip)"

static char *locNames[] = {
    hggUpGenomic,
    hggUpSts,
    hggUpSnp,
    hggUpAffy100,
    hggUpAffy500,
    hggUpHumanHap300,
    };

void uploadPage()
/* Put up initial upload page. */
{
char *oldFileName = cartUsualString(cart, hggUploadFile "__filename", "");
cartWebStart(cart, "Upload Data to Genome Association View");
hPrintf("<FORM ACTION=\"../cgi-bin/hgGenome\" METHOD=\"POST\" ENCTYPE=\"multipart/form-data\">");
cartSaveSession(cart);
hPrintf("Name of data set: ");
cartMakeTextVar(cart, hggDataSetName, "", 16);
hPrintf(" Locations are: ");
cgiMakeDropList(hggLocType, locNames, 
	ArraySize(locNames), cartUsualString(cart, hggLocType, locNames[0]));
hPrintf("<BR>");

hPrintf("Description: ");
cartMakeTextVar(cart, hggDataSetDescription, "", 64);
hPrintf("<BR>");

hPrintf("Display Min Value: ");
cartMakeTextVar(cart, hggMinVal, "", 5);
hPrintf(" Max Value: ");
cartMakeTextVar(cart, hggMaxVal, "", 5);
hPrintf(" <i>Leave these blank to show all data</i><BR>");

hPrintf("Label Values: ");
cartMakeTextVar(cart, hggLabelVals, "", 32);
hPrintf(" <i>Comma-separated numbers for axis label</i><BR>");
hPrintf("Draw lines between markers separated by up to ");
cartMakeIntVar(cart, hggMaxGapToFill, 3000000, 8);
hPrintf(" bases.<BR>");

hPrintf("File name: <INPUT TYPE=FILE NAME=\"%s\" VALUE=\"%s\">", hggUploadFile,
	oldFileName);
cgiMakeButton(hggSubmitUpload, "Submit");
hPrintf("</FORM>\n");
hPrintf("<i>note: If you are uploading more than one data set please give them ");
hPrintf("different names.  Only the most recent data set of a given name is ");
hPrintf("kept.  Data sets will be kept for at least 8 hours.  After that time ");
hPrintf("you may have to upload them again.</i>");

/* Put up section that describes file formats. */
webNewSection("Upload file formats");
hPrintf("%s", 
"The input data files contain one line for each marker. "
"Each line starts with information on the marker, and ends with "
"a numerical value associated with that marker. The exact format "
"of the line depends on what is selected from the locations drop "
"down menu.  If this is <i>chromosome base</i> then the line will "
"contain three tab or space-separated fields:  chromosome, position, "
"and value.  The first base in a chromosome is considered position 0. "
"An example <i>chromosome base</i> type line is is:<PRE><TT>\n"
"chrX 100000 1.23\n"
"</TT></PRE>The lines for other location type contain two fields: "
"marker and value.  For dbSNP rsID's an example is:<PRE><TT>\n"
"rs10218492 0.384\n"
);

cartWebEnd();
}

struct hash *chromInfoHash(struct sqlConnection *conn)
/* Build up hash of chromInfo keyed by name */
{
struct sqlResult *sr;
char **row;
struct hash *hash = hashNew(0);
sr = sqlGetResult(conn, "select * from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct chromInfo *ci = chromInfoLoad(row);
    hashAdd(hash, ci->chrom, ci);
    }
sqlFreeResult(&sr);
return hash;
}

struct chromPos
/* Just chromosome and position */
    {
    char *chrom;	/* Not allocated here */
    int pos;
    };

struct hash *tableToChromPosHash(struct sqlConnection *conn, char *table, 
	char *query)
/* Create hash of chromPos keyed by name field. */ 
{
char buf[256];
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(23);
struct lm *lm = hash->lm;
struct chromPos *pos;
safef(buf, sizeof(buf), query, table);
sr = sqlGetResult(conn, buf);
while ((row = sqlNextRow(sr)) != NULL)
    {
    lmAllocVar(lm, pos);
    pos->chrom = lmCloneString(lm, row[0]);
    pos->pos = sqlUnsigned(row[1]);
    hashAdd(hash, row[2], pos);
    }
sqlFreeResult(&sr);
return hash;
}

struct hash *tableToAliasHash(struct sqlConnection *conn, char *table,
	char *query)
/* Create hash of true name keyed by alias */
{
struct sqlResult *sr;
char **row;
struct hash *hash = hashNew(19);
char buf[256];
safef(buf, sizeof(buf), query, table);
sr = sqlGetResult(conn, buf);
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(hash, row[0], lmCloneString(hash->lm, row[1]));
sqlFreeResult(&sr);
return hash;
}

void processDb(struct sqlConnection *conn,
	struct lineFile *lf, char *outFileName, 
	char *table, char *query, char *aliasTable, char *aliasQuery)
/* Process two column input file into chromGraph.  Treat first
 * column as a name to look up in bed-format table, which should
 * not be split. Return TRUE on success. */
{
struct hash *hash = tableToChromPosHash(conn, table, query);
struct hash *aliasHash = NULL;
char *row[2];
int match = 0, total = 0;
struct chromGraph *list = NULL, *cg;
struct chromPos *pos;
hPrintf("Loaded %d elements from %s table for mapping.<BR>", hash->elCount,
	table);
if (aliasTable != NULL)
    {
    aliasHash = tableToAliasHash(conn, aliasTable, aliasQuery);
    hPrintf("Loaded %d aliases from %s table as well.<BR>", aliasHash->elCount,
    	aliasTable);
    }
while (lineFileRow(lf, row))
    {
    char *name = row[0];
    ++total;
    pos = hashFindVal(hash, name);
    if (pos == NULL && aliasHash != NULL)
        {
	name = hashFindVal(aliasHash, name);
	if (name != NULL)
	    pos = hashFindVal(hash, name);
	}
    if (pos != NULL)
        {
	++match;
	AllocVar(cg);
	cg->chrom = pos->chrom;
	cg->chromStart = pos->pos;
	cg->val = atof(row[1]);
	slAddHead(&list, cg);
	}
    }
hPrintf("Mapped %d of %d (%3.1f%%) of markers<BR>", match, total, 
	100.0*match/total);
slSort(&list, chromGraphCmp);
chromGraphToBin(list, outFileName);
#ifdef SOON
#endif /* SOON */
}

void  processGenomic(struct sqlConnection *conn, struct lineFile *lf, 
	char *outFileName)
/* Process three column file into chromGraph.  Abort if
 * there's a problem. */
{
char *row[3];
struct chromGraph *list = NULL, *cg;
struct hash *chromHash = chromInfoHash(conn);
struct chromInfo *ci;

while (lineFileRow(lf, row))
    {
    cg = chromGraphLoad(row);
    ci = hashFindVal(chromHash, cg->chrom);
    if (ci == NULL)
        errAbort("Error line %d of %s. "
	         "Chromosome %s not found in this assembly (%s).", 
		 lf->lineIx, lf->fileName, cg->chrom, database);
    if (cg->chromStart < 0 || cg->chromStart >= ci->size)
        errAbort("Error line %d of %s. "
	         "Chromosome %s is %d bases long, but got coordinate %u",
		 lf->lineIx, lf->fileName, ci->chrom, ci->size, cg->chromStart);
    slAddHead(&list, cg);
    }
slSort(&list, chromGraphCmp);
chromGraphToBin(list, outFileName);
hPrintf("Read in %d markers and values in <i>chromosome base</i> format.<BR>", 
	slCount(list));
}

boolean errCatchFinish(struct errCatch **pErrCatch)
/* Finish up error catching.  Report error if there is a
 * problem and return FALSE.  If no problem return TRUE.
 * This handles errCatchEnd and errCatchFree. */
{
struct errCatch *errCatch = *pErrCatch;
boolean ok = TRUE;
if (errCatch != NULL)
    {
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	{
	ok = FALSE;
	warn(errCatch->message->string);
	}
    errCatchFree(pErrCatch);
    }
return ok;
}

boolean mayProcessGenomic(struct sqlConnection *conn, struct lineFile *lf, 
	char *outFileName)
/* Process three column file into chromGraph.  If there's a problem
 * print warning message and return FALSE. */
{
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
     processGenomic(conn, lf, outFileName);
return errCatchFinish(&errCatch);
}

boolean mayProcessDb(struct sqlConnection *conn,
	struct lineFile *lf, char *outFileName, 
	char *table, char *query, char *aliasTable, char *aliasQuery)
/* Process database table into chromGraph.  If there's a problem
 * print warning message and return FALSE. */
{
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
     processDb(conn, lf, outFileName, table, query, aliasTable, aliasQuery);
return errCatchFinish(&errCatch);
}

void raSaveNext(struct hash *ra, FILE *f)
/* Write hash to file */
{
struct hashEl *el, *list = hashElListHash(ra);
slSort(&list, hashElCmp);
for (el = list; el != NULL; el = el->next)
    fprintf(f, "%s\t%s\n", el->name, (char*)el->val);
fprintf(f, "\n");
hashElFreeList(&list);
}

void raSaveAll(struct hash *allHash, char *fileName)
/* Save all elements of allHas to ra file */
{
FILE *f = mustOpen(fileName, "w");
struct hashEl *el, *list = hashElListHash(allHash);
slSort(&list, hashElCmp);
for (el = list; el != NULL; el = el->next)
    raSaveNext(el->val, f);
hashElFreeList(&list);
carefulClose(&f);
}

static void addIfNonempty(struct hash *ra, char *cgiVar, char *raVar)
/* If cgiVar exists and is non-empty, add it to ra. */
{
char *val = skipLeadingSpaces(cartUsualString(cart, cgiVar, ""));
if (val[0] != 0)
    hashAdd(ra, raVar, val);
}

void updateUploadRa(char *binFileName)
/* Update upload ra file with current upload data */
{
char *fileName = cartOptionalString(cart, hggUploadRa);
struct tempName tempName;
struct hash *allRaHash, *ra;
char *graphName = skipLeadingSpaces(cartUsualString(cart, hggDataSetName, ""));

if (graphName[0] == 0)
    graphName = "user data";

/* Read in old ra file if possible, otherwise just dummy up an
 * empty hash */
if (fileName == NULL || !fileExists(fileName))
    {
    makeTempName(&tempName, "hggUp", ".ra");
    fileName = tempName.forCgi;
    allRaHash = hashNew(0);
    cartSetString(cart, hggUploadRa, fileName);
    }
else
    {
    allRaHash = raReadAll(fileName, "name");
    }

/* Get rid of old ra record of same name if any */
if (hashLookup(allRaHash, graphName))
    hashRemove(allRaHash, graphName);

/* Create ra hash with our info in it. */
ra = hashNew(8);
hashAdd(ra, "name", graphName);
hashAdd(ra, "description", 
	cartUsualString(cart, hggDataSetDescription, graphName));
hashAdd(ra, "locType",
	cartUsualString(cart, hggLocType, locNames[0]));
hashAdd(ra, "binaryFile", binFileName);
addIfNonempty(ra, hggMinVal, "minVal");
addIfNonempty(ra, hggMaxVal, "minVal");
addIfNonempty(ra, hggMaxGapToFill, "maxGapToFill");
addIfNonempty(ra, hggLabelVals, "linesAt");

/* Update allRaHash and save */
hashAdd(allRaHash, graphName, ra);
raSaveAll(allRaHash, fileName);

hPrintf("Select \"%s\" from one of the drop down menus ", graphName);
hPrintf("in the main page to view this data.<BR>");
}

void processUpload(char *text, struct sqlConnection *conn)
/* Parse uploaded text.  If it looks good then make a 
 * binary chromGraph file out of it, and save information
 * about it in the upload ra file. */
{
char *type = cartUsualString(cart, hggLocType, hggUpGenomic);
struct tempName tempName;
char *binFile;
boolean ok = FALSE;
struct lineFile *lf = lineFileOnString("uploaded data", TRUE, text);
/* NB - do *not* lineFileClose this or a double free can happen. */

makeTempName(&tempName, "hggUp", ".cgb");
binFile = tempName.forCgi;
if (sameString(type, hggUpGenomic))
    ok = mayProcessGenomic(conn, lf, binFile);
else if (sameString(type, hggUpSts))
    ok = mayProcessDb(conn, lf, binFile, "stsMap",
    	"select chrom,round((chromStart+chromEnd)*0.5),name from %s",
	"stsAlias", "select alias,trueName from %s");
else if (sameString(type, hggUpSnp))
    {
    char *query = "select chrom,chromStart,name from %s";
    if (sqlTableExists(conn, "snp126"))
        ok = mayProcessDb(conn, lf, binFile, "snp126", query, NULL, NULL);
    else if (sqlTableExists(conn, "snp125"))
        ok = mayProcessDb(conn, lf, binFile, "snp125", query, NULL, NULL);
    else if (sqlTableExists(conn, "snp"))
        ok = mayProcessDb(conn, lf, binFile, "snp", query, NULL, NULL);
    else
        warn("Couldn't find SNP table");
    }
else if (sameString(type, hggUpAffy100))
    {
    warn("Support for Affy 100k chip coming soon.");
    }
else if (sameString(type, hggUpAffy500))
    {
    warn("Support for Affy 500k chip coming soon.");
    }
else if (sameString(type, hggUpHumanHap300))
    {
    warn("Support for Illumina HumanHap300 coming soon.");
    }
if (ok)
    updateUploadRa(binFile);
}

void submitUpload(struct sqlConnection *conn)
/* Called when they've submitted from uploads page */
{
char *rawText = cartUsualString(cart, hggUploadFile, NULL);
if (rawText == NULL || rawText[0] == 0)
    {
    cartWebStart(cart, "Upload error");
    hPrintf("Please go back, enter a file with something in it, and try again.");
    cartWebEnd();
    }
else
    {
    int rawTextSize = strlen(rawText);
    cartWebStart(cart, "Data Upload Complete (%d bytes)", rawTextSize);
    hPrintf("<FORM ACTION=\"../cgi-bin/hgGenome\">");
    cartSaveSession(cart);
    processUpload(rawText, conn);
    cartRemove(cart, hggUploadFile);
    hPrintf("<CENTER>");
    cgiMakeButton("submit", "OK");
    hPrintf("</CENTER>");
    hPrintf("</FORM>");
    cartWebEnd();
    }
}

void foo()
{
hPrintf("After uploading data, look for the data set name in the graph ");
hPrintf("drop-down menus.");
}
