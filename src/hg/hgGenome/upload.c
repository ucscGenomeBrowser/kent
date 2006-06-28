/* Upload - put up upload pages and sub-pages. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "ra.h"
#include "portable.h"
#include "cheapcgi.h"
#include "cart.h"
#include "web.h"
#include "hgGenome.h"
#include "errCatch.h"

/* Symbolic defines for types of markers we support. */
#define hggUpGenomic "genomic"
#define hggUpSts "sts"
#define hggUpSnp "snp"
#define hggUpAffy100 "affy100k"
#define hggUpAffy500 "affy500k"
#define hggUpHumanHap300 "humanHap300"

static char *locDescriptions[] = {
	"chromosome base",
	"STS marker",
	"dbSNP rsID",
	"Affymetrix 100K Gene Chip",
	"Affymetrix 500K Gene Chip",
	"Illumina HumanHap300 BeadChip",
	};

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
cartWebStart(cart, "Upload Data to Genome Association View");
hPrintf("<FORM ACTION=\"../cgi-bin/hgGenome\" METHOD=\"POST\" ENCTYPE=\"multipart/form-data\">");
cartSaveSession(cart);
hPrintf("Name of data set: ");
cartMakeTextVar(cart, hggDataSetName, "", 16);
hPrintf(" Locations are: ");
cgiMakeDropListFull(hggLocType, locDescriptions, locNames, 
	ArraySize(locNames), cartUsualString(cart, hggLocType, locNames[0]),
	NULL);
hPrintf("<BR>");
hPrintf("Description: ");
cartMakeTextVar(cart, hggDataSetDescription, "", 64);
hPrintf("<BR>");
hPrintf("File name: <INPUT TYPE=FILE NAME=\"%s\">", hggUploadFile);
cgiMakeButton(hggSubmitUpload, "Submit");
hPrintf("</FORM>\n");
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

void  processDbBed(struct lineFile *lf, FILE *f, char *bedTable)
/* Process two column input file into chromGraph.  Treat first
 * column as a name to look up in bed-format table, which should
 * not be split. Return TRUE on success. */
{
uglyf("Theoretically processing via bed-format table %s<BR>", bedTable);
}

void  processGenomic(struct lineFile *lf, FILE *f)
/* Process three column file into chromGraph.  Abort if
 * there's a problem. */
{
uglyf("Theoretically processing via three column format.<BR>");
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

boolean mayProcessGenomic(struct lineFile *lf, FILE *f)
/* Process three column file into chromGraph.  If there's a problem
 * print warning message and return FALSE. */
{
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
     processGenomic(lf, f);
return errCatchFinish(&errCatch);
}

boolean mayProcessDbBed(struct lineFile *lf, FILE *f, char *bedTable)
/* Process three column file into chromGraph.  If there's a problem
 * print warning message and return FALSE. */
{
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
     processDbBed(lf, f, bedTable);
return errCatchFinish(&errCatch);
}

void updateUploadRa(char *binFileName)
/* Update upload ra file with current upload data */
{
uglyf("Theoretically updating upload ra file.<BR>\n");
}

void processUpload(char *text, struct sqlConnection *conn)
/* Parse uploaded text.  If it looks good then make a 
 * binary chromGraph file out of it, and save information
 * about it in the upload ra file. */
{
char *type = cartUsualString(cart, hggLocType, hggUpGenomic);
struct tempName tempName;
char *binFile;
FILE *f;
boolean ok = FALSE;
struct lineFile *lf = lineFileOnString("uploaded data", TRUE, text);
/* NB - do *not* lineFileClose this or a double free can happen. */

makeTempName(&tempName, "up", ".cgb");
binFile = tempName.forCgi;
f = mustOpen(binFile, "wb");
if (sameString(type, hggUpGenomic))
    ok = mayProcessGenomic(lf, f);
else if (sameString(type, hggUpSts))
    ok = mayProcessDbBed(lf, f, "stsMap");
else if (sameString(type, hggUpSnp))
    {
    if (sqlTableExists(conn, "snp126"))
        ok = mayProcessDbBed(lf, f, "snp126");
    else if (sqlTableExists(conn, "snp125"))
        ok = mayProcessDbBed(lf, f, "snp125");
    else if (sqlTableExists(conn, "snp"))
        ok = mayProcessDbBed(lf, f, "snp");
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
carefulClose(&f);
if (ok)
    updateUploadRa(binFile);
}

void submitUpload(struct sqlConnection *conn)
/* Called when they've submitted from uploads page */
{
char *rawText = cartUsualString(cart, hggUploadFile, "");
int rawTextSize = strlen(rawText);
cartWebStart(cart, "Data Upload Complete (%d bytes)", rawTextSize);
hPrintf("<FORM ACTION=\"../cgi-bin/hgGenome\">");
cartSaveSession(cart);
processUpload(rawText, conn);
cartRemove(cart, hggUploadFile);
cgiMakeButton("submit", "OK");
hPrintf("</FORM>");
cartWebEnd();
}

void foo()
{
hPrintf("After uploading data, look for the data set name in the graph ");
hPrintf("drop-down menus.");
}
