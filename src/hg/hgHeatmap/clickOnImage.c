/* clickOnImage - handle click on image. Calculate position in genome
 * and forward to genome browser. */

#include "common.h"
#include "cart.h"
#include "genoLay.h"
#include "hgHeatmap.h"

#define CLICKSPAN 1500000

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
int x = cartInt(cart, hghClickX);
int y = cartInt(cart, hghClickY);
struct genoLay *gl = ggLayout(conn);
struct genoLayChrom *chrom = genoLayChromAt(gl, x, y);
if (chrom != NULL)
    {
    int base = gl->basesPerPixel*(x - chrom->x);
    int start = base-CLICKSPAN, end=base+CLICKSPAN;
    if (start<0) start=0;
    /* demo code to direct to the Myc region */
    if ( sameWord(chrom->fullName,"chr8") && (start > 70000000) )
    {
    start = 127300000;
    end=131500000;
    }
    printf("Location: ../cgi-bin/hgTracks?db=%s&%s&position=%s:%d-%d\r\n\r\n",
	   theDatabase, cartSidUrlString(cart), chrom->fullName, start+1, end);
    }
else
    {
    hghDoUsualHttp();
    }
}
