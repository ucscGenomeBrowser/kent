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

char *bioImageFullPath(struct sqlConnection *conn, int id)
/* Fill in path for full image bioImage of given id. */
{
char query[256], path[PATH_LEN];
struct sqlResult *sr;
char **row;
safef(query, sizeof(query), 
	"select location.name,image.fileName from image,location"
	" where image.id = %d and location.id=image.fullLocation", id);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) == NULL)
    errAbort("No image of id %d", id);
safef(path, PATH_LEN, "%s/%s", row[0], row[1]);
sqlFreeResult(&sr);
return cloneString(path);
}

char *cloneOrNull(char *s)
/* Return copy of string, or NULL if it is empty */
{
if (s == NULL || s[0] == 0)
    return NULL;
return cloneString(s);
}

char *bioImageOrganism(struct sqlConnection *conn, int id)
/* Return binomial scientific name of organism associated with given image. 
 * FreeMem this when done. */
{
char query[256], buf[256];
safef(query, sizeof(query),
	"select uniProt.taxon.binomial from image,uniProt.taxon"
         " where image.id = %d and image.taxon = uniProt.taxon.id",
	 id);
sqlNeedQuickQuery(conn, query, buf, sizeof(buf));
return cloneString(buf);
}


char *bioImageStage(struct sqlConnection *conn, int id)
/* Return string describing growth stage of organism 
 * FreeMem this when done. */
{
char query[256], buf[256];
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
safef(buf, sizeof(buf), "%3.1f days after %s", daysPassed, startMarker);
sqlFreeResult(&sr);
return cloneString(buf);
}

char *bioImageGeneName(struct sqlConnection *conn, int id)
/* Return gene name  - HUGO if possible, RefSeq/GenBank, etc if not. */
{
char query[256], buf[256];
safef(query, sizeof(query),
	"select gene from image where id=%d", id);
sqlNeedQuickQuery(conn, query, buf, sizeof(buf));
if (buf[0] == 0)
    {
    safef(query, sizeof(query),
        "select refSeq from image where id=%d", id);
    sqlNeedQuickQuery(conn, query, buf, sizeof(buf));
    if (buf[0] == 0)
        {
	safef(query, sizeof(query),
	    "select genbank from image where id=%d", id);
	sqlNeedQuickQuery(conn, query, buf, sizeof(buf));
	if (buf[0] == 0)
	    {
	    char locusLink[32];
	    safef(query, sizeof(query), 
		"select locusLink from image where id=%d", id);
	    sqlNeedQuickQuery(conn, query, locusLink, sizeof(locusLink));
	    if (locusLink[0] == 0)
	        {
		safef(query, sizeof(query),
		    "select submitId from image where id=%d", id);
		sqlNeedQuickQuery(conn, query, buf, sizeof(buf));
		}
	    else
	        {
		safef(buf, sizeof(buf), "NCBI Gene ID #%s", locusLink);
		}
	    }
	}
    }
return cloneString(buf);
}

char *bioImageAccession(struct sqlConnection *conn, int id)
/* Return RefSeq/Genbank accession or n/a if none available. */
{
char query[256], buf[256];
safef(query, sizeof(query),
	"select refSeq from image where id=%d", id);
sqlNeedQuickQuery(conn, query, buf, sizeof(buf));
if (buf[0] == 0)
    {
    safef(query, sizeof(query),
        "select genbank from image where id=%d", id);
    sqlNeedQuickQuery(conn, query, buf, sizeof(buf));
    }
return cloneOrNull(buf);
}

char *bioImageSubmitId(struct sqlConnection *conn, int id)
/* Return submitId  for image. */
{
char query[256], buf[256];
safef(query, sizeof(query), 
	"select submitId from image"
	" where image.id=%d", id);
return cloneString(sqlQuickQuery(conn, query, buf, sizeof(buf)));
}

char *bioImageType(struct sqlConnection *conn, int id)
/* Return RefSeq/Genbank accession or n/a if none available. */
{
char query[256], buf[256];
safef(query, sizeof(query),
	"select imageType.name from image,imageType "
	"where image.id=%d and imageType.id = image.imageType", id);
sqlNeedQuickQuery(conn, query, buf, sizeof(buf));
return cloneOrNull(buf);
}

