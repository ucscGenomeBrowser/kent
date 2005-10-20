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
#include "captionElement.h"
#include "visiSearch.h"
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
return "VisiGene";
}

char *hgVisiGeneCgiName()
/* Return name of executable. */
{
return "hgVisiGene";
}

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

static struct slName *geneColorName(struct sqlConnection *conn, int id)
/* Get list of genes.  If more than one in list
 * put color of gene if available. */
{
struct slName *list = NULL, *el = NULL;
list = visiGeneGeneName(conn, id);
if (slCount(list) > 1)
    {
    char query[512], llName[256];
    struct sqlResult *sr;
    char **row;

    slFreeList(&list);	/* We have to do it the complex way. */

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
	  "select gene.name,gene.locusLink,gene.refSeq,gene.genbank,gene.uniProt,probeColor.name"
	  " from imageProbe,probe,gene"
	  " LEFT JOIN probeColor on imageProbe.probeColor = probeColor.id"
	  " where imageProbe.image = %d"
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
	    if (color)
		safef(buf, sizeof(buf), "%s (%s)", geneName, color);
	    else
	        safef(buf, sizeof(buf), "%s", geneName);
	    el = slNameNew(buf);
	    slAddHead(&list, el);
	    }
	}
    sqlFreeResult(&sr);
    slReverse(&list);
    }
return list;
}

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

static char *shortOrgName(char *binomial)
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

