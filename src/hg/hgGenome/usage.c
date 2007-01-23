/* usage - functions that display helpful usage info text go here. -jk)*/

#include "common.h"
#include "hPrint.h"

static char const rcsid[] = "$Id: usage.c,v 1.1 2007/01/23 17:59:22 kent Exp $";

void printMainHelp()
/* Put up main page help info. */
{
hPrintf("%s",

"<P>Genome Graphs is a tool for displaying genome-wide data sets such\n"
"as the results of genome-wide SNP association studies, linkage studies\n"
"and homozygousity mapping. This section provided brief line-by-line\n"
"descriptions of the controls on this page.\n"
"        <UL>\n"
"        <LI><B>clade: </B>Specifies which clade the organism is in."
"        </LI>\n"
"        <LI><B>genome: </B>Specifies which organism data to use."
"        </LI>\n"
"        <LI><B>assembly: </B>Specifies which version of the organism's"
"        genome sequence to use.\n"
"        </LI>\n"
"        <LI><B>graph ... in ...:</B> Selects which graph(s) to display in "
"        which color."
"        </LI>\n"
"        <LI><B>upload: </B> Takes you to a page where you can upload your "
"        own data.\n"
"        </LI>\n"
"        <LI><B>configure: </B> Takes you to a page where you can control "
"        many aspects of the program including the overall size of the image, "
"        how many graphs can be drawn, and the chromosome layout."
"        </LI>\n"
"        <LI><B>significance threshold: </B> Values over this threshold will "
"        be considered significant. A light blue line will be drawn across "
"        the graphs at the significance threshold of the first graph. Regions "
"        over the threshold will be included in the region list you get with "
"        the <B>browse regions</B> button."
"        </LI>\n"
"        <LI><B>browse regions: </B> Takes you to a page with a list of all "
"        regions over the significance threshold on the left, and a Genome "
"        Browser on the right.  Clicking on a region will move the browser's "
"        window to that region."
"        </LI>\n"
"        <LI><B>sort genes: </B> Figures out all of the genes in regions over "
"        the significance threshold, and takes you to the Gene Sorter with a "
"        filter set up to focus on only those genes."
"        </LI>\n"
"        </UL>\n");
}
