/* hgVisiGene - Gene Picture Browser. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "portable.h"
#include "cart.h"
#include "hui.h"
#include "hdb.h"
#include "hgColors.h"
#include "web.h"
#include "visiGene.h"
#include "hgVisiGene.h"
#include "captionElement.h"
#include "visiSearch.h"
#include "probePage.h"
#include "printCaption.h"

/* Globals */
struct cart *cart;		/* Current CGI values */
struct hash *oldCart;		/* Previous CGI values */
char *visiDb = "visiGene";	/* The visiGene database name */
struct sqlConnCache *visiConnCache;  /* Cache of connections to database. */

static struct optionSpec options[] = {
   {NULL, 0},
};

char *hgVisiGeneShortName()
/* Return short descriptive name (not cgi-executable name)
 * of program */
{
return "VisiGene";
}

char *hgVisiGeneCgiName()
/* Return name of executable. */
{
return "hgVisiGene";
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

char *makeCommaSpacedList(struct slName *list)
/* Turn linked list of strings into a single string with
 * elements separated by a comma and a space.  You can
 * freeMem the result when done. */
{
int totalSize = 0, elCount = 0;
struct slName *el;

for (el = list; el != NULL; el = el->next)
    {
    if (el->name[0] != 0)
	{
	totalSize += strlen(el->name);
	elCount += 1;
	}
    }
if (elCount == 0)
    return cloneString("n/a");
else
    {
    char *pt, *result;
    totalSize += 2*(elCount-1);	/* Space for ", " */
    pt = result = needMem(totalSize+1);
    strcpy(pt, list->name);
    pt += strlen(list->name);
    for (el = list->next; el != NULL; el = el->next)
        {
	*pt++ = ',';
	*pt++ = ' ';
	strcpy(pt, el->name);
	pt += strlen(el->name);
	}
    return result;
    }
}

#ifdef UNUSED
struct sqlConnection *vAllocConn()
/* Get a connection from connection cache */
{
return sqlAllocConnection(visiConnCache);
}

void vFreeConn(struct sqlConnection **pConn)
/* Free up connection from connection cache. */
{
sqlFreeConnection(visiConnCache, pConn);
}
#endif /* UNUSED */

#ifdef UNUSED
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
#endif /* UNUSED */

static void saveMatchFile(char *fileName, struct visiMatch *matchList)
/* Save out matches to file name */
{
struct visiMatch *match;
FILE *f = mustOpen(fileName, "w");
for (match = matchList; match != NULL; match = match->next)
    fprintf(f, "%d\t%f\n", match->imageId, match->weight);
carefulClose(&f);
}

static struct visiMatch *readMatchFile(char *fileName)
/* Read in match file */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
struct visiMatch *matchList = NULL, *match;
int count;
while (lineFileRow(lf, row))
    {
    AllocVar(match);
    match->imageId = lineFileNeedNum(lf, row, 0);
    match->weight = lineFileNeedDouble(lf, row, 1);
    slAddHead(&matchList, match);
    }
lineFileClose(&lf);
slReverse(&matchList);
return matchList;
}

static struct visiMatch *onePerImageFile(struct sqlConnection *conn, 
	struct visiMatch *matchList)
/* Return image list that filters out second occurence of
 * same imageFile. */
{
struct visiMatch *newList = NULL, *match, *next;
struct hash *uniqHash = newHash(0);
char query[256];
for (match = matchList; match != NULL; match = next)
    {
    char hashName[16];
    int imageFile;
    next = match->next;
    safef(query, sizeof(query), "select imageFile from image where id=%d", 
    	match->imageId);
    imageFile = sqlQuickNum(conn, query);
    if (imageFile != 0)
	{
	safef(hashName, sizeof(hashName), "%x", imageFile);
	if (!hashLookup(uniqHash, hashName))
	    {
	    hashAdd(uniqHash, hashName, NULL);
	    slAddHead(&newList, match);
	    match = NULL;
	    }
	}
    freez(&match);
    }
hashFree(&uniqHash);
slReverse(&newList);
return newList;
}

