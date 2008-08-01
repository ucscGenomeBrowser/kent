/* phyloGif.c for parsing phylo tree and outputting a gif.
 *
 *  Author: Galt Barber 2006 
 * 
 *  Designed to be run either as a cgi or as a command-line utility.
 *  The input file spec matches the .nh tree format e.g. (cat:0.5,dog:0.6):1.2;
 *  The output gif layout was originally designed to ignore branch lengths.
 *  However, options have now been added to allow it to use branchlengths,
 *  and to label the length of the branches.
 *  Any _suffix is stripped from labels both because pyloTree.c 
 *  can't tolerate underscores and because we don't want suffixes anyway.
 *  A semi-colon is automatically appended to the input if left off.
 *  Because phyloTree.c does errAbort on bad input, this causes cgi err 500
 *  if the input data has incorrect syntax.  See the apache error_log.
 *  Added another option to place form output in a static html page
 *  so that we can prevent IE6 save-as bug, and FF auto-shrunk output.
 *  Added an option to display a length-scale legend or ruler at the bottom.
 *  Added an option to preserve underscores in input as spaces in output.
 *  
 *
 *  One may use as a cgi in html GET: 
 *    <IMG SRC="../cgi-bin/phyloGif?phyloGif_width=120&phyloGif_height=120&phyloGif_tree=(a:1,b:1);" >
 *
 *  Or as cgi in html POST:
<form method="POST" action="../cgi-bin/phyloGif/phylo.gif" name="mainForm">
<table>
<tr><td>width:</td><td><INPUT type="text" name="phyloGif_width" value="240" size="4"></td></tr>
<tr><td>height:</td><td><INPUT type="text" name="phyloGif_height" value="512" size="4"></td></tr>
<tr><td>tree:</td><td><textarea name="phyloGif_tree" rows=14 cols=70>
(((((((((
(human_hg18:0.00669,chimp_panTro1:0.00757):0.0243,
  macaque_rheMac2:0.0592):0.0240,
    ((rat_rn4:0.0817,mouse_mm8:0.0770):0.229,
          rabbit_oryCun1:0.207):0.107):0.0230,
	  (cow_bosTau2:0.159,dog_canFam2:0.148):0.0394):0.0285,
	  armadillo_dasNov1:0.150):0.0160,
	  (elephant_loxAfr1:0.105,tenrec_echTel1:0.260):0.0404):0.218,
	  monodelphis_monDom4:0.371):0.189,
	  chicken_galGal2:0.455):0.123,
	  xenopus_xenTro1:0.782):0.156,
	  ((tetraodon_tetNig1:0.199,fugu_fr1:0.239):0.493,
	      zebrafish_danRer3:0.783):0.156);
</textarea></td></tr>
<tr><td>&nbsp;</td><td><INPUT type="submit" name="phyloGif_submit" value="submit"></td></tr>
</table>
</form>
 
 */

#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "net.h"
#include "cheapcgi.h"
#include "errabort.h"
#include "phyloTree.h"
#include "portable.h"
#include "memgfx.h"

#include "cart.h"
#include "hui.h"
#include "htmshell.h"
#include "web.h"


#include "errabort.h"
#include "errCatch.h"

static char const rcsid[] = "$Id: phyloGif.c,v 1.17.58.1 2008/08/01 06:10:54 markd Exp $";

struct cart *cart=NULL;      /* The user's ui state. */
struct hash *oldVars = NULL;
boolean onWeb = FALSE;

