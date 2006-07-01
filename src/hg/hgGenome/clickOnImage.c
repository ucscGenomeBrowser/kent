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
    cartWebStart(cart, "Theoretically processing image click");
    int base = gl->basesPerPixel*(x - chrom->x);
    uglyf("Click at %d %d in %s %d<BR>\n", x, y, chrom->fullName, base);
    cartWebEnd();
    }
else
    {
    mainPage(conn);
    }
}