static void doThumbnails(struct sqlConnection *conn)
/* Write out list of thumbnail images. */
{
char *sidUrl = cartSidUrlString(cart);
char *listSpec = cartUsualString(cart, hgpListSpec, "");
char *matchFile = cartString(cart, hgpMatchFile);
struct visiMatch *matchList = NULL, *match;
int maxCount = 25, count = 0;
int startAt = cartUsualInt(cart, hgpStartAt, 0);
int imageCount;

htmlSetBgColor(0xC0C0D6);
htmStart(stdout, "doThumbnails");
matchList = readMatchFile(matchFile);
imageCount = slCount(matchList);
if (imageCount > 0)
    {
    printf("<TABLE>\n");
    printf("<TR><TD><B>");
    printf("%d images match<BR>\n", imageCount);
    printf("</B></TD></TR>\n");
    for (match = slElementFromIx(matchList, startAt); 
	    match != NULL; match = match->next)
	{
	int id = match->imageId;
	char *imageFile = visiGeneThumbSizePath(conn, id);
	printf("<TR>");
	printf("<TD>");
	printf("<A HREF=\"../cgi-bin/%s?%s&%s=%d&%s=do\" target=\"image\" >", 
	    hgVisiGeneCgiName(), 
	    sidUrl, hgpId, id, hgpDoImage);
	printf("<IMG SRC=\"%s\"></A><BR>\n", imageFile);
	
	smallCaption(conn, id);
	printf("<BR>\n");
	printf("</TD>");
	printf("</TR>");
	if (++count >= maxCount)
	    break;
	}
    printf("</TABLE>\n");
    printf("<HR>\n");
    }
if (count != imageCount)
   {
   int start;
   int page = 0;
   printf("%d-%d of %d for:<BR>", startAt+1,
   	startAt+count, imageCount);
   printf("&nbsp;%s<BR>\n", listSpec);
   printf("Page:\n");
   for (start=0; start<imageCount; start += maxCount)
       {
       ++page;
       if (start != startAt)
	   {
	   printf("<A HREF=\"%s?", hgVisiGeneCgiName());
	   printf("%s&", sidUrl);
	   printf("%s=on&", hgpDoThumbnails);
	   printf("%s=%s&", hgpListSpec, listSpec);
	   if (start != 0)
	       printf("%s=%d", hgpStartAt, start);
	   printf("\">");
	   }
       printf("%d", page);
       if (start != startAt)
	   printf("</A>");
       printf(" ");
       }
   }
else
    {
    printf("%d images for:<BR>", imageCount);
    printf("&nbsp;%s<BR>\n", listSpec);
    }
cartRemove(cart, hgpStartAt);
htmlEnd();
}

void doImage(struct sqlConnection *conn)
/* Put up image page. */
{
int imageId = cartUsualInt(cart, hgpId, 0);
char buf[1024];
char *p = NULL;
char dir[256];
char name[128];
char extension[64];
int w = 0, h = 0;
htmlSetBgColor(0xE0E0E0);
htmStart(stdout, "do image");

if (imageId != 0)
    {
    printf("<B>");
    smallCaption(conn, imageId);
    printf(".</B> Click in image to zoom, drag to move.  "
	   "Caption is below.<BR>\n");

    visiGeneImageSize(conn, imageId, &w, &h);
    p=visiGeneFullSizePath(conn, imageId);

    splitPath(p, dir, name, extension);
    safef(buf,sizeof(buf),"../visiGene/bigImage.html?url=%s%s/%s&w=%d&h=%d",
	    dir,name,name,w,h);
    printf("<IFRAME name=\"bigImg\" width=\"100%%\" height=\"90%%\" SRC=\"%s\"></IFRAME><BR>\n", buf);

    fullCaption(conn, imageId);
    }
htmlEnd();
}

void doProbe(struct sqlConnection *conn)
/* Put up probe info page. */
{
int probeId = cartInt(cart, hgpDoProbe);
probePage(conn, probeId);
}

