/* hgVisiGene - Gene Picture Browser. */
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
#include "visiGene.h"
#include "hgVisiGene.h"

/* Globals */
struct cart *cart;		/* Current CGI values */
struct hash *oldCart;		/* Previous CGI values */

static struct optionSpec options[] = {
   {NULL, 0},
};

char *hgVisiGeneShortName()
/* Return short descriptive name (not cgi-executable name)
 * of program */
{
return "Gene Pix";
}

char *hgVisiGeneCgiName()
/* Return short descriptive name (not cgi-executable name)
 * of program */
{
return "hgVisiGene";
}

int visiGeneMaxImageId(struct sqlConnection *conn)
/* Get the largest image ID.  Currently all images between
 * 1 and this exist, though I wouldn't count on this always
 * being true. */
{
char query[256];
safef(query, sizeof(query),
    "select max(id) from image");
return sqlQuickNum(conn, query);
}

struct slName *visiGeneGeneColorName(struct sqlConnection *conn, int id)
/* Get list of gene (color),gene (color) */
{
char query[512], llName[256];
struct slName *list = NULL, *el = NULL;
struct sqlResult *sr;
char **row;

/* Note I have found that some things originally selected
   did not get a name here, because of the additional 
   requirement for linking to probeColor, which was
   a result of some having probeColor id of 0.
   This would then mean the the picture's caption
   is missing the gene name entirely.
   So, to fix this, the probeColor.name lookup is 
   now done as an outer join.
*/
safef(query, sizeof(query),
      "select gene.name,gene.locusLink,gene.refSeq,gene.genbank,gene.uniProt,probeColor.name,image.paneLabel"
      " from imageProbe,probe,image,gene"
      " LEFT JOIN probeColor on imageProbe.probeColor = probeColor.id"
      " where imageProbe.image = %d"
      " and imageProbe.image = image.id"
      " and imageProbe.probe = probe.id"
      " and probe.gene = gene.id"
      , id);
      
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *geneName = NULL;
    char *name = row[0];
    char *locusLink = row[1];
    char *refSeq = row[2];
    char *genbank = row[3];
    char *uniProt = row[4];
    char *color = row[5];
    char *paneLabel = row[6];
    if (!color) 
	color = "?";
    if (name[0] != 0)
	geneName = name;
    else if (refSeq[0] != 0)
	geneName = refSeq;
    else if (genbank[0] != 0)
	geneName = genbank;
    else if (uniProt[0] != 0)
	geneName = uniProt;
    else if (locusLink[0] != 0)
	{
	safef(llName, sizeof(llName), "Entrez Gene %s", locusLink);
	geneName = llName;
	}
    if (geneName != NULL)
        {
	char buf[256];
	safef(buf, sizeof(buf), "%s (%s) %s", geneName, color, paneLabel);
	el = slNameNew(buf);
	slAddHead(&list, el);
	}
    }
sqlFreeResult(&sr);
slReverse(&list);
return list;
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
char *scientificName = visiGeneOrganism(conn, imageId);
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
nameList = visiGeneGeneColorName(conn, imageId);
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
    shortOrgName(visiGeneOrganism(conn, imageId)),
    visiGeneStage(conn, imageId, FALSE) );
}

struct slInt *idsInRange(struct sqlConnection *conn, int startId)
/* Select first 50 id's including and after startId */
{
char query[256], **row;
struct sqlResult *sr;
struct slInt *list = NULL, *el;

safef(query, sizeof(query), "select id from image where id >= %d limit %d",
	startId, 50);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = slIntNew(sqlUnsigned(row[0]));
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
slReverse(&list);
return list;
}

struct slInt *onePerImageFile(struct sqlConnection *conn, struct slInt *imageList)
/* Return image list that filters out second occurence of
 * same imageFile. */
{
struct slInt *newList = NULL, *image, *next;
struct hash *uniqHash = newHash(0);
char query[256];
for (image = imageList; image != NULL; image = next)
    {
    char hashName[16];
    int imageFile;
    next = image->next;
    safef(query, sizeof(query), "select imageFile from image where id=%d", 
    	image->val);
    imageFile = sqlQuickNum(conn, query);
    if (imageFile != 0)
	{
	safef(hashName, sizeof(hashName), "%x", imageFile);
	if (!hashLookup(uniqHash, hashName))
	    {
	    hashAdd(uniqHash, hashName, NULL);
	    slAddHead(&newList, image);
	    image = NULL;
	    }
	}
    freez(&image);
    }
hashFree(&uniqHash);
slReverse(&newList);
return newList;
}

