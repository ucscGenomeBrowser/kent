#include "common.h"
#include "cart.h"

static char *edit1affyArchiveTracks[] =
{
"wgEncodeAffyRnaChip",
"affyU133Plus2",
"affyGnf1h",
"affyExonArray",
"affyU95",
"affyU133",
};

static char *edit1genePredArchiveTracks[] =
{
"geneid",
"genscan",
"augustusGene",
"sgpGene",
"nscanGene",
"acembly",
"sibGene",
};

static char *edit1tgpArchiveTracks[] =
{
"tgpTrios",
"tgpPhase1",
"tgpPhase3",
"tgpPhase1Accessibility",
"tgpPhase3Accessibility",
};

void cartEdit1(struct cart *cart)
{
cartTurnOnSuper(cart, edit1affyArchiveTracks,  ArraySize(edit1affyArchiveTracks),  "affyArchive");
cartTurnOnSuper(cart, edit1genePredArchiveTracks,  ArraySize(edit1genePredArchiveTracks),  "genePredArchive");
cartTurnOnSuper(cart, edit1tgpArchiveTracks,  ArraySize(edit1tgpArchiveTracks),  "tgpArchive");
}
