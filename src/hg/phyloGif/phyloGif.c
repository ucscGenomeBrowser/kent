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
 *    <IMG SRC="/cgi-bin/phyloGif?phyloGif_width=120&phyloGif_height=120&phyloGif_tree=(a:1,b:1);" >
 *
 *  Or as cgi in html POST:
<form method="POST" action="/cgi-bin/phyloGif/phylo.gif" name="mainForm">
<table>
<tr><td>width:</td><td><INPUT type="text" name="phyloGif_width" value="240" size="4"></td></tr>
<tr><td>height:</td><td><INPUT type="text" name="phyloGif_height" value="512" size="4"></td></tr>
<tr><td>tree:</td><td><textarea name="phyloGif_tree" rows=14 cols=80>
(((((((((
(human_hg18:0.006690,chimp_panTro1:0.007571):0.024272,
  macaque_rheMac2:0.0592):0.023960,
    ((rat_rn4:0.081728,mouse_mm8:0.077017):0.229273,
          rabbit_oryCun1:0.206767):0.1065):0.023026,
	  (cow_bosTau2:0.159182,dog_canFam2:0.147731):0.039450):0.028505,
	  armadillo_dasNov1:0.149862):0.015994,
	  (elephant_loxAfr1:0.104891,tenrec_echTel1:0.259797):0.040371):0.218400,
	  monodelphis_monDom4:0.371073):0.189124,
	  chicken_galGal2:0.454691):0.123297,
	  xenopus_xenTro1:0.782453):0.156067,
	  ((tetraodon_tetNig1:0.199381,fugu_fr1:0.239894):0.492961,
	      zebrafish_danRer3:0.782561):0.156067);
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


static char const rcsid[] = "$Id: phyloGif.c,v 1.7 2006/06/30 08:22:39 galt Exp $";

struct cart *cart=NULL;      /* The user's ui state. */
struct hash *oldVars = NULL;
boolean onWeb = FALSE;

int width=240,height=512;
boolean branchLengths = FALSE;  /* branch lengths */
boolean lengthLegend = FALSE;   /* length legend */
boolean branchLabels = FALSE;   /* labelled branch lengths */
boolean htmlPageWrapper = FALSE;  /* wrap output in an html page */
boolean preserveUnderscores = FALSE;   /* preserve underscores in input as spaces in output */
int branchDecimals = 2;         /* show branch label length to two decimals by default */
char *escapePattern = NULL;      /* use to escape dash '-' char in input */

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", NULL};

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort(
    "%s\n\n"
    "phylo - parse and display phylo tree from stdin to stdout\n"
    "\n"
    "Command-line Usage:\n"
    "   phyloGif [options] > phylo.gif \n"
    "Options/CGI-vars:\n"
    "  -phyloGif_width=N - width of output GIF, default %d\n"
    "  -phyloGif_height=N - height of output GIF, default %d\n"
    "  -phyloGif_tree= - data in format (nodeA:0.5,nodeB:0.6):1.2;\n"
    "     If running at the command-line, can put filename here or stdin\n"
    "     (this is actually required)\n"
    "  -phyloGif_branchLengths - use branch lengths for layout\n"
    "  -phyloGif_lengthLegend - show length legend at bottom\n"
    "  -phyloGif_branchLabels - show length of branch as label\n"
    "     (used with -phyloGif_branchLengths)\n"
    "  -phyloGif_branchDecimals=N - show length of branch to N decimals, default %d\n"
    "  -phyloGif_htmlPage - wrap the output in an html page (cgi only)\n"
    "  -phyloGif_underscores - preserve underscores in input as spaces in output\n"
    , msg, width, height, branchDecimals);
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
    errAbort("expected tree nodes to have 0 or 2 edges, found %d\n", phyloTree->numEdges); 

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
	safef(label,sizeof(label),patt,phyloTree->ident->length);  /* was %6.4f */
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

//cartWarnCatcher(doMiddle, cart, cartEarlyWarningHandler);


useCart = (!cgiOptionalString("phyloGif_tree"));

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
    preserveUnderscores = cgiVarExists("phyloGif_underscores");
    }
    