int width=240,height=512;
boolean branchLengths = FALSE;  /* branch lengths */
boolean lengthLegend = FALSE;   /* length ruler*/
boolean branchLabels = FALSE;   /* labelled branch lengths */
boolean htmlPageWrapper = FALSE;  /* wrap output in an html page */
boolean preserveUnderscores = FALSE;   /* preserve underscores in input as spaces in output */
int branchDecimals = 2;         /* show branch label length to two decimals by default */
int branchMultiplier = 1;         /* multiply branch length by factor */
char *escapePattern = NULL;      /* use to escape dash '-' char in input */
char layoutErrMsg[1024] = "";

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", "phyloGif_submit", "phyloGif_restore", NULL};

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort(
    "%s\n\n"
    "phyloGif - parse and display phyloGenetic tree\n"
    "\n"
    "Command-line Usage Examples:\n"
    "   phyloGif -phyloGif_tree=tree.nh [options] > phylo.gif \n"
    "   phyloGif -phyloGif_tree='(A:0.1,B:0.1)' [options] > phylo.gif \n"
    "CGI Usage Examples:\n"
    "   Create an interactive form:\n"
    "     http://someserver.com/cgi-bin/phyloGif\n"
    "   Create an image dynamically via URL:\n"
    "     http://someserver.com/cgi-bin/phyloGif?phyloGif_tree=(A:0.1,B:0.1)\n"
    "Options/CGI-vars:\n"
    "  -phyloGif_width=N - width of output GIF, default %d\n"
    "  -phyloGif_height=N - height of output GIF, default %d\n"
    "  -phyloGif_tree= - data in format (nodeA:0.5,nodeB:0.6):1.2;\n"
    "     If running at the command-line, can put filename here or stdin\n"
    "     (this is actually required)\n"
    "  -phyloGif_branchLengths - use branch lengths for layout\n"
    "  -phyloGif_lengthLegend - show length ruler at bottom\n"
    "  -phyloGif_branchLabels - show length of branch as label\n"
    "     (used with -phyloGif_branchLengths)\n"
    "  -phyloGif_branchDecimals=N - show length of branch to N decimals, default %d\n"
    "  -phyloGif_branchMultiplier=N - multiply branch length by N default %d\n"
    "  -phyloGif_htmlPage - wrap the output in an html page (cgi only)\n"
    "  -phyloGif_underscores - preserve underscores in input as spaces in output\n"
    , msg, width, height, branchDecimals, branchMultiplier);
}

struct phyloLayout
{
    int depth;          /* leaves have depth=0 */
    double vPos;        /* vertical position   */
    double hPos;        /* horizontal position */
};



void stripUnderscoreSuffixes(char *s)
/* Strip out underscores in labels, modifies s. */
{
char *j=s;
char c = ' ';
boolean inUnderScore = FALSE;
while(TRUE)
    {
    c = *s++;
    if (c == 0)
	break;
    if (inUnderScore)
	{
	if (!isalnum(c))
	    {
	    *j++ = c;
	    inUnderScore = FALSE;
	    }
	}
    else
	{
	if (c=='_')
	    {
	    inUnderScore = TRUE;
	    }
	else
	    {
	    *j++ = c;
	    }
	}
    }
*j = 0;    
}


#define MARGIN 10

static void phyloTreeLayoutBL(struct phyloTree *phyloTree, 
    int *pMaxDepth, int *pNumLeafs, int depth,
    MgFont *font, int *pMaxLabelWidth, int width, double *pMinMaxFactor, double parentHPos)
