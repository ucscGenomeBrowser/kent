
#include "retroGene.h"
//#include "hgTracks.h"

static char const rcsid[] = "$Id: retroGene.c,v 1.1 2005/03/24 05:31:11 baertsch Exp $";

struct linkedFeatures *lfFromRetroGene(struct pseudoGeneLink *pg)
/* Return a linked feature from a retroGene. */
{
struct linkedFeatures *lf;
struct simpleFeature *sf, *sfList = NULL;
int grayIx = grayInRange(pg->score, 0, 1000);
int *starts = pg->chromStarts, start;
int *sizes = pg->blockSizes;
int blockCount = pg->blockCount, i;

assert(starts != NULL && sizes != NULL && blockCount > 0);

AllocVar(lf);
lf->grayIx = grayIx;
strncpy(lf->name, pg->name, sizeof(lf->name));
lf->orientation = orientFromChar(pg->strand[0]);
for (i=0; i<blockCount; ++i)
    {
    AllocVar(sf);
    start = starts[i] + pg->chromStart;
    sf->start = start;
    sf->end = start + sizes[i];
    sf->grayIx = grayIx;
    slAddHead(&sfList, sf);
    }
slReverse(&sfList);
lf->components = sfList;
linkedFeaturesBoundsAndGrays(lf);
lf->tallStart = pg->thickStart;
lf->tallEnd = pg->thickEnd;
lf->extra = pg->refSeq;
return lf;
}
void lfRetroGene(struct track *tg)
/* Load the items in one custom track - just move beds in
 * window... */
{
struct linkedFeatures *lfList = NULL, *lf;
struct pseudoGeneLink *pg = NULL, *list = NULL;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    pg = pseudoGeneLinkLoad(row+rowOffset);
    slAddHead(&list, pg);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
//slReverse(&list);

for (pg = list; pg != NULL; pg = pg->next)
    {
    //struct simpleFeature *sf; int i;
    lf = lfFromRetroGene(pg);
    lf->grayIx = 9;
    slReverse(&lf->components);
    //for (sf = lf->components, i = 0; sf != NULL && i < bed->expCount;
//		sf = sf->next, i++)
//	{
//	sf->grayIx = bed->expIds[i];
	//sf->grayIx = grayInRange((int)(bed->expIds[i]),11,13);
//	}
    slAddHead(&lfList,lf);
    }
tg->items = lfList;
}

void lookupRetroNames(struct track *tg)
/* This converts the accession to a gene name where possible. */
{
struct linkedFeatures *lf;
struct sqlConnection *conn = hAllocConn();
boolean isNative = sameString(tg->mapName, "pseudoGeneLink");
char *refGeneLabel = cartUsualString(cart, (isNative ? "pseudoGeneLink.label" : "xenoRefGene.label"), "gene");
boolean useGeneName = sameString(refGeneLabel, "gene")
    || sameString(refGeneLabel, "both");
boolean useAcc = sameString(refGeneLabel, "accession")
    || sameString(refGeneLabel, "both");

for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    struct dyString *name = dyStringNew(64);
    if ((useGeneName || useAcc) && !isNative)
        {
        /* append upto 7 chars of org, plus a space */
        char *org = getOrganism(conn, lf->name);
        org = firstWordInLine(org);
        if (org != NULL)
            dyStringPrintf(name, "%0.7s ", org);
        }
    if (useGeneName)
        {
        char *gene = getGeneName(conn, lf->extra);
        if (gene != NULL)
            {
            dyStringAppend(name, gene);
            if (useAcc)
                dyStringAppendC(name, '/');
            }
        }
    if (useAcc)
        dyStringAppend(name, lf->name);
    lf->extra = dyStringCannibalize(&name);
    }
hFreeConn(&conn);
}

void loadRetroGene(struct track *tg)
/* Load up RetroGenes. */
{
enum trackVisibility vis = tg->visibility;
//tg->items = lfFromBed(tg, tg->mapName, chromName, winStart, winEnd);
lfRetroGene(tg);
if (vis != tvDense)
    {
    lookupRetroNames(tg);
    slSort(&tg->items, linkedFeaturesCmpStart);
    }
vis = limitVisibility(tg);
}

void retroGeneMethods(struct track *tg)
/* Make track of retroGenes from bed */
{
tg->loadItems = loadRetroGene;
tg->itemName = refGeneName;
tg->mapItemName = refGeneMapName;
//tg->itemColor = refGeneColor;
}

