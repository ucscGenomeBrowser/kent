/* hgBioImage - CGI script to display things from bioImage database on web. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cart.h"
#include "jksql.h"
#include "cheapcgi.h"
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

void webMain()
{
#ifdef SOON
#endif /* SOON */
int id = cartUsualInt(cart, hgbiId, 1);
char query[256];
struct sqlResult *sr;
char **row;
struct sqlConnection *conn = sqlConnect("bioImage");
char fullPath[PATH_LEN];

safef(query, sizeof(query), "select fullLocation,fileName,submitId,gene,refSeq,genbank,isEmbryo,age,bodyPart,sliceType,imageType,treatment from image where id = %d", id);
uglyf("query: %s<BR>", query);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    uglyf("%s[0] %s[1] %s[2] %s[3] %s[4] %s[5] %s[6] %s[7] %s[8] %s[9] %s[10] %s[11]<BR>\n", 
    	row[0], row[1], row[2], row[3], row[4], row[5], row[6],
    	row[7], row[8], row[9], row[10], row[11]);
    sqlFreeResult(&sr);
    }
else
    {
    warn("Can't find image id %d", id);
    sqlFreeResult(&sr);
    }

bioImageFullPath(conn, id, fullPath);
uglyf("Image path: %s<BR>\n", fullPath);
sqlDisconnect(&conn);
}

void cartMain(struct cart *theCart)
/* We got the persistent/CGI variable cart.  Now
 * set up the globals and make a web page. */
{
cart = theCart;

    {
    /* Default case - start fancy web page. */
    cartWebStart(cart, "UCSC BioImage Browser");
    webMain();
    cartWebEnd();
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