/* do a depth-first recursion over the tree, assigning depth and vPos to each node
 * and keeping track of maxDepth and numLeafs to return */
{
struct phyloLayout *this = NULL;
if (depth > *pMaxDepth)
    *pMaxDepth = depth;
phyloTree->priv = (void *) needMem(sizeof(struct phyloLayout));
this = (struct phyloLayout *) phyloTree->priv;
this->hPos = parentHPos + phyloTree->ident->length;
if (branchLengths && phyloTree->ident->length == 0 && depth != 0)
    safef(layoutErrMsg,sizeof(layoutErrMsg),"Branch length is missing for %s.\n",
	phyloTree->ident->name ? phyloTree->ident->name : "an internal node");
if (phyloTree->numEdges == 2)  /* node */
    {
    int maxDepth = 0;
    struct phyloLayout *that = NULL;
    double vPos = 0;
    phyloTreeLayoutBL(phyloTree->edges[0], pMaxDepth, pNumLeafs, depth+1, 
	font, pMaxLabelWidth, width, pMinMaxFactor, this->hPos);
    phyloTreeLayoutBL(phyloTree->edges[1], pMaxDepth, pNumLeafs, depth+1, 
	font, pMaxLabelWidth, width, pMinMaxFactor, this->hPos);
    that = (struct phyloLayout *) phyloTree->edges[0]->priv;
    if (that->depth > maxDepth)
	maxDepth = that->depth;
    vPos += that->vPos;	    
    that = (struct phyloLayout *) phyloTree->edges[1]->priv;
    if (that->depth > maxDepth)
	maxDepth = that->depth;
    vPos += that->vPos;	    
    this->depth=maxDepth+1;
    this->vPos=vPos/2;
    }
else if (phyloTree->numEdges == 0)  /* leaf */
    {
    int w=0;
    double factor=0.0;
    this->depth=0;
    this->vPos=*pNumLeafs;
    (*pNumLeafs)++;
    /* de-escape name if needed */
    if(stringIn(escapePattern,phyloTree->ident->name))
	{
	char *temp = phyloTree->ident->name;
	phyloTree->ident->name = replaceChars(temp,escapePattern," ");
	freez(&temp);
	}	
    w=mgFontStringWidth(font,phyloTree->ident->name);
    if (w > *pMaxLabelWidth)
	*pMaxLabelWidth = w;
    factor = (width - 3*MARGIN - w) / this->hPos;
    if (*pMinMaxFactor == 0.0 || factor < *pMinMaxFactor)
	*pMinMaxFactor = factor;
    }
else
    {
    safef(layoutErrMsg,sizeof(layoutErrMsg),
	"Expected tree nodes to have 0 or 2 edges, found %d.\n"
	"Check for missing commas or missing data.\n"
	,phyloTree->numEdges); 
    }

}

static void phyloTreeGif(struct phyloTree *phyloTree, 
    int maxDepth, int numLeafs, int maxLabelWidth, 
    int width, int height, struct memGfx *mg, MgFont *font)
/* do a depth-first recursion over the tree, printing tree to gif */
{
struct phyloLayout *this = (struct phyloLayout *) phyloTree->priv;
int fHeight = mgFontPixelHeight(font);
int vdelta = numLeafs < 2 ? 0 : (height - 2*MARGIN - fHeight) / (numLeafs - 1);
int v = MARGIN + this->vPos * vdelta;
int h = width - MARGIN - maxLabelWidth;
int hl = MARGIN;
int hr = h - MARGIN;
int hw = hr - hl;
int deltaH = hw / (maxDepth+1);
if (phyloTree->parent)
    {	   
    struct phyloLayout *that = (struct phyloLayout *) phyloTree->parent->priv;
    mgDrawLine(mg, hr-that->depth*deltaH, v+fHeight/2, hr-this->depth*deltaH, v+fHeight/2, MG_BLACK);
    }

if (phyloTree->numEdges == 2) 
    {
    struct phyloLayout *that0 = (struct phyloLayout *) phyloTree->edges[0]->priv;
    struct phyloLayout *that1 = (struct phyloLayout *) phyloTree->edges[1]->priv;
    phyloTreeGif(phyloTree->edges[0], maxDepth, numLeafs, maxLabelWidth, width, height, mg, font);
    phyloTreeGif(phyloTree->edges[1], maxDepth, numLeafs, maxLabelWidth, width, height, mg, font);
    mgDrawLine(mg, hr-this->depth*deltaH, that0->vPos*vdelta+fHeight/2+MARGIN, 
                   hr-this->depth*deltaH, that1->vPos*vdelta+fHeight/2+MARGIN, MG_BLACK);
    if (phyloTree->ident->name)
	mgText(mg, h, v, MG_BLACK, font, phyloTree->ident->name);
    }
else if (phyloTree->numEdges == 0)  
    {
    mgText(mg, h, v, MG_BLACK, font, phyloTree->ident->name);
    }
else
    errAbort("expected tree nodes to have 0 or 2 edges, found %d\n", phyloTree->numEdges); 
    

}

