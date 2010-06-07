/* hgBioImage - CGI script to display things from bioImage database on web. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cart.h"
#include "dystring.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "portable.h"
#include "htmshell.h"
#include "web.h"
#include "cart.h"
#include "hui.h"
#include "bioImage.h"
#include "hgBioImage.h"

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

void printCaption(struct sqlConnection *conn, int id, char *geneName)
/* Print information about image. */
{
char query[256];
char **row;
char *treatment, *publication;
char *setUrl, *itemUrl;

printf("<B>gene:</B> %s ", naForNull(geneName));
printf("<B>accession:</B> %s ", naForNull(bioImageAccession(conn,id)));
printf("<B>type:</B> %s<BR>\n", naForNull(bioImageType(conn,id)));
printf("<B>organism:</B> %s  ", bioImageOrganism(conn, id));
printf("<B>stage:</B> %s<BR>\n", bioImageStage(conn, id, TRUE));
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
printf("<B>contributors:</B> %s<BR>\n", naForNull(bioImageContributors(conn,id)));
setUrl = bioImageSetUrl(conn, id);
itemUrl = bioImageItemUrl(conn, id);
if (setUrl != NULL || itemUrl != NULL)
    {
    printf("<B>contributor links:</B> ");
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
}


void mainPage(struct sqlConnection *conn, int id, char *geneName)
/* Put up fancy main page - image, caption, and then a bunch of thumbnails. */
{
struct slInt *thumbList = NULL, *thumb;

cartWebStart(cart, NULL, "UCSC BioImage Browser on %s", geneName);
printf("<A HREF=\"");
printf("../cgi-bin/hgBioImage?%s=%d&%s=on", hgbiId, id, hgbiDoFullSize);
printf("\">");
printf("<IMG SRC=\"/%s\" BORDER=1></A><BR>\n", bioImageScreenSizePath(conn, id));
printf("\n");
printCaption(conn, id, geneName);

htmlHorizontalLine();
printf("Other images associated with gene.  Click to view details.<BR>");
thumbList = bioImageOnSameGene(conn, id);
for (thumb = thumbList; thumb != NULL; thumb = thumb->next)
    {
    char *imageFile = bioImageThumbSizePath(conn, thumb->val);
    printf("<A HREF=\"../cgi-bin/hgBioImage?%s=%d\">", hgbiId, thumb->val);
    printf("<IMG SRC=\"/%s\" BORDER=1></A>\n", imageFile);
    printf("&nbsp;");
    }
cartWebEnd();
}

void cartMain(struct cart *theCart)
/* We got the persistent/CGI variable cart.  Now
 * set up the globals and make a web page. */
{
cart = theCart;

/* Break out new block after have set cart */
    {
    int id = cartUsualInt(cart, hgbiId, 1);
    struct sqlConnection *conn = sqlConnect("bioImage");
    char *geneName = bioImageGeneName(conn, id);
    if (cartVarExists(cart, hgbiDoFullSize))
	{
	htmStart(stdout, "BioImage Full Sized Image");
	printf("<IMG SRC=\"/%s\"><BR>\n", bioImageFullSizePath(conn, id));
	printCaption(conn, id, geneName);
	htmlEnd();
	}
    else  /* Default case - start fancy web page. */
	{
	mainPage(conn, id, geneName);
	}

    cartRemovePrefix(cart, hgbiDoPrefix);
    sqlDisconnect(&conn);
    }
}

char *excludeVars[] = {"Submit", "submit", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 1)
    usage();
oldCart = hashNew(12);
cartEmptyShell(cartMain, hUserCookie(), excludeVars, oldCart);
return 0;
}
