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
#include "genePix.h"
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

char *shortOrgName(char *binomial)
/* Return short name for taxon - scientific or common whatever is
 * shorter */
{
static struct hash *hash = NULL;
char *name;

if (hash == NULL)
    hash = hashNew(0);
name = hashFindVal(hash, binomial);
if (name == NULL)
    {
    struct sqlConnection *conn = sqlConnect("uniProt");
    char query[256], **row;
    struct sqlResult *sr;
    int nameSize = strlen(binomial);
    name = cloneString(binomial);
    safef(query, sizeof(query), 
    	"select commonName.val from commonName,taxon "
	"where taxon.binomial = '%s' and taxon.id = commonName.taxon"
	, binomial);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	int len = strlen(row[0]);
	if (len < nameSize)
	    {
	    freeMem(name);
	    name = cloneString(row[0]);
	    nameSize = len;
	    }
	}
    sqlFreeResult(&sr);
    sqlDisconnect(&conn);
    hashAdd(hash, binomial, name);
    }
return name;
}

char *genomeDbForImage(struct sqlConnection *conn, int imageId)
/* Return the genome database to associate with image or NULL if none. */
{
char *scientificName = genePixOrganism(conn, imageId);
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

void smallCaption(struct sqlConnection *conn, int imageId)
/* Write out small format caption. */
{
struct slName *nameList, *name;
nameList = genePixGeneName(conn, imageId);
printf("<B>");
for (name = nameList; name != NULL; name = name->next)
    {
    printf("%s", name->name);
    if (name->next != NULL)
	printf(",");
    }
printf("</B>");
slFreeList(&nameList);
printf(" %s %s",
    shortOrgName(genePixOrganism(conn, imageId)),
    genePixStage(conn, imageId, FALSE) );
}

void doThumbnails(struct sqlConnection *conn)
/* Write out list of thumbnail images. */
{
char *sidUrl = cartSidUrlString(cart);
int imageId = cartInt(cart, hgpId);
struct slInt *imageList = genePixOnSameGene(conn, imageId);
struct slInt *image;
htmlSetBgColor(0xC0C0D6);
htmStart(stdout, "do image");
printf("<TABLE>\n");
for (image = imageList; image != NULL; image = image->next)
    {
    int id = image->val;
    char *imageFile = genePixThumbSizePath(conn, image->val);
    printf("<TR>");
    printf("<TD>");
    printf("<A HREF=\"../cgi-bin/%s?%s&%s=%d&%s=do\" target=\"image\">", hgGenePixCgiName(), 
    	sidUrl, hgpId, image->val, hgpDoImage);
    printf("<IMG SRC=\"/%s\"></A><BR>\n", imageFile);
    
    smallCaption(conn, id);
    printf("<BR>\n");
    printf("</TD>");
    printf("</TR>");
    }
printf("</TABLE>\n");
htmlEnd();
}

void printLabeledList(char *label, struct slName *list)
/* Print label and list by it. */
{
struct slName *el;
printf("<B>%s:</B> ", label);
if (list == NULL)
    printf("n/a");
for (el = list; el != NULL; el = el->next)
    {
    printf("%s", el->name);
    if (el->next != NULL)
        printf(", ");
    }
}

void printCaption(struct sqlConnection *conn, int id, struct slName *geneList)
/* Print information about image. */
{
char query[256];
char **row;
char *treatment, *publication;
char *setUrl, *itemUrl;

printLabeledList("gene", geneList);
printf(" ");
printLabeledList("genbank", genePixGenbank(conn, id));
printf(" ");
printf("<B>organism:</B> %s  ", genePixOrganism(conn, id));
printf("<B>stage:</B> %s<BR>\n", genePixStage(conn, id, TRUE));
printf("<B>body part:</B> %s ", naForNull(genePixBodyPart(conn,id)));
printf("<B>section type:</B> %s<BR>\n", naForNull(genePixSliceType(conn,id)));
treatment = genePixTreatment(conn,id);
if (treatment != NULL)
    printf("<B>treatment:</B> %s<BR>\n", treatment);
publication = genePixPublication(conn,id);
if (publication != NULL)
    {
    char *pubUrl = genePixPubUrl(conn,id);
    printf("<B>reference:</B> ");
    if (pubUrl != NULL)
        printf("<A HREF=\"%s\" target=_blank>%s</A>", pubUrl, publication);
    else
        printf("%s", publication);
    printf("<BR>\n");
    }
printf("<B>contributors:</B> %s<BR>\n", naForNull(genePixContributors(conn,id)));
setUrl = genePixSetUrl(conn, id);
itemUrl = genePixItemUrl(conn, id);
if (setUrl != NULL || itemUrl != NULL)
    {
    printf("<B>contributor links:</B> ");
    if (setUrl != NULL)
        printf("<A HREF=\"%s\" target=_blank>image set</A> ", setUrl);
    if (itemUrl != NULL)
	{
        printf("<A HREF=\"");
	printf(itemUrl, genePixSubmitId(conn, id));
	printf("\" target=_blank>this image</A>");
	}
    printf("<BR>\n");
    }
}

void doImage(struct sqlConnection *conn)
/* Put up image page. */
{
int imageId = cartInt(cart, hgpId);
struct slName *geneList = genePixGeneName(conn, imageId);
htmlSetBgColor(0xE0E0E0);
htmStart(stdout, "do image");

smallCaption(conn, imageId);
printf(" - see also <A HREF=\"#caption\">full caption</A> below<BR>\n");
printf("<A HREF=\"");
printf("../cgi-bin/%s?%s=%d&%s=on", hgGenePixCgiName(), hgpId, imageId, hgpDoFullSized);
printf("\" target=_PARENT>");
printf("<IMG SRC=\"/%s\"></A><BR>\n", genePixScreenSizePath(conn, imageId));
printf("\n");
printf("<A NAME=\"caption\"></A>");
printCaption(conn, imageId, geneList);
htmlEnd();
}

void doControls(struct sqlConnection *conn)
/* Put up controls pane. */
{
int imageId = cartInt(cart, hgpId);
htmlSetBgColor(0xD0FFE0);
htmStart(stdout, "do controls");
printf("<FORM ACTION=\"../cgi-bin/%s\" NAME=\"mainForm\" target=\"_PARENT\" METHOD=GET>\n",
	hgGenePixCgiName());
printf("Image ID (1-5978): ");
cgiMakeIntVar(hgpId,  imageId, 4);
printf(" database: %s ", genomeDbForImage(conn, imageId));
cgiMakeButton("submit", "submit");
printf("</FORM>\n");
htmlEnd();
}

void doFullSized(struct sqlConnection *conn)
/* Put up full sized image. */
{
int imageId = cartInt(cart, hgpId);
struct slName *geneList = genePixGeneName(conn, imageId);
htmStart(stdout, "do image");
printf("<A HREF=\"");
printf("../cgi-bin/%s?%s=%d", hgGenePixCgiName(), hgpId, imageId);
printf("\" target=_PARENT>");
printf("<IMG SRC=\"/%s\"></A><BR>\n", genePixFullSizePath(conn, imageId));
printf("</A><BR>\n");
printCaption(conn, imageId, geneList);
htmlEnd();
}


void doFrame(struct sqlConnection *conn)
/* Make a html frame page.  Fill frame with thumbnail, control bar,
 * and image panes. */
{
int imageId = cartUsualInt(cart, hgpId, 1);
char *sidUrl = cartSidUrlString(cart);
struct slName *geneList, *gene;
cartSetInt(cart, hgpId, imageId);
puts("\n");
puts("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">");
printf("<HTML>\n");
printf("<HEAD>\n");
printf("<TITLE>\n");
printf("%s ", hgGenePixShortName());
geneList = genePixGeneName(conn, imageId);
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    printf("%s", gene->name);
    if (gene->next != NULL)
        printf(",");
    }
printf(" %s %s",
	genePixStage(conn, imageId, FALSE),
	shortOrgName(genePixOrganism(conn, imageId)));
printf("</TITLE>\n");
printf("</HEAD>\n");

printf("<frameset cols=\"230,*\"> \n");
printf("  <frame src=\"../cgi-bin/%s?%s=go&%s/\" noresize frameborder=\"0\" name=\"list\">\n",
    hgGenePixCgiName(), hgpDoThumbnails, sidUrl);
printf("  <frameset rows=\"30,*\">\n");
printf("    <frame name=\"controls\" src=\"../cgi-bin/%s?%s=go&%s\" noresize marginwidth=\"0\" marginheight=\"0\" frameborder=\"0\">\n",
    hgGenePixCgiName(), hgpDoControls, sidUrl);
printf("    <frame src=\"../cgi-bin/%s?%s=go&%s/\" name=\"image\" noresize frameborder=\"0\">\n",
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
struct sqlConnection *conn = sqlConnect("genePix");
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
