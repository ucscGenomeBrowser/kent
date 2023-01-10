
#include "common.h"
#include "cart.h"

static char *edit2Hg19tracks[] =
{
"dbSnp153Composite",
};

static char *edit2Hg38tracks[] =
{
"dbSnp153Composite",
};

void cartEdit2(struct cart *cart)
/* Moving a SNP 153 to an archive super track.   We need
 * to turn on the super track if dbSnp153 was on before.
 */
{
// hg38 tracks
int length = ArraySize(edit2Hg38tracks);
cartTurnOnSuper(cart, edit2Hg38tracks, length, "dbSnpArchive");

// hg19 tracks
length = ArraySize(edit2Hg19tracks);
cartTurnOnSuper(cart, edit2Hg19tracks, length, "dbSnpArchive");
}