static char *indirectString(struct sqlConnection *conn, int id, char *table, char *valField)
/* Return value on table that is connected via id to image. */
{
char query[256], buf[512];
safef(query, sizeof(query),
	"select %s.%s from image,%s "
	"where image.id=%d and image.%s = %s.id",
	table, valField, table, id, table, table);
return cloneOrNull(sqlQuickQuery(conn, query, buf, sizeof(buf)));
}

char *bioImageBodyPart(struct sqlConnection *conn, int id)
/* Return body part if any. */
{
return indirectString(conn, id, "bodyPart", "name");
}

char *bioImageSliceType(struct sqlConnection *conn, int id)
/* Return slice type if any. */
{
return indirectString(conn, id, "sliceType", "name");
}

char *bioImageTreatment(struct sqlConnection *conn, int id)
/* Return treatment if any. */
{
return indirectString(conn, id, "treatment", "conditions");
}

static char *submissionSetPart(struct sqlConnection *conn, int id, char *field)
/* Return field from submissionSet that is linked to image. */
{
return indirectString(conn, id, "submissionSet", field);
}

char *bioImageContributers(struct sqlConnection *conn, int id)
/* Return comma-separated list of contributers in format Kent W.H, Wu F.Y. */
{
return submissionSetPart(conn, id, "contributers");
}

char *bioImagePublication(struct sqlConnection *conn, int id)
/* Return name of publication associated with image. */
{
return submissionSetPart(conn, id, "publication");
}

char *bioImagePubUrl(struct sqlConnection *conn, int id)
/* Return url of publication associated with image. */
{
return submissionSetPart(conn, id, "pubUrl");
}

char *bioImageSetUrl(struct sqlConnection *conn, int id)
/* Return contributer url associated with image set. */
{
return submissionSetPart(conn, id, "setUrl");
}

char *bioImageItemUrl(struct sqlConnection *conn, int id)
/* Return contributer url associated with this image. 
 * Substitute in submitId for %s before using. */
{
return submissionSetPart(conn, id, "itemUrl");
}



void webMain(struct sqlConnection *conn, int id, char *geneName)
{
char query[256];
struct sqlResult *sr;
char **row;
char *fullPath = bioImageFullPath(conn, id);
char *treatment, *publication;
char *setUrl, *itemUrl;

printf("<B>organism:</B> %s  ", bioImageOrganism(conn, id));
printf("<B>stage:</B> %s<BR>\n", bioImageStage(conn, id));
printf("<B>gene:</B> %s ", naForNull(geneName));
printf("<B>accession:</B> %s ", naForNull(bioImageAccession(conn,id)));
printf("<B>type:</B> %s<BR>\n", naForNull(bioImageType(conn,id)));
printf("<B>body part:</B> %s ", naForNull(bioImageBodyPart(conn,id)));
printf("<B>section type:</B> %s<BR>\n", naForNull(bioImageSliceType(conn,id)));
treatment = bioImageTreatment(conn,id);
if (treatment != NULL)
    printf("<B>treatment:</B> %s<BR>\n", treatment);
publication = bioImagePublication(conn,id);
if (publication != NULL)
    {
    char *pubUrl = bioImagePubUrl(conn,id);
    printf("<B>reference:</B> ");
    if (pubUrl != NULL)
        printf("<A HREF=\"%s\" target=_blank>%s</A>", pubUrl, publication);
    else
        printf("%s", publication);
    printf("<BR>\n");
    }
printf("<B>contributors:</B> %s<BR>\n", naForNull(bioImageContributers(conn,id)));
setUrl = bioImageSetUrl(conn, id);
itemUrl = bioImageItemUrl(conn, id);
if (setUrl != NULL || itemUrl != NULL)
    {
    printf("<B>contributer links:</B> ");
    if (setUrl != NULL)
        printf("<A HREF=\"%s\" target=_blank>image set</A> ", setUrl);
    if (itemUrl != NULL)
	{
        printf("<A HREF=\"");
	printf(itemUrl, bioImageSubmitId(conn, id));
	printf("\" target=_blank>this image</A>");
	}
    printf("<BR>\n");
    }
printf("<IMG SRC=\"/%s\"><BR>\n", fullPath);
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
    char *geneName = bioImageGeneName(conn, id);
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