static void phyloTreeGifBL(struct phyloTree *phyloTree, 
    int maxDepth, int numLeafs, int maxLabelWidth, 
    int width, int height, struct memGfx *mg, MgFont *font, double minMaxFactor, boolean isRightEdge)
/* do a depth-first recursion over the tree, printing tree to gif */
{
struct phyloLayout *this = (struct phyloLayout *) phyloTree->priv;
int fHeight = mgFontPixelHeight(font);
int vdelta = numLeafs < 2 ? 0 : (height - 2*MARGIN - fHeight) / (numLeafs - 1);
int v = MARGIN + this->vPos * vdelta;
mgDrawLine(mg, MARGIN+(this->hPos-phyloTree->ident->length)*minMaxFactor, v+fHeight/2, 
               MARGIN+this->hPos*minMaxFactor,                            v+fHeight/2, MG_BLACK);
if (branchLabels)
    {
    if (phyloTree->ident->length > 0.0 && this->hPos > 0.0)
	{
	char patt[16];
	char label[256];
	safef(patt,sizeof(patt),"%%%d.%df",branchDecimals+2,branchDecimals);
	safef(label,sizeof(label),patt,(phyloTree->ident->length)*branchMultiplier);  /* was %6.4f */
    	mgTextCentered(mg, MARGIN+(this->hPos-phyloTree->ident->length)*minMaxFactor, v+(fHeight/2)*(isRightEdge?1:-1), 
	    phyloTree->ident->length*minMaxFactor, fHeight, MG_BLACK, font, label);
	}
    }

if (phyloTree->numEdges == 2) 
    {
    struct phyloLayout *that0 = (struct phyloLayout *) phyloTree->edges[0]->priv;
    struct phyloLayout *that1 = (struct phyloLayout *) phyloTree->edges[1]->priv;
    phyloTreeGifBL(phyloTree->edges[0], maxDepth, numLeafs, maxLabelWidth, width, height, mg, font, minMaxFactor, FALSE);
    phyloTreeGifBL(phyloTree->edges[1], maxDepth, numLeafs, maxLabelWidth, width, height, mg, font, minMaxFactor, TRUE);
    mgDrawLine(mg, MARGIN+this->hPos*minMaxFactor, that0->vPos*vdelta+fHeight/2+MARGIN, 
                   MARGIN+this->hPos*minMaxFactor, that1->vPos*vdelta+fHeight/2+MARGIN, MG_BLACK);
    if (phyloTree->ident->name)
    	mgText(mg, MARGIN+this->hPos*minMaxFactor+MARGIN, v, MG_BLACK, font, phyloTree->ident->name);
    }
else if (phyloTree->numEdges == 0)  
    {
    mgText(mg, MARGIN+this->hPos*minMaxFactor+MARGIN, v, MG_BLACK, font, phyloTree->ident->name);
    }
else
    errAbort("expected tree nodes to have 0 or 2 edges, found %d\n", phyloTree->numEdges); 
    

}