void doThumbnails(struct sqlConnection *conn)
/* Write out list of thumbnail images. */
{
char *sidUrl = cartSidUrlString(cart);
int imageId = cartInt(cart, hgpId);
char *listHow = cartUsualString(cart, hgpListHow, listHowId);
char *listSpec = cartUsualString(cart, hgpListSpec, "1");
struct slInt *imageList = NULL, *image;
htmlSetBgColor(0xC0C0D6);
htmStart(stdout, "do image");


if (sameString(listHow, listHowId))
    imageList = idsInRange(conn, atoi(listSpec));
else if (sameString(listHow, listHowName))
    {
    char *sqlPat = sqlLikeFromWild(listSpec);
    if (sqlWildcardIn(sqlPat))
         imageList = visiGeneSelectNamed(conn, sqlPat, vgsLike);
    else
         imageList = visiGeneSelectNamed(conn, sqlPat, vgsExact);
    }
else if (sameString(listHow, listHowGenbank))
    {
    imageList = visiGeneSelectGenbank(conn, listSpec);
    }
else if (sameString(listHow, listHowLocusLink))
    {
    imageList = visiGeneSelectLocusLink(conn, listSpec);
    }
imageList = onePerImageFile(conn, imageList);
printf("<TABLE>\n");
for (image = imageList; image != NULL; image = image->next)
    {
    int id = image->val;
    char *imageFile = visiGeneThumbSizePath(conn, id);
    printf("<TR>");
    printf("<TD>");
    printf("<A HREF=\"../cgi-bin/%s?%s&%s=%d&%s=do\" target=\"image\" "
	"onclick=\"parent.controls.document.mainForm.ZM[0].checked=true;return true;\" "
	">", hgVisiGeneCgiName(), 
    	sidUrl, hgpId, id, hgpDoImage);
    printf("<IMG SRC=\"%s\"></A><BR>\n", imageFile);
    
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
char *permeablization, *publication, *copyright, *acknowledgement;
char *setUrl, *itemUrl;
char *caption = visiGeneCaption(conn, id);

printLabeledList("gene", geneList);
printf(" ");
printf("<B>organism:</B> %s  ", visiGeneOrganism(conn, id));
printf("<B>stage:</B> %s<BR>\n", visiGeneStage(conn, id, TRUE));
publication = visiGenePublication(conn,id);
if (publication != NULL)
    {
    char *pubUrl = visiGenePubUrl(conn,id);
    printf("<B>reference:</B> ");
    if (pubUrl != NULL)
        printf("<A HREF=\"%s\" target=_blank>%s</A>", pubUrl, publication);
    else
        printf("%s", publication);
    printf("<BR>\n");
    }
if (caption != NULL)
    {
    printf("<B>notes:</B> %s<BR>\n", caption);
    freez(&caption);
    }
printf("<B>body part:</B> %s ", naForNull(visiGeneBodyPart(conn,id)));
printf("<B>section type:</B> %s ", naForNull(visiGeneSliceType(conn,id)));
printLabeledList("genbank", visiGeneGenbank(conn, id));
printf("<BR>\n");
permeablization = visiGenePermeablization(conn,id);
if (permeablization != NULL)
    printf("<B>permeablization:</B> %s<BR>\n", permeablization);
printf("<B>contributors:</B> %s<BR>\n", naForNull(visiGeneContributors(conn,id)));
setUrl = visiGeneSetUrl(conn, id);
itemUrl = visiGeneItemUrl(conn, id);
if (setUrl != NULL || itemUrl != NULL)
    {
    printf("<B>%s links:</B> ", visiGeneSubmissionSource(conn, id));
    if (setUrl != NULL)
        printf("<A HREF=\"%s\" target=_blank>top level</A> ", setUrl);
    if (itemUrl != NULL)
	{
        printf("<A HREF=\"");
	printf(itemUrl, visiGeneSubmitId(conn, id));
	printf("\" target=_blank>this image</A>");
	}
    printf("<BR>\n");
    }
copyright = visiGeneCopyright(conn, id);
if (copyright != NULL)
    printf("<B>copyright:</B> %s<BR>\n", copyright);
acknowledgement = visiGeneAcknowledgement(conn, id);
if (acknowledgement != NULL)
    printf("<B>acknowledgements:</B> %s<BR>\n", acknowledgement);
printf("<BR>\n");
}

void doImage(struct sqlConnection *conn)
/* Put up image page. */
{
int imageId = cartInt(cart, hgpId);
struct slName *geneList = visiGeneGeneName(conn, imageId);
char buf[1024];
char *p = NULL;
char dir[256];
char name[128];
char extension[64];
int w = 0, h = 0;
htmlSetBgColor(0xE0E0E0);
htmStart(stdout, "do image");

printf("<B>VisiGene</B> Click in image to zoom, drag to move.  Caption is below.<BR>\n");
//smallCaption(conn, imageId);

// printf(" - see also <A HREF=\"#caption\">full caption</A> below<BR>\n");

//printf("<A HREF=\"");
//printf("../cgi-bin/%s?%s=%d&%s=on", hgVisiGeneCgiName(), hgpId, imageId, hgpDoFullSized);
//printf("\" target=_PARENT>");
//printf("<IMG SRC=\"%s\"></A><BR>\n", visiGeneScreenSizePath(conn, imageId));
//printf("\n");


visiGeneImageSize(conn, imageId, &w, &h);

p=visiGeneFullSizePath(conn, imageId);

splitPath(p, dir, name, extension);

safef(buf,sizeof(buf),"../visiGene/bigImage.html?url=%s%s/%s&w=%d&h=%d",dir,name,name,w,h);

printf("<IFRAME name=\"bigImg\" width=\"100%%\" height=\"80%%\" SRC=\"%s\"></IFRAME><BR>\n", buf);


printf("<A NAME=\"caption\"></A>");
printCaption(conn, imageId, geneList);
htmlEnd();
}

char *listHowMenu[] =
/* This and the listHowVals must be kept in sync. */
    {
    "Image ID",
    "Name (* and ? ok)",
    "Genbank",
    "Gene Entrez",
    };

char *listHowVals[] =
/* This and the listHowMenu must be kept in sync. */
    {
    listHowId,
    listHowName,
    listHowGenbank,
    listHowLocusLink,
    };

void doControls(struct sqlConnection *conn)
/* Put up controls pane. */
{
int imageId = cartInt(cart, hgpId);
char *listHow = cartUsualString(cart, hgpListHow, listHowId);
char *listSpec = cartUsualString(cart, hgpListSpec, "1");
char *selected = NULL;
int selIx = stringArrayIx(listHow, listHowVals, ArraySize(listHowVals));
htmlSetBgColor(0xD0FFE0);
htmStart(stdout, "do controls");
printf("<FORM ACTION=\"../cgi-bin/%s\" NAME=\"mainForm\" target=\"_parent\" METHOD=GET>\n",
	hgVisiGeneCgiName());
cartSaveSession(cart);
printf("Gene: ");

/* Put up drop-down list. */
if (selIx >= 0)
     selected = listHowMenu[selIx];
cgiMakeDropListFull(hgpListHow, listHowMenu, listHowVals,
	ArraySize(listHowMenu), selected, NULL);
printf(" ");
cgiMakeTextVar(hgpListSpec, listSpec, 10);

cgiMakeButton("submit", "submit");


printf(" Zoom: ");
printf("<INPUT TYPE=RADIO NAME=ZM onclick=\"parent.image.bigImg.changeMouse('in');return true;\" CHECKED>in ");
printf("<INPUT TYPE=RADIO NAME=ZM onclick=\"parent.image.bigImg.changeMouse('center');return true;\">center ");
printf("<INPUT TYPE=RADIO NAME=ZM onclick=\"parent.image.bigImg.changeMouse('out');return true;\">out ");
printf("\n");

printf("</FORM>\n");
printf("<BR> database: %s ", genomeDbForImage(conn, imageId));
htmlEnd();

}

void doFullSized(struct sqlConnection *conn)
/* Put up full sized image. */
{
int imageId = cartInt(cart, hgpId);
struct slName *geneList = visiGeneGeneName(conn, imageId);
char buf[1024];
char *p = NULL;
char dir[256];
char name[128];
char extension[64];
int w = 0, h = 0;
htmStart(stdout, "do image");
//printf("<A HREF=\"");
//printf("../cgi-bin/%s?%s=%d", hgVisiGeneCgiName(), hgpId, imageId);
//printf("\" target=_parent>");
//printf("<IMG SRC=\"%s\"></A><BR>\n", visiGeneFullSizePath(conn, imageId));

p=visiGeneFullSizePath(conn, imageId);

//safef(buf,sizeof(buf),"../htdocs/%s",p+3); /* skip "../" */

visiGeneImageSize(conn, imageId, &w, &h);

splitPath(p, dir, name, extension);

safef(buf,sizeof(buf),"../visiGene/bigImage.html?url=%s%s/%s&w=%d&h=%d",dir,name,name,w,h);

printf("<IFRAME width=\"100%%\" height=\"80%%\" SRC=\"%s\"></IFRAME><BR>\n", buf);

//printf("</A><BR>\n");

printCaption(conn, imageId, geneList);
htmlEnd();
freez(&p);
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
printf("%s ", hgVisiGeneShortName());
geneList = visiGeneGeneName(conn, imageId);
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    printf("%s", gene->name);
    if (gene->next != NULL)
        printf(",");
    }
printf(" %s %s",
	visiGeneStage(conn, imageId, FALSE),
	shortOrgName(visiGeneOrganism(conn, imageId)));
printf("</TITLE>\n");
printf("</HEAD>\n");

printf("<frameset cols=\"230,*\"> \n");
printf("  <frame src=\"../cgi-bin/%s?%s=go&%s/\" noresize frameborder=\"0\" name=\"list\">\n",
    hgVisiGeneCgiName(), hgpDoThumbnails, sidUrl);
printf("  <frameset rows=\"30,*\">\n");
printf("    <frame name=\"controls\" src=\"../cgi-bin/%s?%s=go&%s\" noresize marginwidth=\"0\" marginheight=\"0\" frameborder=\"0\">\n",
    hgVisiGeneCgiName(), hgpDoControls, sidUrl);
printf("    <frame src=\"../cgi-bin/%s?%s=go&%s/\" name=\"image\" noresize frameborder=\"0\">\n",
    hgVisiGeneCgiName(), hgpDoImage, sidUrl);
printf("  </frameset>\n");
printf("  <noframes>\n");
printf("  <body>\n");
printf("  <p>This web page uses frames, but your browser doesn't support them.</p>\n");
printf("  </body>\n");
printf("  </noframes>\n");
printf("</frameset>\n");


printf("</HTML>\n");
}

void doId(struct sqlConnection *conn)
/* Set up Gene Pix on given ID. */
{
int id = cartInt(cart, hgpDoId);
struct slName *genes = visiGeneGeneName(conn, id);
cartSetInt(cart, hgpId, id);
if (genes == NULL)
    {
    cartSetString(cart, hgpListHow, listHowId);
    cartSetInt(cart, hgpListSpec, id);
    }
else
    {
    cartSetString(cart, hgpListHow, listHowName);
    cartSetString(cart, hgpListSpec, genes->name);
    }
slFreeList(&genes);
doFrame(conn);
}

void dispatch()
/* Set up a connection to database and dispatch control
 * based on hgpDo type var. */
{
struct sqlConnection *conn = sqlConnect("visiGene");
if (cartVarExists(cart, hgpDoThumbnails))
    doThumbnails(conn);
else if (cartVarExists(cart, hgpDoImage))
    doImage(conn);
else if (cartVarExists(cart, hgpDoControls))
    doControls(conn);
else if (cartVarExists(cart, hgpDoFullSized))
    doFullSized(conn);
else if (cartVarExists(cart, hgpDoId))
    doId(conn);
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
