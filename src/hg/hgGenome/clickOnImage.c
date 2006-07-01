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


void clickOnImage(struct sqlConnection *conn)
/* Handle click on image - calculate position in forward to genome browser. */
{
cartWebStart(cart, "Theoretically processing image click");
cartWebEnd();
}
