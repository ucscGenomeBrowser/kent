/* hgBioImage - CGI script to display things from bioImage database on web. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cart.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "portable.h"
#include "htmshell.h"
#include "web.h"
#include "cart.h"
#include "hui.h"
#include "hgBioImage.h"

char *cartOptionalString(struct cart *cart, char *var);

/* Globals */
struct cart *cart;		/* Current CGI values */
struct hash *oldCart;		/* Previous CGI values */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgBioImage - CGI script to display things from bioImage database on web\n"
  "usage:\n"
  "   hgBioImage XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void bioImageFullPath(struct sqlConnection *conn, int id, char path[PATH_LEN])
/* Fill in path for full image bioImage of given id. */
{
char query[256];
struct sqlResult *sr;
char **row;
safef(query, sizeof(query), 
	"select location.name,image.fileName from image,location"
	" where image.id = %d", id);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("No image of id %d", id);
safef(path, PATH_LEN, "%s/%s", row[0], row[1]);
sqlFreeResult(&sr);
}

void bioImageOrganism(struct sqlConnection *conn, int id, char organism[PATH_LEN])
/* Return binomial scientific name of organism associated with given image. */
{
char query[256];
safef(query, sizeof(query),
	"select uniProt.taxon.binomial from image,uniProt.taxon"
         " where image.id = %d and image.taxon = uniProt.taxon.id",
	 id);
sqlNeedQuickQuery(conn, query, organism, PATH_LEN);
}


void bioImageStage(struct sqlConnection *conn, int id, char stage[PATH_LEN])
/* Return string describing growth stage of organism */
{
char query[256];
double daysPassed;
char *startMarker;
struct sqlResult *sr;

char **row;
safef(query, sizeof(query),
    "select isEmbryo,age from image where id = %d", id);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("No image of id %d", id);
if (!sameString(row[0], "0"))
    startMarker = "fertilization";
else
    startMarker = "birth";
daysPassed = atof(row[1]);
safef(stage, PATH_LEN, "%3.1f days after %s", daysPassed, startMarker);
sqlFreeResult(&sr);
}

void bioImageGeneName(struct sqlConnection *conn, int id, char geneName[PATH_LEN])
/* Return gene name  - HUGO if possible, RefSeq/GenBank, etc if not. */
{
char query[256];
safef(query, sizeof(query),
	"select gene from image where id=%d", id);
sqlNeedQuickQuery(conn, query, geneName, PATH_LEN);
if (geneName[0] == 0)
    {
    safef(query, sizeof(query),
        "select refSeq from image where id=%d", id);
    sqlNeedQuickQuery(conn, query, geneName, PATH_LEN);
    if (geneName[0] == 0)
        {
	safef(query, sizeof(query),
	    "select genbank from image where id=%d", id);
	sqlNeedQuickQuery(conn, query, geneName, PATH_LEN);
	if (geneName[0] == 0)
	    {
	    char locusLink[32];
	    safef(query, sizeof(query), 
		"select locusLink from image where id=%d", id);
	    sqlNeedQuickQuery(conn, query, locusLink, sizeof(locusLink));
	    if (locusLink[0] == 0)
	        {
		safef(query, sizeof(query),
		    "select submitId from image where id=%d", id);
		sqlNeedQuickQuery(conn, query, geneName, PATH_LEN);
		}
	    else
	        {
		safef(geneName, PATH_LEN, "NCBI Gene ID #%s", locusLink);
		}
	    }
	}
    }
}

void bioImageAccession(struct sqlConnection *conn, int id, char accession[PATH_LEN])
/* Return RefSeq/Genbank accession or n/a if none available. */
{
char query[256];
safef(query, sizeof(query),
	"select refSeq from image where id=%d", id);
sqlNeedQuickQuery(conn, query, accession, PATH_LEN);
if (accession[0] == 0)
    {
    safef(query, sizeof(query),
        "select genbank from image where id=%d", id);
    sqlNeedQuickQuery(conn, query, accession, PATH_LEN);
    if (accession[0] == 0)
	safef(accession, PATH_LEN, "n/a");
    }
}

void bioImageType(struct sqlConnection *conn, int id, char type[PATH_LEN])
/* Return RefSeq/Genbank accession or n/a if none available. */
{
char query[256];
safef(query, sizeof(query),
	"select imageType.name from image,imageType "
	"where image.id=%d and imageType.id = image.imageType", id);
sqlNeedQuickQuery(conn, query, type, PATH_LEN);
if (type[0] == 0)
    safef(type, PATH_LEN, "n/a");
}


void webMain(struct sqlConnection *conn, int id, char *geneName)
{
char query[256];
struct sqlResult *sr;
char **row;
char fullPath[PATH_LEN], organism[PATH_LEN], stage[PATH_LEN], accession[PATH_LEN],
	imageType[PATH_LEN];


bioImageFullPath(conn, id, fullPath);
printf("<IMG SRC=\"/%s\"><BR>\n", fullPath);
uglyf("<B>Image path:</B> %s<BR>\n", fullPath);
bioImageOrganism(conn, id, organism);
printf("<B>organism:</B> %s  ", organism);
bioImageAccession(conn, id, accession);
bioImageStage(conn, id, stage);
bioImageType(conn, id, imageType);
printf("<B>stage:</B> %s<BR>\n", stage);
printf("<B>gene:</B> %s ", geneName);
printf("<B>accession:</B> %s ", accession);
printf("<B>type:</B> %s<BR>\n", imageType);
}

void cartMain(struct cart *theCart)
/* We got the persistent/CGI variable cart.  Now
 * set up the globals and make a web page. */
{
cart = theCart;


    {
    int id = cartUsualInt(cart, hgbiId, 1);
    struct sqlConnection *conn = sqlConnect("bioImage");
    /* Default case - start fancy web page. */
    char geneName[PATH_LEN];
    bioImageGeneName(conn, id, geneName);
    cartWebStart(cart, "UCSC BioImage Browser on %s", geneName);
    webMain(conn, id, geneName);
    cartWebEnd();
    sqlDisconnect(&conn);
    }
}

char *excludeVars[] = {"Submit", "submit", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
htmlSetStyle(htmlStyleUndecoratedLink);
if (argc != 1)
    usage();
oldCart = hashNew(12);
cartEmptyShell(cartMain, hUserCookie(), excludeVars, oldCart);
return 0;
}
