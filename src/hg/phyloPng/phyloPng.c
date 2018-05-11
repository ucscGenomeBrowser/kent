/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* phyloPng.c for parsing phylo tree and outputting a png.
 *
 *  Author: Galt Barber 2006 
 * 
 *  Designed to be run either as a cgi or as a command-line utility.
 *  The input file spec matches the .nh tree format e.g. (cat:0.5,dog:0.6):1.2;
 *  The output png layout was originally designed to ignore branch lengths.
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
 *    <IMG SRC="../cgi-bin/phyloPng?phyloPng_width=120&phyloPng_height=120&phyloPng_tree=(a:1,b:1);" >
 *
 *  Or as cgi in html POST:
<form method="POST" action="../cgi-bin/phyloPng/phylo.png" name="mainForm">
<table>
<tr><td>width:</td><td><INPUT type="text" name="phyloPng_width" value="240" size="4"></td></tr>
<tr><td>height:</td><td><INPUT type="text" name="phyloPng_height" value="512" size="4"></td></tr>
<tr><td>tree:</td><td><textarea name="phyloPng_tree" rows=14 cols=70>
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
<tr><td>&nbsp;</td><td><INPUT type="submit" name="phyloPng_submit" value="submit"></td></tr>
</table>
</form>
 
 */


#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "net.h"
#include "cheapcgi.h"
#include "errAbort.h"
#include "phyloTree.h"
#include "portable.h"
#include "memgfx.h"

#include "cart.h"
#include "hui.h"
#include "htmshell.h"
#include "web.h"


#include "errAbort.h"
#include "errCatch.h"


struct cart *cart=NULL;      /* The user's ui state. */
struct hash *oldVars = NULL;
boolean onWeb = FALSE;

