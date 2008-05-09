/* hgVisiGene - Gene Picture Browser. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "net.h"
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
#include "configPage.h"
#include "printCaption.h"
#include "trashDir.h"

/* Globals */
struct cart *cart;		/* Current CGI values */
struct hash *oldCart;		/* Previous CGI values */
char *visiDb = VISIGENE;	/* The visiGene database name */
struct sqlConnCache *visiConnCache;  /* Cache of connections to database. */

char *hgVisiGeneShortName()
/* Return short descriptive name (not cgi-executable name)
 * of program */
{
return "VisiGene";
}

char *hgVisiGeneCgiName()
/* Return name of executable. */
{
return "../cgi-bin/hgVisiGene";
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
struct visiMatch *matchList = NULL, *match;
struct lineFile *lf = lineFileMayOpen(fileName, TRUE);
if (lf != NULL)
    {
    char *row[2];
    while (lineFileRow(lf, row))
	{
	AllocVar(match);
	match->imageId = lineFileNeedNum(lf, row, 0);
	match->weight = lineFileNeedDouble(lf, row, 1);
	slAddHead(&matchList, match);
	}
    lineFileClose(&lf);
    slReverse(&matchList);
    }
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

htmlSetStyle(
"<STYLE TYPE=\"text/css\">\n"
"  BODY {margin: 2px}\n"
"  </STYLE>\n"
);
htmlSetBgColor(0xC0C0D6);
htmStart(stdout, "doThumbnails");
matchList = readMatchFile(matchFile);
imageCount = slCount(matchList);
if (imageCount > 0)
    {
    printf(
"    <DIV"
"    ID='perspClip'"
"    STYLE='position:absolute;left:-1000px;top:-1000px;z-index:2;visibility:visible;overflow:hidden!important;'"
"         onmouseover='this.style.left=-1000;' "
"    >"
"    <DIV"
"    ID='perspective'"
"    STYLE='position:absolute;left:0px;top:0px;'"
"    >"
"    <IMG ID='perspBox' SRC='../images/dot_clear.gif' BORDER=2 HEIGHT=100 WIDTH=100 STYLE='position:absolute;left:0px;top:0px;border-color:#8080FF;border-width:1' "
">"
"    </DIV>"
"    </DIV>"
    );
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
	printf("<A HREF=\"%s?%s&%s=%d&%s=do\" target=\"image\" >", 
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
char *sidUrl = cartSidUrlString(cart);
char buf[1024];
char url[1024];
char *p = NULL;
char dir[256];
char name[128];
char extension[64];
int w = 0, h = 0;
htmlSetBgColor(0xE0E0E0);
htmStart(stdout, "do image");

if (!visiGeneImageSize(conn, imageId, &w, &h))
    imageId = 0;
	
if (imageId != 0)
    {
    printf("<B>");
    smallCaption(conn, imageId);
    printf(".</B> Click image to zoom in, drag or arrow keys to move. "
	   "Caption is below.<BR>\n");

    p=visiGeneFullSizePath(conn, imageId);

    splitPath(p, dir, name, extension);
#ifdef DEBUG
    safef(buf,sizeof(buf),"../bigImageTest.html?url=%s%s/%s&w=%d&h=%d",
	    dir,name,name,w,h);
#else	    
    safef(buf,sizeof(buf),"../bigImage.html?url=%s%s/%s&w=%d&h=%d",
	    dir,name,name,w,h);
#endif	    
    printf("<IFRAME name=\"bigImg\" width=\"100%%\" height=\"90%%\" SRC=\"%s\"></IFRAME><BR>\n", buf);

    fullCaption(conn, imageId);

    safef(buf,sizeof(buf),"%s%s%s", dir, name, extension);
    safef(url,sizeof(url),"%s?%s=go&%s&%s=%d",
    	hgVisiGeneCgiName(), hgpDoDownload, sidUrl, hgpId, imageId);
   
    printf("<B>Full-size image:</B> %d x %d &nbsp; <A HREF='%s'> download </A> ", w, h, url);

    /* Currently this is dangerous for users with less than 1 GB RAM to use 
       on large images, because their machines can thrash themselves into a coma.
       X-windows (i.e. used by FireFox) will allocate 5 bytes per pixel.
       If the image size in pixels times 5 exceeds real ram size, then
       Linux thrashes incessantly.  But you can hit ctrl-alt-F1 to 
       get a text only screen, then kill the bad processes (FF) and then
       you can restore desktop with ctrl-alt-F7.  Hiram says that's a
       feature credited to SCO-Unix.  On my 1GB machines at work/home,
       I never encountered any problem what-so-ever, even with the 
       largest visiGene AllenBrain - about 19000x9000 pix.
       
    printf(" &nbsp;&nbsp; <A HREF='%s'> view </A>\n", buf);
    */
    printf("\n");
    
    }
htmlEnd();
}

void doProbe(struct sqlConnection *conn)
/* Put up probe info page. */
{
int probeId = cartInt(cart, hgpDoProbe);
int submissionSourceId = cartInt(cart, hgpSs);
cartRemove(cart, hgpSs);
probePage(conn, probeId, submissionSourceId);
}

void doControls(struct sqlConnection *conn)
/* Put up controls pane. */
{
char *listSpec = cartUsualString(cart, hgpListSpec, "");
htmlSetBgColor(0xD0FFE0);
htmlSetStyle("	<LINK REL=\"STYLESHEET\" HREF=\"../style/HGStyle.css\">" "\n");
htmStart(stdout, "do controls");
printf("<FORM ACTION=\"%s\" NAME=\"mainForm\" target=\"_parent\" METHOD=GET>\n",
	hgVisiGeneCgiName());
cartSaveSession(cart);

printf("<TABLE WIDTH=100%%><TR>");
printf("<TD WIDTH=230 bgcolor=\"#"HG_COL_HOTLINKS"\">");
printf("&nbsp;<B><A HREF=\"../index.html\" target=\"_parent\" class=\"topbar\">UCSC</A> ");
printf("&nbsp;<A HREF=\"%s?%s=\" target=\"_parent\" class=\"topbar\">VisiGene</A></B> ",
	hgVisiGeneCgiName(), hgpListSpec);
printf("</TD>");

printf("<TD>");
printf("&nbsp");
cgiMakeTextVar(hgpListSpec, listSpec, 16);
cgiMakeButton(hgpDoSearch, "search");
printf("</TD>");

#ifdef SOON
printf("<TD>");
cgiMakeButton(hgpDoConfig, "configure");
printf("</TD>");
#endif /* SOON */

printf("<TD>");
printf("Zoom: ");
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
printf("</TD>");

printf("<TD ALIGN=Right>");
printf("<A HREF=\"../goldenPath/help/hgTracksHelp.html#VisiGeneHelp\" target=_parent>");
printf("Help");
printf("</A>&nbsp;");
printf("</TD>");
printf("</TR></TABLE>");
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

#ifdef SOON
struct visiMatch *removeMutants(struct sqlConnection *conn,
	struct visiMatch *matchList)
/* Remove images that are associated with mutant genotypes
 * from list. */
{
struct visiMatch *match, *next, *newList = NULL;
for (match = matchList; match != NULL; match = next)
    {
    char *genotype;
    char query[256];
    next = match->next;
    safef(query, sizeof(query),
        "select genotype.alleles from image,specimen,genotype "
	"where image.id=%d and image.specimen=specimen.id "
	"and specimen.genotype=genotype.id", match->imageId);
    genotype = sqlQuickString(conn, query);
    if (genotype == NULL || genotype[0] == 0 || 
    	startsWith("wild type", genotype))
	{
	slAddHead(&newList, match);
	}
    freeMem(genotype);
    }
slReverse(&newList);
return newList;
}
#endif /* SOON */

void doFrame(struct sqlConnection *conn, boolean forceImageToList)
/* Make a html frame page.  Fill frame with thumbnail, control bar,
 * and image panes. */
{
int imageId = cartUsualInt(cart, hgpId, 0);
char *sidUrl = cartSidUrlString(cart);
char *listSpec = cartUsualString(cart, hgpListSpec, "");
struct tempName matchTempName;
char *matchFile = NULL;
struct visiMatch *matchList = visiSearch(conn, listSpec);
#ifdef SOON
if (!cartUsualBoolean(cart, hgpIncludeMutants, FALSE))
    matchList = removeMutants(conn, matchList);
#endif /* SOON */
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

trashDirFile(&matchTempName, "vg", "visiMatch", ".tab");

matchFile = matchTempName.forCgi;
saveMatchFile(matchFile, matchList);
cartSetString(cart, hgpMatchFile, matchFile);
cartSetInt(cart, hgpId, imageId);
//puts("\n");
puts("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">");
printf("<HTML>\n");
printf("<HEAD>\n");
printf("<TITLE>\n");
printf("%s ", hgVisiGeneShortName());
printf("%s",titleMessage);    
printf("</TITLE>\n");
printf("</HEAD>\n");


printf("  <frameset rows=\"27,*\">\n");
printf("    <frame name=\"controls\" src=\"%s?%s=go&%s&%s=%d\" noresize marginwidth=\"0\" marginheight=\"0\" frameborder=\"0\">\n",
    hgVisiGeneCgiName(), hgpDoControls, sidUrl, hgpId, imageId);
printf("  <frameset cols=\"230,*\"> \n");
printf("    <frame src=\"%s?%s=go&%s&%s=%d\" noresize frameborder=\"0\" name=\"list\">\n",
    hgVisiGeneCgiName(), hgpDoThumbnails, sidUrl, hgpId, imageId);
printf("    <frame src=\"%s?%s=go&%s&%s=%d\" name=\"image\" noresize frameborder=\"0\">\n",
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
"<H3>Images Available</H3>\n"
"<P> \n"
"The following image collections are currently available for browsing: \n"
"<UL> \n"
"<LI>High-quality high-resolution images of eight-week-old male mouse \n"
"sagittal brain slices with reverse-complemented mRNA hybridization probes \n"
"from the <A HREF=\"http://brain-map.org/\" TARGET=_blank>Allen Brain \n"
"Atlas</A>, courtesy of the <A HREF=\"http://www.alleninstitute.org/\" \n"
"TARGET=_blank>Allen Institute for Brain Science</A> \n"
"<LI>Mouse <em>in situ</em> images from the \n"
"<A HREF=\"http://www.informatics.jax.org/expression.shtml\" \n"
"TARGET=_blank>Jackson Lab Gene Expression Database</A> (GXD) at MGI \n" 
"<LI>Transcription factors in mouse embryos from the \n"
"<A HREF=\"http://mahoney.chip.org/mahoney/\" TARGET=_blank>Mahoney Center \n"
"for Neuro-Oncology</A> \n"
"<LI>Mouse head and brain <em>in situ</em> images from NCBI's  \n"
"<A HREF=\"http://www.ncbi.nlm.nih.gov/projects/gensat/\" TARGET=_blank>Gene \n"
"Expression Nervous System Atlas</A> (GENSAT) database \n"
"<LI><em>Xenopus laevis</em> <em>in situ</em> images from the \n"
"<A HREF=\"http://www.nibb.ac.jp/en/index.php\" TARGET=_blank>National \n"
"Institute for Basic Biology</A> (NIBB) XDB project \n"
"</UL> \n"
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
"The image may be zoomed in or out, sized to match the resolution of the \n"
"original image or best fit the image display window, and moved or \n"
"scrolled in any direction to focus on areas of interest. \n"
"<P> \n"
"<B>Zooming in:</B> To enlarge the image by 2X, click the Zoom <em>in</em> \n"
"button above the image or click on the image using the left mouse button. \n"
"Alternatively, the + key may be used to zoom in when the main image pane is \n"
"the active window.</P> \n"
"<P> \n"
"<B>Zooming out:</B> To reduce the image by 2X, click the Zoom <em>out</em> \n"
"button above the image or click on the image using the right mouse button. \n"
"Alternatively, the - key may be used to zoom out when the main image pane \n"
"is the active window.</P> \n"
"<P> \n"
"<B>Sizing to full resolution:</B> Click the Zoom <em>full</em> button above \n"
"the image to resize the image such that each pixel on the screen \n"
"corresponds to a pixel in the digitized image. </P> \n"
"<P> \n"
"<B>Sizing to best fit:</B> Click the Zoom <em>fit</em> button above \n"
"the image to zoom the image to the size that best fits the main image \n"
"pane. </P>\n"
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
"<B>Downloading the original full-sized image:</B> Most images may be viewed \n"
"in their original full-sized format by clicking the &quot;download&quot; \n"
"link at the bottom of the image caption. NOTE: due to the large size of \n"
"some images, this action may take a long time and could potentially exceed \n"
"the capabilities of some Internet browsers. </P> \n"
"<H3>Credits</H3>\n"
"<P> \n"
"VisiGene was written by Jim Kent and Galt Barber. \n"
"Contact <A HREF=\"mailto:kent@soe.ucsc.edu\">Jim</A> if you have \n"
"an image set you would like to contribute for display. </P>\n"
);
}


void doInitialPage()
/* Put up page with search box that explains program and
 * some good things to search on. */
{
char *listSpec = NULL;

webStartWrapperDetailedNoArgs(cart, "", "VisiGene Image Browser",
        FALSE, FALSE, FALSE, TRUE);

printf("<FORM ACTION=\"%s\" METHOD=GET>\n",
	hgVisiGeneCgiName());
puts("<P>VisiGene is a virtual microscope for viewing <em>in situ</em> images. \n"
"These images show where a gene is used in an organism, sometimes down to \n"
"cellular resolution. With VisiGene users can retrieve images that meet specific "
"search criteria, then interactively zoom and scroll across the collection.</P>\n");
printf("<CENTER>");
listSpec = cartUsualString(cart, hgpListSpec, "");
cgiMakeTextVar(hgpListSpec, listSpec, 30);
cgiMakeButton(hgpDoSearch, "search");
printf("<BR>\n");
printf("</CENTER>");
puts(
"<P>Good search terms include gene symbols, authors, years, body parts,\n"
"organisms, GenBank and UniProt accessions, Known Gene descriptive terms,\n"
"<A HREF=\"http://genex.hgu.mrc.ac.uk/Atlas/intro.html\" \n"
"TARGET=_blank>Theiler</A> stages for mice, and \n"
"<A HREF=\"http://www.xenbase.org/atlas/NF/NF-all.html\" \n"
"TARGET=_blank>Nieuwkoop/Faber</A> stages for frogs. The wildcard characters\n"
"* and ? work with gene symbols; otherwise the full word must match.</P>\n"
"<P> \n"
"<H3>Sample queries</H3> \n"
"<TABLE  border=0 CELLPADDING=0 CELLSPACING=0> \n"
"    <TR><TD VALIGN=Top NOWRAP><B>Request:</B><BR></TD> \n"
"        <TD VALIGN=Top COLSPAN=2><B>&nbsp;&nbsp; VisiGene Response:</B><BR></TD> \n"
"    </TR> \n"
// "    <TR><TD VALIGN=Top><BR></TD></TR> \n"
"    <TR><TD VALIGN=Top NOWRAP>nkx2-2</TD> \n"
"        <TD WIDTH=14></TD> \n"
"        <TD VALIGN=Top>Displays images associated with the gene nkx2-2</TD>\n"
"    </TR> \n"
"    <TR><TD VALIGN=Top NOWRAP>hoxa*</TD> \n"
"        <TD WIDTH=14></TD> \n"
"        <TD VALIGN=Top>Displays images of all genes in the Hox-A cluster (Note: * works only at the end of the word)</TD>\n"
"    </TR> \n"
"    <TR><TD VALIGN=Top NOWRAP>NM_007492</TD> \n"
"        <TD WIDTH=14></TD> \n"
"        <TD VALIGN=Top>Displays images associated with accession NM_007492</TD> \n"
"    </TR> \n"
"    <TR><TD VALIGN=Top NOWRAP>theiler 22</TD> \n"
"        <TD WIDTH=14></TD> \n"
"        <TD VALIGN=Top>Displays all images that show Theiler stage 22</TD> \n"
"    </TR> \n"
"    <TR><TD VALIGN=Top NOWRAP>vgPrb_16</TD> \n"
"        <TD WIDTH=14></TD> \n"
"        <TD VALIGN=Top>Displays images associated with VisiGene probe ID 16</TD> \n"
"    </TR> \n"
"    <TR><TD VALIGN=Top NOWRAP>allen institute</TD> \n"
"        <TD WIDTH=14></TD> \n"
"        <TD VALIGN=Top>Displays all images from the Allen Brain Atlas</TD> \n"
"    </TR> \n"
"    <TR><TD VALIGN=Top NOWRAP>mouse</TD> \n"
"        <TD WIDTH=14></TD> \n"
"        <TD VALIGN=Top>Displays all mouse images </TD> \n"
"    </TR> \n"
"    <TR><TD VALIGN=Top NOWRAP>xenopus</TD> \n"
"        <TD WIDTH=14></TD> \n"
"        <TD VALIGN=Top>Displays all images associated with frogs of genus Xenopus </TD>\n"
"    </TR> \n"
"    <TR><TD VALIGN=Top NOWRAP>mouse midbrain</TD> \n"
"        <TD WIDTH=14></TD> \n"
"        <TD VALIGN=Top>Displays mouse images that show expression in the "
"midbrain</TD> \n"
"    </TR> \n"
"    <TR><TD VALIGN=Top NOWRAP>smith jc 1994</TD> \n"
"        <TD WIDTH=14></TD> \n"
"        <TD VALIGN=Top>Displays images contributed by scientist J.C. Smith "
"in 1994</TD> \n"
"    </TR> \n"
"</TABLE> \n"
);
printf("</FORM>\n");
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


static void problemPage(char *msg, char *url)
/* send back a page describing problem */
{
printf("Content-Type: text/plain\n");
printf("\n");
htmStart(stdout, "do download");
printf("%s %s",msg,url);
htmlEnd();
}

static void doDownload(struct sqlConnection *conn)
/* Try to force user's browser to download by giving special response headers */
{
int imageId = cartUsualInt(cart, hgpId, 0);
char url[1024];
char *p = NULL;
char dir[256];
char name[128];
char extension[64];
int w = 0, h = 0;
int sd = -1;

if (!visiGeneImageSize(conn, imageId, &w, &h))
    imageId = 0;
	
if (imageId == 0)
    {
    problemPage("invalid imageId","");
    }
else    
    {
    p=visiGeneFullSizePath(conn, imageId);
    splitPath(p, dir, name, extension);
    safef(url,sizeof(url),"%s%s%s", dir, name, extension);
    sd = netUrlOpen(url);
    if (sd < 0)
	{
	problemPage("Couldn't open", url);
	}
    else
	{
	if (netSkipHttpHeaderLines(&sd, url))  /* url needed only for err msgs and redirect url*/
	    {
	    char buf[32*1024];
	    int readSize;
	    printf("Content-Type: application/octet-stream\n");
	    printf("Content-Disposition: attachment; filename=%s%s\n", name, extension);
	    printf("\n");
	    while ((readSize = read(sd, buf, sizeof(buf))) > 0)
	        fwrite(buf, 1,  readSize, stdout);
	    close(sd);
	    sd = -1;
	    fflush(stdout);
	    fclose(stdout);
	    }
	else
	    {
	    problemPage("Skip http header problem", url);
    	    }	
	}
    }
}

void dispatch()
/* Set up a connection to database and dispatch control
 * based on hgpDo type var. */
{
struct sqlConnection *conn = sqlConnect(visiDb);
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
#ifdef SOON
else if (cartVarExists(cart, hgpDoConfig))
    configPage(conn);
#endif /* SOON */
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
if (cgiVarExists(hgpDoDownload))  /* use cgiVars -- do not commit to any cart method yet */
    {
    struct sqlConnection *conn = sqlConnect(visiDb);
    cart = cartAndCookieNoContent(hUserCookie(), excludeVars, oldCart);
    doDownload(conn);
    cartCheckout(&cart);
    }
else
    {
    cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldCart);
    }
return 0;
}
