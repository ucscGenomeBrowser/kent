/* hgGenePix - Gene Picture Browser. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "hdb.h"
#include "hgColors.h"
#include "bioImage.h"
#include "hgGenePix.h"

/* Globals */
struct cart *cart;		/* Current CGI values */
struct hash *oldCart;		/* Previous CGI values */

static struct optionSpec options[] = {
   {NULL, 0},
};

char *hgGenePixShortName()
/* Return short descriptive name (not cgi-executable name)
 * of program */
{
return "Gene Pix";
}

char *hgGenePixCgiName()
/* Return short descriptive name (not cgi-executable name)
 * of program */
{
return "hgGenePix";
}

char *genomeDbForImage(struct sqlConnection *conn, int imageId)
/* Return the genome database to associate with image or NULL if none. */
{
char *scientificName = bioImageOrganism(conn, imageId);
struct sqlConnection *cConn = hConnectCentral();
char query[256];
char *db;

safef(query, sizeof(query), 
	"select defaultDb.name from defaultDb,dbDb where "
	"defaultDb.genome = dbDb.organism and "
	"dbDb.active = 1 and "
	"dbDb.scientificName = '%s'" 
	, scientificName);
db = sqlQuickString(cConn, query);
hDisconnectCentral(&cConn);
return db;
}

void doThumbnails(struct sqlConnection *conn)
/* Write out list of thumbnail images. */
{
char *sidUrl = cartSidUrlString(cart);
int imageId = cartInt(cart, hgpId);
struct slInt *imageList = bioImageOnSameGene(conn, imageId);
struct slInt *image;
htmlSetBgColor(0xC0C0D6);
htmStart(stdout, "do image");
printf("<TABLE>\n");
for (image = imageList; image != NULL; image = image->next)
    {
    int id = image->val;
    char *imageFile = bioImageThumbSizePath(conn, image->val);
    printf("<TR>");
    if (id == imageId)
	printf("<TD BGCOLOR=\"#FFE0E0\">");
    else
	printf("<TD>");
    printf("<A HREF=\"../cgi-bin/%s?%s&%s=%d\" target=_PARENT>", hgGenePixCgiName(), sidUrl, hgpId, image->val);
    printf("<IMG SRC=\"/%s\"></A><BR>\n", imageFile);
    
    printf("%s %s %s<BR>\n",
	bioImageGeneName(conn, id),
	bioImageOrganism(conn, id),
	bioImageStage(conn, id, FALSE) );
    printf("</TD>");
    printf("</TR>");
    }
printf("</TABLE>\n");
htmlEnd();
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

void doImage(struct sqlConnection *conn)
{
int imageId = cartInt(cart, hgpId);
char *geneName = bioImageGeneName(conn, imageId);
htmlSetBgColor(0xE0E0E0);
htmStart(stdout, "do image");
printf("<A HREF=\"");
printf("../cgi-bin/%s?%s=%d&%s=on", hgGenePixCgiName(), hgpId, imageId, hgpDoFullSized);
printf("\">");
printf("<IMG SRC=\"/%s\"></A><BR>\n", bioImageScreenSizePath(conn, imageId));
printf("\n");
printCaption(conn, imageId, geneName);
htmlEnd();
}

void doControls(struct sqlConnection *conn)
{
int imageId = cartInt(cart, hgpId);
htmlSetBgColor(0xD0FFE0);
htmStart(stdout, "do image");
printf("database for image: %s ", genomeDbForImage(conn, imageId));
printf("Do controls");
htmlEnd();
}

void doFullSized(struct sqlConnection *conn)
{
htmStart(stdout, "do image");
printf("Do full sized");
htmlEnd();
}


void doFrame(struct sqlConnection *conn)
/* Make a html frame page.  Fill frame with thumbnail, control bar,
 * and image panes. */
{
int imageId = cartUsualInt(cart, hgpId, 1);
char *sidUrl = cartSidUrlString(cart);
cartSetInt(cart, hgpId, imageId);
puts("\n");
puts("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">");
printf("<HTML>\n");
printf("<HEAD>\n");
printf("<TITLE>\n");
printf("%s %s %s %s",  hgGenePixShortName(),  
	bioImageGeneName(conn, imageId),
	bioImageStage(conn, imageId, FALSE),
	bioImageOrganism(conn, imageId) );
printf("</TITLE>\n");
printf("</HEAD>\n");

printf("<frameset cols=\"230,*\"> \n");
printf("  <frame src=\"../cgi-bin/%s?%s=go&%s/\" noresize frameborder=\"0\" name=\"thumbList\">\n",
    hgGenePixCgiName(), hgpDoThumbnails, sidUrl);
printf("  <frameset rows=\"30,*\">\n");
printf("    <frame name=\"controls\" src=\"../cgi-bin/%s?%s=go&%s\" noresize marginwidth=\"0\" marginheight=\"0\" frameborder=\"0\">\n",
    hgGenePixCgiName(), hgpDoControls, sidUrl);
printf("    <frame src=\"../cgi-bin/%s?%s=go&%s/\" name=\"fullImage\" noresize frameborder=\"0\">\n",
    hgGenePixCgiName(), hgpDoImage, sidUrl);
printf("  </frameset>\n");
printf("  <noframes>\n");
printf("  <body>\n");
printf("  <p>This web page uses frames, but your browser doesn't support them.</p>\n");
printf("  </body>\n");
printf("  </noframes>\n");
printf("</frameset>\n");


printf("</HTML>\n");
}

void dispatch()
/* Set up a connection to database and dispatch control
 * based on hgpDo type var. */
{
struct sqlConnection *conn = sqlConnect("bioImage");
if (cartVarExists(cart, hgpDoThumbnails))
    doThumbnails(conn);
else if (cartVarExists(cart, hgpDoImage))
    doImage(conn);
else if (cartVarExists(cart, hgpDoControls))
    doControls(conn);
else if (cartVarExists(cart, hgpDoFullSized))
    doFullSized(conn);
else
    doFrame(conn);
cartRemovePrefix(cart, hgpDoPrefix);
}

void doMiddle(struct cart *theCart)
/* Save cart to global, print time, call dispatch */
{
cart = theCart;
dispatch();
}

char *excludeVars[] = {"Submit", "submit", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
uglyTime(NULL);
cgiSpoof(&argc, argv);
oldCart = hashNew(0);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldCart);
return 0;
}