static char *genomeDbForImage(struct sqlConnection *conn, int imageId)
/* Return the genome database to associate with image or NULL if none. */
{
char *db;
char *scientificName = visiGeneOrganism(conn, imageId);
struct sqlConnection *cConn = hConnectCentral();
char query[256];

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

struct slName *vgImageFileGenes(struct sqlConnection *conn, int imageFile)
/* Return alphabetical list of all genes associated with image file. */
{
struct hash *uniqHash = hashNew(0);
struct slInt *imageList, *image;
struct slName *geneList = NULL, *gene;
char *name;

imageList = visiGeneImagesForFile(conn, imageFile);
for (image = imageList; image != NULL; image = image->next)
    {
    struct slName *oneList, *next;
    oneList = visiGeneGeneName(conn, image->val);
    for (gene = oneList; gene != NULL; gene = next)
        {
	next = gene->next;
	if (hashLookup(uniqHash, gene->name))
	    freeMem(gene);
	else
	    {
	    hashAdd(uniqHash, gene->name, NULL);
	    slAddHead(&geneList, gene);
	    }
	}
    }
slFreeList(&imageList);
hashFree(&uniqHash);
slSort(&geneList, slNameCmp);
return geneList;
}

static void smallCaption(struct sqlConnection *conn, int imageId)
/* Write out small format caption. */
{
struct slName *geneList, *gene;
int imageFile = visiGeneImageFile(conn, imageId);
printf("%s", shortOrgName(visiGeneOrganism(conn, imageId)));
geneList = vgImageFileGenes(conn, imageFile);
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    printf(" %s", gene->name);
    }
slFreeList(&geneList);
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
int maxCount = 50, count = 0;
int startAt = cartUsualInt(cart, hgpStartAt, 0);
int imageCount = 0, fullCount = 0, partCount = 0;

htmlSetBgColor(0xC0C0D6);
htmStart(stdout, "doThumbnails");
matchList = readMatchFile(matchFile);
imageCount = slCount(matchList);
if (imageCount > 0)
    {
    double bestWeight = matchList->weight;
    boolean didLine = FALSE;
    for (match = matchList; match != NULL; match = match->next)
        {
	if (match->weight == bestWeight)
	    fullCount += 1;
	else
	    partCount += 1;
	}
        
    printf("<TABLE>\n");
    printf("<TR><TD><B>");
    if (fullCount == imageCount)
	printf("%d images match<BR>\n", fullCount);
    else
        printf("%d full %d partial match<BR>\n", fullCount, partCount);
    printf("</B></TD></TR>\n");
    for (match = slElementFromIx(matchList, startAt); 
	    match != NULL; match = match->next)
	{
	int id = match->imageId;
	char *imageFile = visiGeneThumbSizePath(conn, id);
	if (match->weight < bestWeight && !didLine)
	    {
	    printf("<TR><TD><HR></TD></TR>\n");
	    printf("<TR BGCOLOR=\"#D0FFE0\"><TD><B>partial matches:</B></TD></TR>\n");
	    didLine = TRUE;
	    }
	printf("<TR>");
	printf("<TD>");
	printf("<A HREF=\"../cgi-bin/%s?%s&%s=%d&%s=do\" target=\"image\" "
	    "onclick=\"parent.controls.document.mainForm.hgp_zm[0].checked=true;return true;\" "
	    ">", hgVisiGeneCgiName(), 
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


static char *makeCommaSpacedList(struct slName *list)
/* Turn linked list of strings into a single string with
 * elements separated by a comma and a space. */
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


static struct slName *expTissuesForProbeInImage(struct sqlConnection *conn, 
	int imageId,
	int probeId)
/* Get list of tissue where we have expression info in gene.
 * Put + or - depending on expression level. */
{
struct dyString *dy = dyStringNew(0);
struct slName *tissueList = NULL, *tissue;
char query[512], **row;
struct sqlResult *sr;
safef(query, sizeof(query),
   "select bodyPart.name,expressionLevel.level "
   "from expressionLevel,bodyPart,imageProbe "
   "where imageProbe.image = %d "
   "and imageProbe.probe = %d "
   "and imageProbe.id = expressionLevel.imageProbe "
   "and expressionLevel.bodyPart = bodyPart.id "
   "order by bodyPart.name"
   , imageId, probeId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    double level = atof(row[1]);
    dyStringClear(dy);
    dyStringAppend(dy, row[0]);
    if (level >= 0.1)
       dyStringAppend(dy, "(+)");
    else 
       dyStringAppend(dy, "(-)");
    tissue = slNameNew(dy->string);
    slAddHead(&tissueList, tissue);
    }
sqlFreeResult(&sr);
slReverse(&tissueList);
dyStringFree(&dy);
return tissueList;
}

struct expInfo 
/* Info on expression in various tissues for a particular gene. */
    {
    struct expInfo *next;	/* Next in list. */
    int probeId;		/* Id in probe table. */
    int geneId;			/* Id in gene table. */
    char *geneName;		/* Name of gene. */
    struct slName *tissueList;  /* List of tissues where gene expressed */
    };

static void expInfoFree(struct expInfo **pExp)
/* Free up memory associated with expInfo. */
{
struct expInfo *exp = *pExp;
if (exp != NULL)
    {
    freeMem(exp->geneName);
    slFreeList(&exp->tissueList);
    freez(pExp);
    }
}

static void expInfoFreeList(struct expInfo **pList)
/* Free a list of dynamically allocated expInfo's */
{
struct expInfo *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    expInfoFree(&el);
    }
*pList = NULL;
}


static struct expInfo *expInfoForImage(struct sqlConnection *conn, int imageId)
/* Return list of expression info for a given pane. */
{
struct dyString *dy = dyStringNew(0);
struct expInfo *expList = NULL, *exp;
char query[512], **row;
struct sqlResult *sr;

/* Get list of probes associated with image, and start building
 * expList around it. */
safef(query, sizeof(query), 
   "select imageProbe.probe,probe.gene from imageProbe,probe "
   "where imageProbe.image = %d "
   "and imageProbe.probe = probe.id", imageId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(exp);
    exp->probeId = sqlUnsigned(row[0]);
    exp->geneId = sqlUnsigned(row[1]);
    slAddHead(&expList, exp);
    }
sqlFreeResult(&sr);
slReverse(&expList);

/* Finish filling in expInfo and return. */
for (exp = expList; exp != NULL; exp = exp->next)
    {
    exp->geneName = vgGeneNameFromId(conn, exp->geneId);
    exp->tissueList = expTissuesForProbeInImage(conn, imageId, exp->probeId);
    }

return expList;
}

static void addExpressionInfo(struct sqlConnection *conn, int imageId,
	struct captionElement **pCeList)
/* Add information about expression to caption element list.
 * Since the pane can contain multiple genes each with
 * separate caption information this is more complex than
 * the other caption elements.  We'll make one captionElement
 * for each gene that we have expression info on. */
{
int geneWithDataCount = 0, expCount = 0;
struct expInfo *exp, *expList = expInfoForImage(conn, imageId);
struct captionElement *ce;

/* Figure out how much data we really have.  */
for (exp = expList; exp != NULL; exp = exp->next)
    {
    if (exp->tissueList != NULL)
        ++geneWithDataCount;
    ++expCount;
    }

/* If no data just add n/a */
if (geneWithDataCount == 0)
    {
    ce = captionElementNew(imageId, "expression", cloneString("n/a"));
    slAddHead(pCeList, ce);
    }
else
    {
    for (exp = expList; exp != NULL; exp = exp->next)
        {
	if (exp->tissueList != NULL)
	    {
	    char label[256];
	    if (expCount == 1)
	        safef(label, sizeof(label), "expression");
	    else
	        safef(label, sizeof(label), "expression of %s", exp->geneName);
	    ce = captionElementNew(imageId, label, 
	    	makeCommaSpacedList(exp->tissueList));
	    slAddHead(pCeList, ce);
	    }
	}
    }
}

static struct captionElement *makePaneCaptionElements(
	struct sqlConnection *conn, struct slInt *imageList)
/* Make list of all caption elements */
{
struct slInt *image;
struct captionElement *ceList = NULL, *ce;
for (image = imageList; image != NULL; image = image->next)
    {
    int paneId = image->val;
    struct slName *geneList = geneColorName(conn, paneId);
    struct slName *genbankList = visiGeneGenbank(conn, paneId);
    ce = captionElementNew(paneId, "gene", makeCommaSpacedList(geneList));
    slAddHead(&ceList, ce);
    ce = captionElementNew(paneId, "genbank", makeCommaSpacedList(genbankList));
    slAddHead(&ceList, ce);
    ce = captionElementNew(paneId, "organism", visiGeneOrganism(conn, paneId));
    slAddHead(&ceList, ce);
    ce = captionElementNew(paneId, "stage", visiGeneStage(conn, paneId, TRUE));
    slAddHead(&ceList, ce);
    ce = captionElementNew(paneId, "body part",
    	naForNull(visiGeneBodyPart(conn, paneId)));
    slAddHead(&ceList, ce);
    addExpressionInfo(conn, paneId, &ceList);
    ce = captionElementNew(paneId, "section type",
    	naForNull(visiGeneSliceType(conn, paneId)));
    slAddHead(&ceList, ce);
    ce = captionElementNew(paneId, "permeablization",
    	naForNull(visiGenePermeablization(conn, paneId)));
    }
slReverse(&ceList);
return ceList;
}

static void printCaptionElements(struct sqlConnection *conn, 
	struct captionElement *captionElements, struct slInt *imageList)
/* Print out caption elements - common elements first and then
 * pane-specific ones. */
{
struct captionBundle *bundleList, *bundle;
struct slRef *ref;
struct captionElement *ce;
int bundleCount;
int maxCharsInLine = 80;

bundleList = captionElementBundle(captionElements, imageList);
bundleCount = slCount(bundleList);
for (bundle = bundleList; bundle != NULL; bundle = bundle->next)
    {
    int charsInLine = 0, charsInEl;
    if (bundleCount > 1)
	printf("<HR>\n");
    if (bundle->image != 0)
        {
	char *label = visiGenePaneLabel(conn, bundle->image);
	if (label != NULL)
	   printf("<B>Pane %s</B><BR>\n", label);
	}
    else if (bundleCount > 1)
        {
	printf("<B>All Panes</B><BR>\n");
	}
    for (ref = bundle->elements; ref != NULL; ref = ref->next)
        {
	ce = ref->val;
	charsInEl = strlen(ce->type) + strlen(ce->value) + 2;
	if (charsInLine != 0)
	    {
	    if (charsInLine + charsInEl > maxCharsInLine )
		{
		printf("<BR>\n");
		charsInLine = 0;
		}
	    else if (charsInLine != 0)
		printf(" ");
	    }
	printf("<B>%s:</B> %s", ce->type, ce->value);
	charsInLine += charsInEl;
	}
    if (charsInLine != 0)
	printf("<BR>\n");
    }
if (bundleCount > 1)
    printf("<HR>\n");
}

static void fullCaption(struct sqlConnection *conn, int id)
/* Print information about image. */
{
char query[256];
char **row;
char *permeablization, *publication, *copyright, *acknowledgement;
char *setUrl, *itemUrl;
char *caption = visiGeneCaption(conn, id);
struct slName *geneList;
int imageFile = visiGeneImageFile(conn, id);
struct slInt *imageList, *image;
int imageCount, imageIx=0;
struct captionElement *captionElements;

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
imageList = visiGeneImagesForFile(conn, imageFile);
imageCount = slCount(imageList);
captionElements = makePaneCaptionElements(conn, imageList);
printCaptionElements(conn, captionElements, imageList);

printf("<B>year:</B> %d ", visiGeneYear(conn,id));
printf("<B>contributors:</B> %s<BR>\n", 
	naForNull(visiGeneContributors(conn,id)));
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
int imageId = cartUsualInt(cart, hgpId, 0);
char buf[1024];
char *p = NULL;
char dir[256];
char name[128];
char extension[64];
int w = 0, h = 0;
htmlSetBgColor(0xE0E0E0);
htmStart(stdout, "do image");

if (imageId == 0)
    {
    printf("Click on thumbnail image in the list to the left to see a larger, "
           "zoomable image in this space.");
    }
else
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

void doControls(struct sqlConnection *conn)
/* Put up controls pane. */
{
char *listSpec = cartUsualString(cart, hgpListSpec, "");
char *selected = NULL;
htmlSetBgColor(0xD0FFE0);
htmStart(stdout, "do controls");
printf("<FORM ACTION=\"../cgi-bin/%s\" NAME=\"mainForm\" target=\"_parent\" METHOD=GET>\n",
	hgVisiGeneCgiName());
cartSaveSession(cart);

cgiMakeTextVar(hgpListSpec, listSpec, 10);
cgiMakeButton(hgpDoSearch, "search");


printf(" Zoom: ");
printf("<INPUT TYPE=RADIO NAME=hgp_zm onclick=\"parent.image.bigImg.changeMouse('in');return true;\" CHECKED>in ");
printf("<INPUT TYPE=RADIO NAME=hgp_zm onclick=\"parent.image.bigImg.changeMouse('out');return true;\">out ");
printf("\n");

printf("</FORM>\n");
htmlEnd();
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
if (forceImageToList && matchList != NULL)
    imageId = matchList->imageId;
matchList = onePerImageFile(conn, matchList);
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
"You can search on gene names, authors, body parts, years, "
"GenBank or UniProt accessions.  You can include multiple "
"search terms.  The images matching the most terms will be "
"at the top of the results list. For gene names you can include "
"* and ? wildcard characters.  For instance to see images of all "
"genes in the Hox A cluster search for hoxa*.  When searching on "
"author names you can include the initials after the last name.");
printf("</FORM>\n");
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
