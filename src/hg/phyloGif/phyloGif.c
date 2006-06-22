/* phyloGif.c for parsing phylo tree and outputting a gif.
 *
 *  Author: Galt Barber 2006 
 * 
 *  Designed to be run either as a cgi or as a command-line utility.
 *  The input file spec matches the .nh tree format e.g. (cat:0.5,dog:0.6):1.2;
 *  The output gif layout ignores branch lengths by design.
 *  Any _suffix is stripped from labels both because pyloTree.c 
 *  can't tolerate underscores and because we don't want suffixes anyway.
 *  A semi-colon is automatically appended to the input if left off.
 *  Because phyloTree.c does errAbort on bad input, this causes cgi err 500
 *  if the input data has incorrect syntax.  See the apache error_log.
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

static char const rcsid[] = "$Id: phyloGif.c,v 1.1 2006/06/22 19:13:42 galt Exp $";

int width=240,height=512;

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
    "  -phyloGif_width - width of output GIF, default %d\n"
    "  -phyloGif_height - height of output GIF, default %d\n"
    "  -phyloGif_tree - data in format (nodeA:0.5,nodeB:0.6):1.2;\n"
    "     If running at the command-line, can put filename here or stdin.\n"
    "     (this is actually required).\n"
    , msg, width, height);
}

struct phyloLayout
{
    int depth;          /* leaves have depth=0 */
    double vPos;        /* vertical position   */
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


static void phyloTreeLayout(struct phyloTree *phyloTree, 
    int *pMaxDepth, int *pNumLeafs, int depth,
    MgFont *font, int *pMaxLabelWidth)
/* do a depth-first recursion over the tree, assigning depth and vPos to each node
 * and keeping track of maxDepth and numLeafs to return */
{
struct phyloLayout *this = NULL;
if (depth > *pMaxDepth)
    *pMaxDepth = depth;
phyloTree->priv = (void *) needMem(sizeof(struct phyloLayout));
this = (struct phyloLayout *) phyloTree->priv;
if (phyloTree->numEdges == 2)  /* node */
    {
    int maxDepth = 0;
    struct phyloLayout *that = NULL;
    double vPos = 0;
    phyloTreeLayout(phyloTree->edges[0], pMaxDepth, pNumLeafs, depth+1, font, pMaxLabelWidth);
    phyloTreeLayout(phyloTree->edges[1], pMaxDepth, pNumLeafs, depth+1, font, pMaxLabelWidth);
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
    this->depth=0;
    this->vPos=*pNumLeafs;
    (*pNumLeafs)++;
    w=mgFontStringWidth(font,phyloTree->ident->name);
    if (w > *pMaxLabelWidth)
	*pMaxLabelWidth = w;
    }
else
    errAbort("expected tree nodes to have 0 or 2 edges, found %d\n", phyloTree->numEdges); 

}

#define MARGIN 10

static void phyloTreeGif(struct phyloTree *phyloTree, 
    int maxDepth, int numLeafs, int maxLabelWidth, 
    int width, int height, struct memGfx *mg, MgFont *font)
/* do a depth-first recursion over the tree, printing tree to gif
 *  */
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

//debug
//fprintf(stderr,"name=%s depth=%d vPos=%f\n", phyloTree->ident->name, this->depth, this->vPos);

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

int main(int argc, char *argv[])
{
char *phyloData = NULL, *temp = NULL;
struct phyloTree *phyloTree = NULL;
int maxDepth = 0, numLeafs = 0, maxLabelWidth = 0;
struct memGfx *mg = NULL;
MgFont *font = mgMediumBoldFont();
boolean onWeb = cgiIsOnWeb();
if (onWeb && sameString(getenv("REQUEST_METHOD"),"HEAD"))
    { /* tell browser it's static just so it can save it */
    printf("Content-type: image/gif\r\n");
    printf("\r\n");
    return 0;
    }
cgiSpoof(&argc, argv);
if (argc != 1)
    usage("wrong number of args");

width = cgiOptionalInt("phyloGif_width",width);    
height = cgiOptionalInt("phyloGif_height",height);    
phyloData = cloneString(cgiOptionalString("phyloGif_tree"));
if (!phyloData)
    usage("-phyloGif_tree is required 'option' or cgi variable.");
//debug
//fprintf(stderr,"width=%d height=%d\n%s\n-------------\n", width, height, phyloData);

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

/* get rid of underscore suffixes */
stripUnderscoreSuffixes(phyloData);
//debug
//fprintf(stderr,"%s\n-------------\n", phyloData);

/* add trailing semi-colon if it got stripped off */
if (lastChar(phyloData) !=  ';')
    {
    temp = phyloData;
    phyloData = addSuffix(phyloData,";");
    freez(&temp);
    }

mg = mgNew(width,height);

phyloTree = phyloParseString(phyloData);
if (phyloTree)
    {
    //phyloDebugTree(phyloTree,stderr);
    //phyloPrintTree(phyloTree,stderr);

    phyloTreeLayout(phyloTree, &maxDepth, &numLeafs, 0, font, &maxLabelWidth);
    //debug
    //fprintf(stderr,"maxDepth=%d numLeafs=%d maxLabelWidth=%d\n", maxDepth, numLeafs, maxLabelWidth);

    phyloTreeGif(phyloTree, maxDepth, numLeafs, maxLabelWidth, width, height, mg, font);
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


/* there's no code for freeing the phyloTree yet in phyloTree.c */

mgFree(&mg);
freez(&phyloData);
return 0;
}

