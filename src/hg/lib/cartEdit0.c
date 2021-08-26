
#include "common.h"
#include "cart.h"

static char *edit0Mm10tracks[] =
{
"snp142",
"snp142Mult",
"snp138",
"snp138Common",
"snp138Mult",
"snp137Common",
"snp137Mult",
"snp137",
};

static char *edit0Hg19tracks[] =
{
"snp151Flagged",
"snp151Mult",
"snp150Mult",
"snp150Flagged",
"snp150",
"snp150Common",
"snp149Mult",
"snp149Flagged",
"snp149",
"snp149Common",
"snp147Flagged",
"snp147Mult",
"snp147Common",
"snp147",
"snp146Mult",
"snp146Flagged",
"snp146",
"snp146Common",
"snp144Mult",
"snp144Flagged",
"snp144Common",
"snp144",
"snp142Mult",
"snp142Flagged",
"snp142Common",
"snp142",
"snp141Flagged",
"snp141Common",
"snp141",
"snp138Flagged",
"snp138Mult",
"snp138",
"snp138Common",
"snp137Common",
"snp137Flagged",
"snp137Mult",
"snp137",
"snp135Common",
"snp135Flagged",
"snp135Mult",
"snp135",
"snp132Common",
"snp132Flagged",
"snp132Mult",
"snp132",
"snp131",
"snp130",
};

static char *edit0Hg38tracks[] =
{
"snp151",
"snp151Common",
"snp151Flagged",
"snp151Mult",
"snp150Mult",
"snp150",
"snp150Common",
"snp150Flagged",
"snp149Mult",
"snp149",
"snp149Common",
"snp149Flagged",
"snp141Mult",
"snp141Flagged",
"snp141Common",
"snp141",
"snp142Mult",
"snp142Flagged",
"snp142Common",
"snp142",
"snp144Mult",
"snp144Flagged",
"snp144Common",
"snp144",
"snp147Mult",
"snp147Flagged",
"snp147Common",
"snp147",
"snp146Mult",
"snp146Flagged",
"snp146Common",
"snp146",
};

void cartEdit0(struct cart *cart)
/* Moving a bunch of SNP tracks to an archive super track.   We need
 * to turn on the super track if any of what are now subTracks are visible.
 */
{
// hg38 tracks
int length = ArraySize(edit0Hg38tracks);
cartTurnOnSuper(cart, edit0Hg38tracks, length, "dbSnpArchive");

// mm10 tracks
length = ArraySize(edit0Mm10tracks);
cartTurnOnSuper(cart, edit0Mm10tracks, length, "dbSnpArchive");

// hg19 tracks
length = ArraySize(edit0Hg19tracks);
cartTurnOnSuper(cart, edit0Hg19tracks, length, "dbSnpArchive");
}
