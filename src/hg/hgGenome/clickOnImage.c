/* clickOnImage - handle click on image. Calculate position in genome
 * and forward to genome browser. */

#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "chromGraph.h"
#include "genoLay.h"
#include "binRange.h"
#include "hdb.h"
#include "hgGenome.h"

static char const rcsid[] = "$Id: clickOnImage.c,v 1.5 2008/09/17 18:36:35 galt Exp $";

struct genoLayChrom *genoLayChromAt(struct genoLay *gl, int x, int y)
/* Return chromosome if any at x,y */
{
struct genoLayChrom *chrom;
for (chrom = gl->chromList; chrom != NULL; chrom = chrom->next)
    {
    if (chrom->x <= x && x < chrom->x + chrom->width
       && chrom->y <= y && y < chrom->y + chrom->height)
       break;
    }
return chrom;
}

void clickOnImage(struct sqlConnection *conn)
/* Handle click on image - calculate position in forward to genome browser. */
{
int x = cartInt(cart, hggClickX);
int y = cartInt(cart, hggClickY);
struct genoLay *gl = ggLayout(conn, linesOfGraphs(), graphsPerLine());
struct genoLayChrom *chrom = genoLayChromAt(gl, x, y);
if (chrom != NULL)
    {
    int base = gl->basesPerPixel*(x - chrom->x);
    int start = base-500000, end=base+500000;
    if (start<0) start=0;
    printf("Location: ../cgi-bin/hgTracks?db=%s&%s&position=%s:%d-%d&hgGenomeClick=image\r\n\r\n",
    	database, cartSidUrlString(cart), chrom->fullName, start+1, end);
    }
else
    {
    hggDoUsualHttp();
    }
}
