
#include "retroGene.h"

static char const rcsid[] = "$Id: retroGene.c,v 1.12.26.1 2008/07/31 02:24:15 markd Exp $";

struct linkedFeatures *lfFromRetroGene(struct retroMrnaInfo *pg)
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
safef(lf->name, sizeof(lf->name), "%s", pg->name);
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
lf->extra = pg;
return lf;
}
void lfRetroGene(struct track *tg)
/* Load the items in one custom track - just move beds in
 * window... */
{
struct linkedFeatures *lfList = NULL, *lf;
struct retroMrnaInfo *pg = NULL, *list = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;
int colCount ;

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
colCount = sqlCountColumns(sr);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (colCount == 56)
        pg = retroMrnaInfo56Load(row+rowOffset);
    else
        pg = retroMrnaInfoLoad(row+rowOffset);
    slAddHead(&list, pg);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);

for (pg = list; pg != NULL; pg = pg->next)
    {
    lf = lfFromRetroGene(pg);
    lf->grayIx = 9;
    slReverse(&lf->components);
    slAddHead(&lfList,lf);
    }
tg->items = lfList;
}

#ifdef junk
void lookupRetroNames(struct track *tg)
/* This converts the accession to a gene name where possible. */
{
struct linkedFeatures *lf;
struct sqlConnection *conn = hAllocConn(database);
boolean isNative = sameString(tg->mapName, "pseudoGeneLink") ;
boolean isRetroNative = sameString(tg->mapName, "retroMrnaInfo");
char *refGeneLabel = cartUsualString(cart, (isNative ? "pseudoGeneLink.label" : (isRetroNative ? "retroMrnaInfo.label" : "xenoRefGene.label")), "gene") ;
boolean useGeneName = sameString(refGeneLabel, "gene")
    || sameString(refGeneLabel, "both");
boolean useAcc = sameString(refGeneLabel, "accession")
    || sameString(refGeneLabel, "both");

for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    struct dyString *name = dyStringNew(64);
    char *geneName = lf->extra;
    if (geneName == NULL)
        geneName = lf->name;
    if ((useGeneName || useAcc) && !isNative)
        {
        char *org = getOrganismShort(conn, lf->name);
        if (org != NULL)
            dyStringPrintf(name, "%s ", org);
        }
    /* prepend retro- to parent gene name */
    dyStringAppend(name, "retro-");
    /* lookup name using knowngene method */
    if ((useGeneName) && hTableExists("kgXref") )
        {
        char *gene = NULL;
        char cond_str[256];
        char *geneSymbol;
        safef(cond_str, sizeof(cond_str), "kgID='%s'", lf->name);
        geneSymbol = sqlGetField(conn, database, "kgXref", "geneSymbol", cond_str);

        if (geneSymbol != NULL)
            gene = cloneString(geneSymbol);
        if (gene != NULL)
            {
            dyStringAppend(name, gene);
            if (useAcc)
                dyStringAppendC(name, '/');
            }
        else
            {
            if (hTableExists("refLink"))
                {
                safef(cond_str, sizeof(cond_str), "mrnaAcc = '%s'", lf->name);
                geneSymbol = sqlGetField(conn, database, "refLink", "name", cond_str);
                if (geneSymbol != NULL)
                    gene = cloneString(geneSymbol);
                }
            }
        if (gene != NULL)
            dyStringAppend(name, gene);
        else
            dyStringAppend(name, geneName);
        }
    if (useAcc)
        dyStringAppend(name, geneName);
    //lf->extra = dyStringCannibalize(&name);
    }
hFreeConn(&conn);
}
#endif

void loadRetroGene(struct track *tg)
/* Load up RetroGenes. */
{
enum trackVisibility vis = tg->visibility;
lfRetroGene(tg);
if (vis != tvDense)
    {
    slSort(&tg->items, linkedFeaturesCmpStart);
    }
vis = limitVisibility(tg);
}

Color retroGeneColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw retroGene in. */
{
return hvGfxFindColorIx(hvg, CHROM_20_R, CHROM_20_G, CHROM_20_B);
}

char *retroName(struct track *tg, void *item)
/* Get name to use for retroGene item. */
{
struct linkedFeatures *lf = item;
boolean isNative = sameString(tg->mapName, "pseudoGeneLink") ;
boolean isRetroNative = sameString(tg->mapName, "retroMrnaInfo");
char *refGeneLabel = cartUsualString(cart, (isNative ? "pseudoGeneLink.label" : (isRetroNative ? "retroMrnaInfo.label" : "xenoRefGene.label")), "gene") ;
boolean useGeneName = sameString(refGeneLabel, "gene")
    || sameString(refGeneLabel, "both");
boolean useAcc = sameString(refGeneLabel, "accession")
    || sameString(refGeneLabel, "both");
struct dyString *name = dyStringNew(64);
if  (useAcc || useGeneName)
    dyStringAppend(name, "retro-");
char *geneSymbol = NULL;
if (lf->extra != NULL) 
    {
    struct sqlConnection *conn = hAllocConn(database);
    struct retroMrnaInfo *r = lf->extra;
    char cond_str[512];
    if (r != NULL)
        {
        if ((useGeneName) && hTableExists(database, "kgXref") )
            {
            char *gene = NULL;
            char cond_str[256];
            char *geneSymbol;
            safef(cond_str, sizeof(cond_str), "kgID='%s'", lf->name);
            geneSymbol = sqlGetField(database, "kgXref", "geneSymbol", cond_str);

            if (geneSymbol != NULL)
                gene = cloneString(geneSymbol);
            if (gene != NULL)
                {
                dyStringAppend(name, gene);
                if (useAcc)
                    dyStringAppendC(name, '/');
                }
            }
        if ((useGeneName) && hTableExists(database, "refLink") )
            {
            safef(cond_str, sizeof(cond_str), "mrnaAcc = '%s'", r->refSeq);
            geneSymbol = sqlGetField(database, "refLink", "name", cond_str);
            if (geneSymbol != NULL)
                {
                dyStringAppend(name, geneSymbol);
                if (useAcc)
                    dyStringAppendC(name, '/');
                }
            }
        }
    hFreeConn(&conn);
    }
if ((useGeneName) && geneSymbol == NULL)
    dyStringAppend(name, lf->name);
if (useAcc)
    dyStringAppend(name, lf->name);
return dyStringCannibalize(&name);
}

void retroGeneMethods(struct track *tg)
/* Make track of retroGenes from bed */
{
tg->loadItems = loadRetroGene;
tg->itemName = retroName;
tg->mapItemName = refGeneMapName;
tg->itemColor = retroGeneColor;
tg->exonArrows = TRUE;
tg->exonArrowsAlways = TRUE;
}