void doControls(struct sqlConnection *conn)
/* Put up controls pane. */
{
char *listSpec = cartUsualString(cart, hgpListSpec, "");
char *selected = NULL;
htmlSetBgColor(0xD0FFE0);
htmlSetStyle(htmlStyleUndecoratedLink);
htmStart(stdout, "do controls");
printf("<FORM ACTION=\"../cgi-bin/%s\" NAME=\"mainForm\" target=\"_parent\" METHOD=GET>\n",
	hgVisiGeneCgiName());
cartSaveSession(cart);

cgiMakeTextVar(hgpListSpec, listSpec, 16);
cgiMakeButton(hgpDoSearch, "search");

printf(" &nbsp; Zoom: ");
printf(
"<INPUT TYPE=SUBMIT NAME=\"hgp_zmOut\" VALUE=\" out \""
" onclick=\"parent.image.bigImg.zoomer('out');return false;\"> "
"<INPUT TYPE=SUBMIT NAME=\"hgp_zmIn\" VALUE=\" in \""
" onclick=\"parent.image.bigImg.zoomer('in');return false;\"> "
"<INPUT TYPE=SUBMIT NAME=\"hgp_zmFull\" VALUE=\" full \""
" onclick=\"parent.image.bigImg.zoomer('full');return false;\"> "
"<INPUT TYPE=SUBMIT NAME=\"hgp_zmFit\" VALUE=\" fit \""
" onclick=\"parent.image.bigImg.zoomer('fit');return false;\"> "
"\n");

printf("&nbsp;&nbsp;&nbsp;<RIGHT><A HREF=\"/index.html\" class=\"leftbar\" target=\"_parent\">Home</A></RIGHT>");
printf("</FORM>\n");
htmlEnd();
}

void weighMatches(struct sqlConnection *conn, struct visiMatch *matchList)
/* Set match field in match list according to priority, etc. */
{
struct visiMatch *match;
struct dyString *dy = dyStringNew(0);

/* Weigh matches by priority, and secondarily by age. */
for (match = matchList; match != NULL; match = match->next)
    {
    double priority;
    double age;

    /* Fetch priority. */
    dyStringClear(dy);
    dyStringPrintf(dy, 
    	"select imageFile.priority from imageFile,image "
	"where image.id = %d and image.imageFile = imageFile.id"
	, match->imageId);
    priority = sqlQuickDouble(conn, dy->string);

    /* Fetch age. */
    dyStringClear(dy);
    dyStringPrintf(dy,
        "select specimen.age from image,specimen "
	"where image.id = %d and image.specimen = specimen.id"
	, match->imageId);
    age = sqlQuickDouble(conn, dy->string);

    match->weight = -priority - age*0.0001;
    }
}