int main(int argc, char *argv[])
{
char *phyloData = NULL, *temp = NULL;
struct phyloTree *phyloTree = NULL;
int maxDepth = 0, numLeafs = 0, maxLabelWidth = 0;
double minMaxFactor = 0.0;
struct memGfx *mg = NULL;
boolean useCart = FALSE;
oldVars = hashNew(8);
onWeb = cgiIsOnWeb();
cgiSpoof(&argc, argv);
if (argc != 1)
    usage("wrong number of args");

if (onWeb)
    {
    htmlSetBackground(hBackgroundImage());  /* uses cfgOption */
    /* this will cause it to kick out the set-cookie: http response header line */
    cart = cartAndCookieNoContent(hUserCookie(), excludeVars, oldVars);
    }
else    
    {
    if (!cgiOptionalString("phyloGif_tree"))
    	usage("-phyloGif_tree is a required 'option' or cgi variable.");
    }

//cartWarnCatcher(doMiddle, cart, cartEarlyWarningHandler);


useCart = (!cgiOptionalString("phyloGif_tree") || cgiVarExists("phyloGif_restore"));

MgFont *font = mgMediumBoldFont();
htmlPageWrapper = cgiVarExists("phyloGif_htmlPage"); /* wrap output in a page */

if (onWeb && sameString(getenv("REQUEST_METHOD"),"HEAD"))
    { /* tell browser it's static just so it can save it */
    if (htmlPageWrapper)
    	printf("Content-type: text/html\r\n");
    else
    	printf("Content-type: image/gif\r\n");
    printf("\r\n");
    return 0;
    }

if (useCart)
    {
    width = cartUsualInt(cart,"phyloGif_width",width);    
    height = cartUsualInt(cart,"phyloGif_height",height);    
    phyloData = cloneString(cartOptionalString(cart,"phyloGif_tree"));
    branchLengths = cartVarExists(cart,"phyloGif_branchLengths");
    lengthLegend = cartVarExists(cart,"phyloGif_lengthLegend");
    branchLabels = cartVarExists(cart,"phyloGif_branchLabels");
    branchDecimals = cartUsualInt(cart,"phyloGif_branchDecimals", branchDecimals);
    branchMultiplier = cartUsualInt(cart,"phyloGif_branchMultiplier", branchMultiplier);
    preserveUnderscores = cartVarExists(cart,"phyloGif_underscores");
    }
else
    {
    width = cgiUsualInt("phyloGif_width",width);    
    height = cgiUsualInt("phyloGif_height",height);    
    phyloData = cloneString(cgiOptionalString("phyloGif_tree"));
    branchLengths = cgiVarExists("phyloGif_branchLengths");
    lengthLegend = cgiVarExists("phyloGif_lengthLegend");
    branchLabels = cgiVarExists("phyloGif_branchLabels");
    branchDecimals = cgiUsualInt("phyloGif_branchDecimals", branchDecimals);
    branchMultiplier = cgiUsualInt("phyloGif_branchMultiplier", branchMultiplier);
    preserveUnderscores = cgiVarExists("phyloGif_underscores");
    }
    
if (useCart)
    {
    if (onWeb)
	{
    	printf("Content-type: text/html\r\n");
	printf("\r\n");
	cartWebStart(cart, NULL, "%s", "phyloGif Interactive Phylogenetic Tree Gif Maker");
	puts("<form method=\"GET\" action=\"phyloGif\" name=\"mainForm\">");
	cartSaveSession(cart);
	puts("<table>");
	puts("<tr><td>Width:</td><td>"); cartMakeIntVar(cart, "phyloGif_width", width, 4); puts("</td></tr>");
	puts("<tr><td>Height:</td><td>"); cartMakeIntVar(cart, "phyloGif_height", height, 4); puts("</td></tr>");
	puts("<tr><td>Use branch lengths?</td><td>"); cartMakeCheckBox(cart, "phyloGif_branchLengths", branchLengths); puts("</td></tr>");
	puts("<tr><td>&nbsp; Show length ruler?</td><td>"); cartMakeCheckBox(cart, "phyloGif_lengthLegend", lengthLegend); puts("</td></tr>");
	puts("<tr><td>&nbsp; Show length values?</td><td>"); cartMakeCheckBox(cart, "phyloGif_branchLabels", branchLabels); puts("</td></tr>");
	puts("<tr><td>&nbsp; How many decimal places?</td><td>"); cartMakeIntVar(cart, "phyloGif_branchDecimals", branchDecimals,1); puts("</td></tr>");
	puts("<tr><td>&nbsp; Multiply branch length by factor?</td><td>"); cartMakeIntVar(cart, "phyloGif_branchMultiplier", branchMultiplier,5); puts("</td></tr>");
	puts("<tr><td>Preserve Underscores?</td><td>"); cartMakeCheckBox(cart, "phyloGif_underscores", preserveUnderscores); puts("</td></tr>");
	puts("<tr><td>Wrap in html page?</td><td>"); cartMakeCheckBox(cart, "phyloGif_htmlPage", htmlPageWrapper); puts("</td></tr>");

        printf("<tr><td><big>TREE:</big>");
	puts("<br><br><INPUT type=\"submit\" name=\"phyloGif_restore\" value=\"restore default\">");
	
	puts("</td><td><textarea name=\"phyloGif_tree\" rows=14 cols=70>");
	if (NULL == phyloData || phyloData[0] == '\0' || cgiVarExists("phyloGif_restore"))
	    {
	    puts(
"(((((((((\n"
"(human_hg18:0.00669,chimp_panTro1:0.00757):0.0243,\n"
"  macaque_rheMac2:0.0592):0.0240,\n"
"    ((rat_rn4:0.0817,mouse_mm8:0.0770):0.229,\n"
"          rabbit_oryCun1:0.207):0.107):0.0230,\n"
"          (cow_bosTau2:0.159,dog_canFam2:0.148):0.0394):0.0285,\n"
"          armadillo_dasNov1:0.150):0.0160,\n"
"          (elephant_loxAfr1:0.105,tenrec_echTel1:0.260):0.0404):0.218,\n"
"          monodelphis_monDom4:0.371):0.189,\n"
"          chicken_galGal2:0.455):0.123,\n"
"          xenopus_xenTro1:0.782):0.156,\n"
"          ((tetraodon_tetNig1:0.199,fugu_fr1:0.239):0.493,\n"
"              zebrafish_danRer3:0.783):0.156);"
		);
	    }
	else
	    {
	    printf("%s",phyloData);
	    }
	puts("</TEXTAREA>");
	puts("</td></tr>");
	puts("<tr><td>&nbsp;</td><td>");
	puts("<INPUT type=\"submit\" name=\"phyloGif_submit\" value=\"submit\">");
	puts("</td></tr>");
	puts("</table>");
	puts("</form>");
	webNewSection("Notes");
    	puts(
"\n"
"1. Length-ruler and length-values cannot be shown unless use-branch-lengths is also checked.<br>\n"
"<br>\n"
"2. Underscores and anything following them are automatically stripped from node labels\n"
"unless the preserve-underscores checkbox is checked, in which case they are converted to spaces.<br>\n"
"<br>\n"
"3. If a space is required in a node label, enter it as a dash.<br>\n"
"<br>\n"
"4. The tree is in the phastCons or .nh format name:length.  Parentheses create a parent.\n"
"Parents must have two children. Length is not required if use-branch-lengths is not checked.\n"
"The length of the root branch is usually not specified.<br>\n"
"Examples:<br>\n"
"<table cellpadding=10>\n"
"<tr><td><PRE>\n"
"((A:0.1,B:0.1):0.2,C:0.15);\n"
"</PRE></td><td>\n"
"<IMG SRC=\"?phyloGif_width=200&phyloGif_height=120&phyloGif_branchLengths=1&phyloGif_tree=((A:0.1,B:0.1):0.2,C:0.15);\">\n"
"</td></tr>\n"
"<tr><td><PRE>\n"
"((A:0.1,B:0.1)D:0.2,C:0.15)E;\n"
"</PRE></td><td>\n"
"<IMG SRC=\"?phyloGif_width=200&phyloGif_height=120&phyloGif_branchLengths=1&phyloGif_tree=((A:0.1,B:0.1)D:0.2,C:0.15)E;\">\n"
"<br>(internal or ancestral node labels)\n"
"</td></tr>\n"
"<tr><td><PRE>\n"
"  ((((\n"
"   (\n"
"     ((mouse,rat),human),\n"
"       (dog,cow)\n"
"    ),\n"
"     opossum),\n"
"     chicken),\n"
"     xenopus),\n"
"    (tetraodon,zebrafish));\n"
"</PRE></td><td>\n"
"<IMG SRC=\"?phyloGif_width=200&phyloGif_height=200&phyloGif_tree=(((((((mouse,rat),human),(dog,cow)),opossum),chicken),xenopus),(tetraodon,zebrafish));\">\n"
"</td></tr>\n"
"</table>\n"
"5. PhastCons branch lengths are expected substitutions per site, allowing for\n"
"multiple hits.  So a branch length of 0.5 means an average of one\n"
"substitution every two nucleotide sites, but the percent id will be\n"
"less than 50% because some of those substitutions are obscured by\n"
"subsequent substitutions.  They are estimated from neutral sites,\n"
"sometimes fourfold degenerate sites in coding regions, or sometimes\n"
"\"nonconserved\" sites according to phastCons.  The numbers are significant\n"
"to two or three figures.<br>\n"
"<br>\n"
"6. Wrap-in-html is useful when the browser automatically shinks a large image.\n"
"This option keeps the image view full in the browser automatically.\n"
"However, do not use with IE6 when performing save-as.\n"
"<br>"
	    );
	cartWebEnd();
	return 0;
	}
    else	    
    	usage("-phyloGif_tree is a required 'option' or cgi variable.");
    }

if (htmlPageWrapper)
    {
    printf("Content-type: text/html\r\n");
    printf("\r\n");
    puts("<html><head><title>Phylogenetic Tree</title></head><body>");
    printf("<IMAGE SRC=\"http://%s%s"
	    "?phyloGif_width=%d"
	    "&phyloGif_height=%d"
	    "&phyloGif_tree=%s"
	,getenv("SERVER_NAME"),getenv("SCRIPT_NAME"),width,height,phyloData);
    if (branchLengths)
	printf("&phyloGif_branchLengths=1");
    if (lengthLegend)
	printf("&phyloGif_lengthLegend=1");
    if (branchLabels)
	printf("&phyloGif_branchLabels=1");
    printf("&phyloGif_branchDecimals=%d",branchDecimals);
    printf("&phyloGif_branchMultipliers=%d",branchMultiplier);
    if (preserveUnderscores)
	printf("&phyloGif_underscores=1");
    puts("\"></body></html>");
    freez(&phyloData);
    return 0;
    }



if (!onWeb && phyloData[0] != '(')
    {
    int fd = 0;  /* default to stdin */
    if (!sameString(phyloData,"stdin"))
	fd = open(phyloData,O_RDONLY);
    struct dyString *dy = netSlurpFile(fd);
    if (fd)
    	close(fd);
    freez(&phyloData);
    phyloData = dyStringCannibalize(&dy);
    }

/* preserve underscores option */
if (preserveUnderscores)
    {
    char *temp = phyloData;
    phyloData = replaceChars(temp,"_","-");
    freez(&temp);
    }

/* get rid of underscore suffixes */
stripUnderscoreSuffixes(phyloData);

/* escape dash chars with some XXX pattern */
escapePattern = cloneString("");
do
    {
    char *temp = escapePattern;
    escapePattern=addSuffix(temp,"X");
    freez(&temp);
    } while (stringIn(escapePattern,phyloData));
if (strchr(phyloData,'-'))
    {
    char *temp = phyloData;
    phyloData = replaceChars(temp,"-",escapePattern);
    freez(&temp);
    }	


/* add trailing semi-colon if it got stripped off */
if (!strchr(phyloData,';'))
    {
    temp = phyloData;
    phyloData = addSuffix(phyloData,";");
    freez(&temp);
    }


/* parse phyloTree, but catch errAborts if any */

{
struct errCatch *errCatch = errCatchNew();
char *errMsg = NULL;
if (errCatchStart(errCatch))
    {
    phyloTree = phyloParseString(phyloData);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    errMsg = cloneString(errCatch->message->string);
    }
errCatchFree(&errCatch);
if (errMsg)
    {
    if (onWeb)
	{
	printf("Content-type: text/html\r\n");
	printf("\r\n");
	puts("<html><head><title>PhyloTree parse error</title></head><body><pre>");
	/* we dont think the specific error message coming back are correct or useful
	 * so supply a generic err msg */
	errMsg = cloneString("syntax error.");
    	printf("input tree: [%s]\n\n%s",cgiString("phyloGif_tree"),errMsg);
    	puts("</pre></body></html>");
	}
    else
	{
    	warn(errMsg);
	}
    freez(&errMsg);    
    freez(&phyloData);
    return 0;
    }
}




if (phyloTree)
    {
    mg = mgNew(width,height);
    mgClearPixels(mg);

    lengthLegend = lengthLegend && branchLengths;  /* moot without lengths */
    if (lengthLegend)
	{
	int fHeight = mgFontPixelHeight(font);
	height -= (MARGIN+2*fHeight);
	}

    phyloTreeLayoutBL(phyloTree, &maxDepth, &numLeafs, 0, font, &maxLabelWidth, width, &minMaxFactor, 0.0);
    
    if (layoutErrMsg[0] != 0)
	{
	if (onWeb)
	    {
	    printf("Content-type: text/html\r\n");
	    printf("\r\n");
	    puts("<html><head><title>PhyloTree error</title></head><body><pre>");
	    printf("input tree: [%s]\n\n%s",cgiString("phyloGif_tree"),layoutErrMsg);
	    puts("</pre></body></html>");
	    }
	else
	    {
	    warn(layoutErrMsg);
	    }
	freez(&phyloData);
	mgFree(&mg);
	return 0;
	}

    if (branchLengths)
        phyloTreeGifBL(phyloTree, maxDepth, numLeafs, maxLabelWidth, width, height, 
	    mg, font, minMaxFactor, FALSE);
    else	    
        phyloTreeGif(phyloTree, maxDepth, numLeafs, maxLabelWidth, width, height, mg, font);

    if (lengthLegend)
	{
	int i = 0;
	char out[256];
	double scaleEnd = (width - 2*MARGIN); 
	int fHeight = mgFontPixelHeight(font);
	int x=0;
	int dh=0;
	
	mgDrawLine(mg, MARGIN,       height+fHeight/2, 
		       width-MARGIN, height+fHeight/2, MG_BLACK);
	while(TRUE)
	    {
	    x=((minMaxFactor*i)/10);
	    if (x >= scaleEnd)
		break;
	    if ((i % 5) == 0)
		{
		int y = mgFontCharWidth(font,'0');
		y += 0.5*mgFontCharWidth(font,'.');
		safef(out,sizeof(out),"%3.2f",branchMultiplier*i/10.0);
                if (branchMultiplier > 10)
                    safef(out,sizeof(out),"%3.0f",branchMultiplier*i/10.0);
    		mgText(mg, MARGIN+x-y, height+fHeight, MG_BLACK, font, out);
		dh=fHeight/2;
		}
	    else
		{
		dh = fHeight / 4;
		}
	    mgDrawLine(mg, MARGIN+x, height+fHeight/2-dh, 
	     		   MARGIN+x, height+fHeight/2+dh, MG_BLACK);
	    ++i;
	    }

	
	}

    }


if (onWeb)
    {
    printf("Content-type: image/gif\r\n");
    printf("\r\n");
    }
if (!mgSaveToGif(stdout, mg))
    {
    errAbort("Couldn't save gif to stdout");
    }

if (cgiOptionalString("phyloGif_submit"))
    cartCheckout(&cart);

/* there's no code for freeing the phyloTree yet in phyloTree.c */

mgFree(&mg);
freez(&phyloData);


return 0;
}

