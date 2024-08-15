
#include "common.h"
#include "cart.h"

static char *edit3tracksCovid[] =
{
"covidMuts",
"covidHgiGwasR4Pval",
"covidHgiGwas"
};

static char *edit3tracksAssembly[] =
{
"scaffolds",
"ctgPos2",
"stsMap"
"traceClone"
"fishClones"
};

static char *edit3tracksMgcOrfeome[] =
{
"orfeomeMrna",
"mgcFullMrna",
};



void cartEdit3(struct cart *cart)
{
int length = ArraySize(edit3tracksCovid);
cartTurnOnSuper(cart, edit3tracksCovid, length, "covid");

length = ArraySize(edit3tracksAssembly);
cartTurnOnSuper(cart, edit3tracksAssembly, length, "assemblyContainer");

length = ArraySize(edit3tracksMgcOrfeome);
cartTurnOnSuper(cart, edit3tracksMgcOrfeome, length, "mgcOrfeomeMrna");
}