void doFrame(struct sqlConnection *conn, boolean forceImageToList)
/* Make a html frame page.  Fill frame with thumbnail, control bar,
 * and image panes. */
{
int imageId = cartUsualInt(cart, hgpId, 0);
char *sidUrl = cartSidUrlString(cart);
char *listSpec = cartUsualString(cart, hgpListSpec, "");
char *matchListFile;
struct slName *geneList, *gene;
struct tempName matchTempName;
char *matchFile = NULL;
struct visiMatch *matchList = visiSearch(conn, listSpec);
matchList = onePerImageFile(conn, matchList);
weighMatches(conn, matchList);
slSort(&matchList, visiMatchCmpWeight);
if (forceImageToList)
    {
    if (matchList != NULL)
	imageId = matchList->imageId;
    else
        imageId = 0;
    }
makeTempName(&matchTempName, "visiMatch", ".tab");
matchFile = matchTempName.forCgi;
saveMatchFile(matchFile, matchList);
cartSetString(cart, hgpMatchFile, matchFile);
cartSetInt(cart, hgpId, imageId);
puts("\n");
puts("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">");
printf("<HTML>\n");
printf("<HEAD>\n");
printf("<TITLE>\n");
printf("%s ", hgVisiGeneShortName());
if (imageId != 0)
    {
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
    }
printf("</TITLE>\n");
printf("</HEAD>\n");

printf("<frameset cols=\"230,*\"> \n");
printf("  <frame src=\"../cgi-bin/%s?%s=go&%s&%s=%d\" noresize frameborder=\"0\" name=\"list\">\n",
    hgVisiGeneCgiName(), hgpDoThumbnails, sidUrl, hgpId, imageId);
printf("  <frameset rows=\"30,*\">\n");
printf("    <frame name=\"controls\" src=\"../cgi-bin/%s?%s=go&%s&%s=%d\" noresize marginwidth=\"0\" marginheight=\"0\" frameborder=\"0\">\n",
    hgVisiGeneCgiName(), hgpDoControls, sidUrl, hgpId, imageId);
printf("    <frame src=\"../cgi-bin/%s?%s=go&%s&%s=%d/\" name=\"image\" noresize frameborder=\"0\">\n",
    hgVisiGeneCgiName(), hgpDoImage, sidUrl, hgpId, imageId);
printf("  </frameset>\n");
printf("  <noframes>\n");
printf("  <body>\n");
printf("  <p>This web page uses frames, but your browser doesn't support them.</p>\n");
printf("  </body>\n");
printf("  </noframes>\n");
printf("</frameset>\n");


printf("</HTML>\n");
}

void doHelp()
/* Print help section */
{
puts(
"<P> \n"
"VisiGene is a browser for viewing <em>in situ</em> images. \n"
"It enables the user to examine cell-by-cell as well as tissue-by-tissue \n"
"expression patterns. The browser serves as a virtual microscope, allowing \n" 
"users to retrieve images that meet specific search criteria, then \n"
"interactively zoom and scroll across the collection.</P>\n"
"<H3>Images Available</H3>\n"
"<P> \n"
"The following image collections are currently available for browsing: \n"
"<UL> \n"
"<LI>Mouse <em>in situ</em> images from the \n"
"<A HREF=\"http://www.informatics.jax.org/menus/expression_menu.shtml\" \n"
"TARGET=_blank>Jackson Labs Gene Expression Database</A> (GED) \n" 
"<LI>Transcription factors in mouse embryos from the \n"
"<A HREF=\"http://mahoney.chip.org/mahoney/\" TARGET=_blank>Mahoney Center \n"
"for Neuro-Oncology</A> \n"
"<LI><em>Xenopus laevis</em> <em>in situ</em> images from the \n"
"<A HREF=\"http://www.nibb.ac.jp/en/index.php\" TARGET=_blank>National \n"
"Institute for Basic Biology</A> (NIBB) XDB project \n"
"</UL> \n"
"<H3>Searching the Image Database</H3>\n"
"<P> \n"
"The image database may be searched by organism names, gene names, authors, \n"
"body parts, year of publication, GenBank or UniProt accessions, \n"
"<A HREF=\"http://genex.hgu.mrc.ac.uk/Atlas/intro.html\" \n"
"TARGET=_blank>Theiler stages</A>, and \n"
"<A HREF=\"http://www.xenbase.org/atlas/NF/NF-all.html\" \n"
"TARGET=_blank>Nieuwkoop/Faber stages</A>. \n"
"The search returns only those images that match all the specified criteria. \n"
"<P> \n"
"The wildcard characters * and ? are supported for gene name searches. \n"
"For example, to view the images of all genes in the Hox A cluster, search \n"
"for <em>hoxa*</em>. \n"  
"When searching on author names that include initials, \n"
"use the format <em>Smith AJ</em>. </P>\n"
"<H3>Image Navigation</H3>\n"
"<P> \n"
"Following a successful search, VisiGene displays a list of thumbnails of \n"
"images matching the search criteria \n"
"in the lefthand pane of the browser. By default, the image corresponding to \n"
"the first thumbnail in the list is displayed in the main image pane. \n"
"If more than 25 images meet the search criteria, links at the bottom of \n"
"the thumbnail pane allow the user to toggle among pages of search results. \n"
"To display a different image in the main browser pane, \n"
"click the thumbnail of the image you wish to view. </P>\n"
"<P> \n"
"By default, an image is displayed at a resolution that provides \n"
"optimal viewing of the overall image. This size varies among images. \n"
"The image may be zoomed in or out, and the viewing area may be moved or \n"
"scrolled in any direction to focus on areas of interest. \n"
"<P> \n"
"<B>Zooming in:</B> Set the Zoom option above the image to <em>in</em>, \n"
"then click on the image. Each click repetition enlarges the image 2X and \n"
"centers the image on the location of the mouse click. Keyboard shortcut: \n"
"Click on the image, then use the <em>+</em> key.</P>\n"
"<P> \n"
"<B>Zooming out:</B> Set the Zoom option above the image to <em>out</em>, \n"
"then click on the image. Each click repetition reduces the image 2X and \n"
"centers the image on the location of the mouse click.\n"
"Click on the image, then use the <em>-</em> key. </P> \n"
"<P> \n"
"<B>Moving the image:</B> To move the image viewing area in any direction, \n"
"click and drag the image using the mouse. Alternatively, the following \n"
"keyboard shortcuts may be used after clicking on the image: \n"
"<UL>"
"<LI><B>Scroll left in the image: </B>Left-arrow key or <em>Home</em> key \n"
"<LI><B>Scroll right in the image: </B>Right-arrow key or <em>End</em> key \n"
"<LI><B>Scroll up in the image: </B>Up-arrow key or <em>PgUp</em> key \n"
"<LI><B>Scroll down in the image: </B>Down-arrow key or <em>PgDn</em> key \n"
"</UL></P> \n"
"<H3>Credits</H3>\n"
"<P> \n"
"VisiGene was written by Jim Kent with assistance from Galt Barber. \n"
"Contact <A HREF=\"mailto:kent@soe.ucsc.edu\">Jim</A> if you have \n"
"an image set you'd like to contribute for display. </P>\n"
);
}

void doInitialPage()
/* Put up page with search box that explains program and
 * some good things to search on. */
{
char *listSpec = NULL;
webStartWrapper(cart, "VisiGene Image Browser", NULL, FALSE, FALSE);
printf("<FORM ACTION=\"../cgi-bin/%s\" METHOD=GET>\n",
	hgVisiGeneCgiName());
listSpec = cartUsualString(cart, hgpListSpec, "");
cgiMakeTextVar(hgpListSpec, listSpec, 30);
cgiMakeButton(hgpDoSearch, "search");
printf("<BR>\n");
puts(
"<P> \n"
"Valid search terms include organism names, gene names, authors, body parts, \n"
"year of publication, GenBank or UniProt accessions, Theiler stages, and \n"
"Nieuwkoop/Faber stages. \n"
"See &quot;About VisiGene&quot; below for more \n"
"information about the VisiGene browser, search options, and image \n"
"navigation. </P> \n" 
);
printf("</FORM>\n");
webNewSection("About VisiGene");
doHelp();
webEnd();
}

void doDefault(struct sqlConnection *conn, boolean newSearch)
/* Put up default page - if there is no specific do variable. */
{
char *listSpec = cartUsualString(cart, hgpListSpec, "");
listSpec = skipLeadingSpaces(listSpec);
if (listSpec[0] == 0)
    doInitialPage();
else
    doFrame(conn, newSearch);
}

void doId(struct sqlConnection *conn)
/* Set up Gene Pix on given ID. */
{
int id = cartInt(cart, hgpDoId);
struct slName *genes = visiGeneGeneName(conn, id);
if (genes == NULL)
    {
    cartRemove(cart, hgpListSpec);
    cartRemove(cart, hgpId);
    }
else
    {
    cartSetInt(cart, hgpId, id);
    cartSetString(cart, hgpListSpec, genes->name);
    }
slFreeList(&genes);
doDefault(conn, FALSE);
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
else if (cartVarExists(cart, hgpDoProbe))
    doProbe(conn);
else if (cartVarExists(cart, hgpDoControls))
    doControls(conn);
else if (cartVarExists(cart, hgpDoId))
    doId(conn);
else if (cartVarExists(cart, hgpDoSearch))
    doDefault(conn, TRUE);
else 
    {
    char *oldListSpec = hashFindVal(oldCart, hgpListSpec);
    char *newListSpec = cartOptionalString(cart, hgpListSpec);
    boolean isNew = differentStringNullOk(oldListSpec, newListSpec);
    doDefault(conn, isNew);
    }
cartRemovePrefix(cart, hgpDoPrefix);
}

void doMiddle(struct cart *theCart)
/* Save cart to global, print time, call dispatch */
{
cart = theCart;
// visiConnCache = sqlNewConnCache(visiDb);
dispatch();
// sqlFreeConnCache(&visiConnCache);
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