if (useCart)
    {
    if (onWeb)
	{
    	printf("Content-type: text/html\r\n");
	printf("\r\n");
	cartWebStart(cart, "%s", "phyloGif Interactive Phylogenetic Tree Gif Maker");
	printf("<form method=\"GET\" action=\"phyloGif\" name=\"mainForm\">");
	cartSaveSession(cart);
	puts("<table>");
	puts("<tr><td>Width:</td><td>"); cartMakeIntVar(cart, "phyloGif_width", width, 4); puts("</td></tr>");
	puts("<tr><td>Height:</td><td>"); cartMakeIntVar(cart, "phyloGif_height", height, 4); puts("</td></tr>");
	puts("<tr><td>Use branch lengths?</td><td>"); cartMakeCheckBox(cart, "phyloGif_branchLengths", branchLengths); puts("</td></tr>");
	puts("<tr><td>&nbsp; Show length legend?</td><td>"); cartMakeCheckBox(cart, "phyloGif_lengthLegend", lengthLegend); puts("</td></tr>");
	puts("<tr><td>&nbsp; Show length values?</td><td>"); cartMakeCheckBox(cart, "phyloGif_branchLabels", branchLabels); puts("</td></tr>");
	puts("<tr><td>&nbsp; How many decimal places?</td><td>"); cartMakeIntVar(cart, "phyloGif_branchDecimals", branchDecimals,1); puts("</td></tr>");
	puts("<tr><td>Preserve Underscores?</td><td>"); cartMakeCheckBox(cart, "phyloGif_underscores", preserveUnderscores); puts("</td></tr>");
	puts("<tr><td>Wrap in html page?</td><td>"); cartMakeCheckBox(cart, "phyloGif_htmlPage", htmlPageWrapper); puts("</td></tr>");

        printf("<tr><td>TREE:</td><td><textarea name=\"phyloGif_tree\" rows=14 cols=80>");
	if (NULL == phyloData || phyloData[0] == '\0')
	    {
	    puts(
"(((((((((\n"
"(human_hg18:0.006690,chimp_panTro1:0.007571):0.024272,\n"
"  macaque_rheMac2:0.0592):0.023960,\n"
"    ((rat_rn4:0.081728,mouse_mm8:0.077017):0.229273,\n"
"          rabbit_oryCun1:0.206767):0.1065):0.023026,\n"
"          (cow_bosTau2:0.159182,dog_canFam2:0.147731):0.039450):0.028505,\n"
"          armadillo_dasNov1:0.149862):0.015994,\n"
"          (elephant_loxAfr1:0.104891,tenrec_echTel1:0.259797):0.040371):0.218400,\n"
"          monodelphis_monDom4:0.371073):0.189124,\n"
"          chicken_galGal2:0.454691):0.123297,\n"
"          xenopus_xenTro1:0.782453):0.156067,\n"
"          ((tetraodon_tetNig1:0.199381,fugu_fr1:0.239894):0.492961,\n"
"              zebrafish_danRer3:0.782561):0.156067);"
		);
	    }
	else
	    {
	    puts(phyloData);
	    }
	puts("</TEXTAREA>\n");
	puts("</td></tr>");
	puts("<tr><td>&nbsp;</td><td><INPUT type=\"submit\" name=\"phyloGif_submit\" value=\"submit\"></td></tr>");
	puts("</table>");
	puts("</form>");
	webNewSection("Notes");
    	puts(
"<pre>\n"
"1. If a space is required in a node label, enter it as a dash.\n"
"\n"
"2. Underscores and anything following them are automatically stripped from node labels \n"
"unless the preserve-underscores checkbox is checked.\n"
"\n"
"3. Length-legend and length-values cannot be shown unless use-branch-lengths is also checked.\n"
"\n"
"4. The branch lengths are expected substitutions per site, allowing for\n"
"multiple hits.  So a branch length of 0.5 means an average of one\n"
"substitutions every two nucleotide sites, but the percent id will be\n"
"less than 50% because some of those substitutions are obscured by\n"
"subsequent substitutions.  They're estimated from neutral sites,\n"
"sometimes fourfold degenerate sites in coding regions, or sometimes\n"
"\"nonconserved\" sites according to phastCons.  The numbers are significant\n"
"to two or 3 figures.\n"
"</pre>"
	    );
	cartWebEnd();
	return 0;
	}
    else	    
    	usage("-phyloGif_tree is required 'option' or cgi variable.");
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

if (htmlPageWrapper)
    {
    printf("Content-type: text/html\r\n");
    printf("\r\n");
    printf("<html><head><title></title></head><body>\n");
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
    if (preserveUnderscores)
	printf("&phyloGif_underscores=1");
    printf("\"></body></html>\n");
    freez(&phyloData);
    return 0;
    }


mg = mgNew(width,height);
mgClearPixels(mg);

lengthLegend = lengthLegend && branchLengths;  /* moot without lengths */

if (lengthLegend)
    {
    int fHeight = mgFontPixelHeight(font);
    height -= (MARGIN+2*fHeight);
    }

phyloTree = phyloParseString(phyloData);
if (phyloTree)
    {

    phyloTreeLayoutBL(phyloTree, &maxDepth, &numLeafs, 0, font, &maxLabelWidth, width, &minMaxFactor, 0.0);
    
    if (branchLengths)
        phyloTreeGifBL(phyloTree, maxDepth, numLeafs, maxLabelWidth, width, height, mg, font, minMaxFactor, FALSE);
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
		safef(out,sizeof(out),"%3.2f",i/10.0);
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