int width=240,height=512;
boolean branchLengths = FALSE;  /* branch lengths */
boolean lengthLegend = FALSE;   /* length ruler*/
boolean branchLabels = FALSE;   /* labelled branch lengths */
boolean htmlPageWrapper = FALSE;  /* wrap output in an html page */
boolean stripUnderscoreSuff = FALSE;   /* strip underscore suffixes from labels in input */
boolean dashToSpace = FALSE;    /* convert dash to space */
boolean underToSpace = FALSE;   /* convert underscore to space */
boolean monospace = FALSE;      /* use monospace font */
int branchDecimals = 2;         /* show branch label length to two decimals by default */
int branchMultiplier = 1;       /* multiply branch length by factor */
char layoutErrMsg[1024] = "";

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", "phyloPng_submit", "phyloPng_restore", NULL};

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort(
    "%s\n\n"
    "phyloPng - parse and display phyloGenetic tree\n"
    "\n"
    "Command-line Usage Examples:\n"
    "   phyloPng -phyloPng_tree=tree.nh [options] > phylo.png \n"
    "   phyloPng -phyloPng_tree='(A:0.1,B:0.1)' [options] > phylo.png \n"
    "CGI Usage Examples:\n"
    "   Create an interactive form:\n"
    "     http://someserver.com/cgi-bin/phyloPng\n"
    "   Create an image dynamically via URL:\n"
    "     http://someserver.com/cgi-bin/phyloPng?phyloPng_tree=(A:0.1,B:0.1)\n"
    "Options/CGI-vars:\n"
    "  -phyloPng_width=N - width of output PNG, default %d\n"
    "  -phyloPng_height=N - height of output PNG, default %d\n"
    "  -phyloPng_tree= - data in format (nodeA:0.5,nodeB:0.6):1.2;\n"
    "     If running at the command-line, can put filename here or stdin\n"
    "     (this is actually required)\n"
    "  -phyloPng_branchLengths - use branch lengths for layout\n"
    "  -phyloPng_lengthLegend - show length ruler at bottom\n"
    "  -phyloPng_branchLabels - show length of branch as label\n"
    "     (used with -phyloPng_branchLengths)\n"
    "  -phyloPng_branchDecimals=N - show length of branch to N decimals, default %d\n"
    "  -phyloPng_branchMultiplier=N - multiply branch length by N default %d\n"
    "  -phyloPng_htmlPage - wrap the output in an html page (cgi only)\n"
    "  -phyloPng_underscores - preserve underscores in input as spaces in output\n"
    "  -phyloPng_monospace - use a monospace proportional font\n"
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

    if(!phyloTree->ident->name)
	{
	safef(layoutErrMsg,sizeof(layoutErrMsg),
	"leaf is missing label\n");
	return;
	}

    if(dashToSpace)
	{
	char *temp = phyloTree->ident->name;
	phyloTree->ident->name = replaceChars(temp,"-"," ");
	freez(&temp);
	}	

    if(underToSpace)
	{
	char *temp = phyloTree->ident->name;
	phyloTree->ident->name = replaceChars(temp,"_"," ");
	freez(&temp);
	}	

    /* strip underscore suffixes option */
    if (stripUnderscoreSuff)
	stripUnderscoreSuffixes(phyloTree->ident->name);

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

static void phyloTreePng(struct phyloTree *phyloTree, 
    int maxDepth, int numLeafs, int maxLabelWidth, 
    int width, int height, struct memGfx *mg, MgFont *font)
/* do a depth-first recursion over the tree, printing tree to png */
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
    phyloTreePng(phyloTree->edges[0], maxDepth, numLeafs, maxLabelWidth, width, height, mg, font);
    phyloTreePng(phyloTree->edges[1], maxDepth, numLeafs, maxLabelWidth, width, height, mg, font);
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

static void phyloTreePngBL(struct phyloTree *phyloTree, 
    int maxDepth, int numLeafs, int maxLabelWidth, 
    int width, int height, struct memGfx *mg, MgFont *font, double minMaxFactor, boolean isRightEdge)
/* do a depth-first recursion over the tree, printing tree to png */
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
    phyloTreePngBL(phyloTree->edges[0], maxDepth, numLeafs, maxLabelWidth, width, height, mg, font, minMaxFactor, FALSE);
    phyloTreePngBL(phyloTree->edges[1], maxDepth, numLeafs, maxLabelWidth, width, height, mg, font, minMaxFactor, TRUE);
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
boolean isMSIE = FALSE;
char *userAgent = getenv("HTTP_USER_AGENT");
if (userAgent && strstr(userAgent ,"MSIE"))
    isMSIE = TRUE;
	

cgiSpoof(&argc, argv);
if (argc != 1)
    usage("wrong number of args");

if (onWeb)
    {
    /* this will cause it to kick out the set-cookie: http response header line */
    cart = cartAndCookieNoContent(hUserCookie(), excludeVars, oldVars);
    }
else    
    {
    if (!cgiOptionalString("phyloPng_tree"))
    	usage("-phyloPng_tree is a required 'option' or cgi variable.");
    }

useCart = (!cgiOptionalString("phyloPng_tree") || cgiVarExists("phyloPng_restore"));

htmlPageWrapper = cgiVarExists("phyloPng_htmlPage"); /* wrap output in a page */

if (onWeb && sameString(getenv("REQUEST_METHOD"),"HEAD"))
    { /* tell browser it's static just so it can save it */
    if (htmlPageWrapper)
    	printf("Content-type: text/html\r\n");
    else
    	printf("Content-type: image/png\r\n");
    printf("\r\n");
    return 0;
    }

if (useCart)
    {
    width = cartUsualInt(cart,"phyloPng_width",width);    
    height = cartUsualInt(cart,"phyloPng_height",height);    
    phyloData = cloneString(cartOptionalString(cart,"phyloPng_tree"));
    branchLengths = cartVarExists(cart,"phyloPng_branchLengths");
    lengthLegend = cartVarExists(cart,"phyloPng_lengthLegend");
    branchLabels = cartVarExists(cart,"phyloPng_branchLabels");
    branchDecimals = cartUsualInt(cart,"phyloPng_branchDecimals", branchDecimals);
    branchMultiplier = cartUsualInt(cart,"phyloPng_branchMultiplier", branchMultiplier);
    stripUnderscoreSuff = cartVarExists(cart,"phyloPng_undersuff_strip");
    dashToSpace = cartVarExists(cart,"phyloPng_dash_to_space");
    underToSpace = cartVarExists(cart,"phyloPng_under_to_space");
    monospace = cartVarExists(cart, "phyloPng_monospace");
    }
else
    {
    width = cgiUsualInt("phyloPng_width",width);    
    height = cgiUsualInt("phyloPng_height",height);    
    phyloData = cloneString(cgiOptionalString("phyloPng_tree"));
    branchLengths = cgiVarExists("phyloPng_branchLengths");
    lengthLegend = cgiVarExists("phyloPng_lengthLegend");
    branchLabels = cgiVarExists("phyloPng_branchLabels");
    branchDecimals = cgiUsualInt("phyloPng_branchDecimals", branchDecimals);
    branchMultiplier = cgiUsualInt("phyloPng_branchMultiplier", branchMultiplier);
    stripUnderscoreSuff = cgiVarExists("phyloPng_undersuff_strip");
    dashToSpace = cgiVarExists("phyloPng_dash_to_space");
    underToSpace = cgiVarExists("phyloPng_under_to_space");
    monospace = cgiVarExists("phyloPng_monospace");
    }
    
if (useCart)
    {
    if (onWeb)
	{
    	printf("Content-type: text/html\r\n");
	printf("\r\n");
	cartWebStart(cart, NULL, "%s", "phyloPng Interactive Phylogenetic Tree Png Maker");

	if (isMSIE)  /* cannot handle long urls */
	    puts("<form method=\"POST\" action=\"phyloPng\" name=\"mainForm\">");
	else
	    puts("<form method=\"GET\" action=\"phyloPng\" name=\"mainForm\">");

	cartSaveSession(cart);
	puts("<table>");
	puts("<tr><td>Width:</td><td>"); cartMakeIntVar(cart, "phyloPng_width", width, 4); puts("</td></tr>");
	puts("<tr><td>Height:</td><td>"); cartMakeIntVar(cart, "phyloPng_height", height, 4); puts("</td></tr>");
	puts("<tr><td>Use branch lengths?</td><td>"); cartMakeCheckBox(cart, "phyloPng_branchLengths", branchLengths); puts("</td></tr>");
	puts("<tr><td>&nbsp; Show length ruler?</td><td>"); cartMakeCheckBox(cart, "phyloPng_lengthLegend", lengthLegend); puts("</td></tr>");
	puts("<tr><td>&nbsp; Show length values?</td><td>"); cartMakeCheckBox(cart, "phyloPng_branchLabels", branchLabels); puts("</td></tr>");
	puts("<tr><td>&nbsp; How many decimal places?</td><td>"); cartMakeIntVar(cart, "phyloPng_branchDecimals", branchDecimals,1); puts("</td></tr>");
	puts("<tr><td>&nbsp; Multiply branch length by factor?</td><td>"); cartMakeIntVar(cart, "phyloPng_branchMultiplier", branchMultiplier,5); puts("</td></tr>");
	puts("<tr><td>Strip underscore-suffixes?</td><td>"); cartMakeCheckBox(cart, "phyloPng_undersuff_strip", stripUnderscoreSuff); puts("</td></tr>");
	puts("<tr><td>Change dash to space?</td><td>"); cartMakeCheckBox(cart, "phyloPng_dash_to_space", dashToSpace); puts("</td></tr>");
	puts("<tr><td>Change underscore to space?</td><td>"); cartMakeCheckBox(cart, "phyloPng_under_to_space", underToSpace); puts("</td></tr>");
	puts("<tr><td>Wrap in html page?</td><td>"); cartMakeCheckBox(cart, "phyloPng_htmlPage", htmlPageWrapper); puts("</td></tr>");
	puts("<tr><td>Monospace font?</td><td>"); cartMakeCheckBox(cart, "phyloPng_monospace", monospace); puts("</td></tr>");

        printf("<tr><td><big>TREE:</big>");
	puts("<br><br><INPUT type=\"submit\" name=\"phyloPng_restore\" value=\"restore default\">");
	
	puts("</td><td><textarea name=\"phyloPng_tree\" rows=14 cols=70>");
	if (NULL == phyloData || phyloData[0] == '\0' || cgiVarExists("phyloPng_restore"))
	    {
	    puts(
"(((((Human:0.00655,\n"
"    Chimp:0.00684):0.027424,\n"
"   Rhesus:0.037601):0.109934,\n"
"  (Mouse:0.084509,\n"
"  Rat:0.091589):0.271974):0.020593,\n"
" Dog:0.165928):0.258392,\n"
"Opossum:0.340786);\n"
		);
	    }
	else
	    {
	    printf("%s",phyloData);
	    }
	puts("</TEXTAREA>");
	puts("</td></tr>");
	puts("<tr><td>&nbsp;</td><td>");
	puts("<INPUT type=\"submit\" name=\"phyloPng_submit\" value=\"submit\">");
	cgiMakeClearButtonNoSubmit("mainForm", "phyloPng_tree");
	puts("</td></tr>");
	puts("</table>");
	puts("</form>");
	webNewSection("Notes");
    	puts(
"\n"
"1. Length-ruler and length-values cannot be shown unless use-branch-lengths is also checked.<br>\n"
"<br>\n"
"2. If \"Strip underscore-suffixes?\" is checked, underscores and anything following them are stripped from node labels.<br>\n"
"<br>\n"
"3. For backwards compatibility, options exist to convert a dash or underscore to a space in a node label.<br>\n"
"<br>\n"
"4. The tree is in the phastCons or .nh format name:length.  Parentheses create a parent.\n"
"Parents must have two children. Length is not required if use-branch-lengths is not checked.\n"
"The length of the root branch is usually not specified.<br>\n"
"<br>\n"
"Examples:<br>\n"
"<table cellpadding=10>\n"
"<tr><td><PRE>\n"
"((A:0.1,B:0.1):0.2,C:0.15);\n"
"</PRE></td><td>\n"
"<IMG SRC=\"?phyloPng_width=200&phyloPng_height=120&phyloPng_branchLengths=1&phyloPng_tree=((A:0.1,B:0.1):0.2,C:0.15);\">\n"
"</td></tr>\n"
"<tr><td><PRE>\n"
"((A:0.1,B:0.1)D:0.2,C:0.15)E;\n"
"</PRE></td><td>\n"
"<IMG SRC=\"?phyloPng_width=200&phyloPng_height=120&phyloPng_branchLengths=1&phyloPng_tree=((A:0.1,B:0.1)D:0.2,C:0.15)E;\">\n"
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
"<IMG SRC=\"?phyloPng_width=200&phyloPng_height=200&phyloPng_tree=(((((((mouse,rat),human),(dog,cow)),opossum),chicken),xenopus),(tetraodon,zebrafish));\">\n"
"</td></tr>\n"
"<tr><td>\n"
"We have extended the Newick format to allow spaces <br>\n"
"and other non-alphanumeric characters in node labels.<br>\n"
"If you need a backslash, comma, semi-colon, colon, or parenthesis,<br>\n"
"it must be escaped with a back-slash character. <br>\n"
"<PRE>\n"
"((Brandt's myotis \\(bat\\):0.1,\n"
"  White-tailed eagle:0.1):0.2,\n"
" S. purpuratus:0.15);\n"
"</PRE></td><td>\n"
"<IMG SRC=\"?phyloPng_width=200&phyloPng_height=120&phyloPng_branchLengths=1&phyloPng_tree=((Brandt's myotis \\(bat\\):0.1,White-tailed eagle:0.1):0.2,S. purpuratus:0.15);\">\n"
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
    	usage("-phyloPng_tree is a required 'option' or cgi variable.");
    }

if (htmlPageWrapper)
    {
    printf("Content-type: text/html\r\n");
    printf("\r\n");
    puts("<html><head><title>Phylogenetic Tree</title></head><body>");
    printf("<IMAGE SRC=\"http://%s%s"
	    "?phyloPng_width=%d"
	    "&phyloPng_height=%d"
	    "&phyloPng_tree=%s"
	,getenv("SERVER_NAME"),getenv("SCRIPT_NAME"),width,height,phyloData);
    if (branchLengths)
	printf("&phyloPng_branchLengths=1");
    if (lengthLegend)
	printf("&phyloPng_lengthLegend=1");
    if (branchLabels)
	printf("&phyloPng_branchLabels=1");
    printf("&phyloPng_branchDecimals=%d",branchDecimals);
    printf("&phyloPng_branchMultipliers=%d",branchMultiplier);
    if (stripUnderscoreSuff)
	printf("&phyloPng_underscores=1");
    if (dashToSpace)
	printf("&phyloPng_dash_to_space=1");
    if (underToSpace)
	printf("&phyloPng_under_to_space=1");
    if (monospace)
	printf("&phyloPng_monospace=1");
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

/* remove carriage returns which are a side-effect of html forms */
if (strchr(phyloData,'\r'))
    {
    char *temp = phyloData;
    phyloData = replaceChars(temp,"\r","");
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
        htmlPrintf("Original input tree:\n[%s]\n\n",cgiString("phyloPng_tree"));
        htmlPrintf("Input tree as passed to parser:\n[%s]\n\n",phyloData);
        printf("Parser syntax error:\n%s",errMsg);
        puts("</pre></body></html>");
        }
    else
	{
	warn("%s", errMsg);
	}
    freez(&errMsg);    
    freez(&phyloData);
    return 0;
    }
}



MgFont *font = NULL;
if (monospace)
    font = mgMenloMediumFont();
else
    font = mgMediumBoldFont();


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
	    printf("input tree: [%s]\n\n%s",cgiString("phyloPng_tree"),layoutErrMsg);
	    puts("</pre></body></html>");
	    }
	else
	    {
	    warn("%s", layoutErrMsg);
	    }
	freez(&phyloData);
	mgFree(&mg);
	return 0;
	}

    if (branchLengths)
        phyloTreePngBL(phyloTree, maxDepth, numLeafs, maxLabelWidth, width, height, 
	    mg, font, minMaxFactor, FALSE);
    else	    
        phyloTreePng(phyloTree, maxDepth, numLeafs, maxLabelWidth, width, height, mg, font);

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
    printf("Content-type: image/png\r\n");
    printf("\r\n");
    }

if (!mgSaveToPng(stdout, mg, FALSE))
    {
    errAbort("Couldn't save png to stdout");
    }

if (cgiOptionalString("phyloPng_submit"))
    cartCheckout(&cart);

/* there's no code for freeing the phyloTree yet in phyloTree.c */

mgFree(&mg);
freez(&phyloData);


return 0;
}

