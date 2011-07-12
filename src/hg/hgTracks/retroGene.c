
#include "retroGene.h"
#include "transMapStuff.h"

static char const rcsid[] = "$Id: retroGene.c,v 1.19 2010/05/11 01:43:28 kent Exp $";

static void addToLabel(struct dyString *label, char *val)
/* append a label to the label, separating with a space, do nothing if val is
 * NULL.*/
{
if (val != NULL)
    {
    if (label->stringSize > 0)
        dyStringAppendC(label, ' ');
    dyStringAppend(label, val);
    }
}

/* bit set of labels to use */
enum {useOrgCommon = 0x01,
      useOrgAbbrv  = 0x02,
      useOrgDb     = 0x04,
      useGene      = 0x08,
      useAcc       = 0x10};

struct linkedFeatures *lfFromRetroGene(struct ucscRetroInfo *pg)
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
lf->name = cloneString(pg->name);
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
struct ucscRetroInfo *pg = NULL, *list = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;
int colCount ;

sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL, &rowOffset);
colCount = sqlCountColumns(sr);
while ((row = sqlNextRow(sr)) != NULL)
    {
    //if (colCount == 56) 
    //    pg = retroMrnaInfo56Load(row+rowOffset);
    //else
    pg = ucscRetroInfoLoad(row+rowOffset);
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

char *getRetroParentSymbol(struct ucscRetroInfo *r, char *parentName)
{
struct sqlConnection *conn = hAllocConn(database);
char cond_str[512];
char *geneSymbol = NULL;
if (r != NULL)
    {
    if (hTableExists(database, "kgXref") )
        {
        safef(cond_str, sizeof(cond_str), "kgID='%s'", parentName);
        geneSymbol = sqlGetField(database, "kgXref", "geneSymbol", cond_str);
        }

    if (hTableExists(database, "refLink") )
        {
        safef(cond_str, sizeof(cond_str), "mrnaAcc = '%s'", r->refSeq);
        geneSymbol = sqlGetField(database, "refLink", "name", cond_str);
        }
    }
hFreeConn(&conn);
return geneSymbol;
}
char *retroName(struct track *tg, void *item)
/* Get name to use for retroGene item. */
{
struct linkedFeatures *lf = item;
char cartvar[512];
safef(cartvar, sizeof(cartvar), "%s.label", tg->table);
char *refGeneLabel = cartUsualString(cart, cartvar, "gene") ;
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
   geneSymbol = getRetroParentSymbol(lf->extra, lf->name);
   if (geneSymbol != NULL)
        {
        dyStringAppend(name, geneSymbol);
        if (useAcc)
            dyStringAppendC(name, '/');
        }
   }
if ((useGeneName) && sameString(name->string, "retro-"))
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
}

static unsigned getLabelTypes(struct track *tg)
/* get set of labels to use */
{
unsigned labelSet = 0;

// label setting are on parent track
char prefix[128];
safef(prefix, sizeof(prefix), "%s.label", tg->tdb->track);
struct hashEl *labels = cartFindPrefix(cart, prefix);

if (labels == NULL)
    {
    // default to common name+accession and save this in cart so it makes sense in trackUi
    labelSet = useAcc;
    char setting[64];
    safef(setting, sizeof(setting), "%s.label.gene", tg->tdb->track);
    cartSetBoolean(cart, setting, TRUE);
    }
struct hashEl *label;
for (label = labels; label != NULL; label = label->next)
    {
    if (endsWith(label->name, ".orgCommon") && differentString(label->val, "0"))
        labelSet |= useOrgCommon;
    else if (endsWith(label->name, ".orgAbbrv") && differentString(label->val, "0"))
        labelSet |= useOrgAbbrv;
    else if (endsWith(label->name, ".db") && differentString(label->val, "0"))
        labelSet |= useOrgDb;
    else if (endsWith(label->name, ".gene") && differentString(label->val, "0"))
        labelSet |= useGene;
    else if (endsWith(label->name, ".acc") && differentString(label->val, "0"))
        labelSet |= useAcc;
    }
return labelSet;
}
static void getItemLabel(struct sqlConnection *conn,
                         char *retroInfoTbl , //char *transMapGeneTbl,
                         unsigned labelSet,
                         struct linkedFeatures *lf)
/* get label for a retro item */
{
struct ucscRetroInfo *info = NULL;
info = ucscRetroInfoQuery(conn, retroInfoTbl, lf->name);
char *geneSymbol = getRetroParentSymbol(info, lf->name);
//lf->extra = geneSymbol;

struct dyString *label = dyStringNew(64);
if ((labelSet & useGene) && (retroInfoTbl != NULL))
    {
    if ((geneSymbol != NULL) && (strlen(geneSymbol) > 0))
        addToLabel(label, geneSymbol);
    else
        labelSet |= useAcc;  // no gene, so force acc
    }
if (labelSet & useAcc)
    addToLabel(label, lf->name);

ucscRetroInfoFree(&info);
lf->extra = dyStringCannibalize(&label);
}

static void lookupRetroAliLabels(struct track *tg)
/* This converts the retro ids to labels. */
{
struct sqlConnection *conn = hAllocConn(database);
char *retroInfoTbl = trackDbRequiredSetting(tg->tdb, retroInfoTblSetting);

struct linkedFeatures *lf;
unsigned labelSet = getLabelTypes(tg);

for (lf = tg->items; lf != NULL; lf = lf->next)
    getItemLabel(conn, retroInfoTbl, labelSet, lf);
hFreeConn(&conn);
}

static void loadRetroAli(struct track *tg)
/* Load up retro alignments. */
{
loadXenoPsl(tg);
enum trackVisibility vis = limitVisibility(tg);
if (!((vis == tvDense) || (vis == tvSquish)))
    lookupRetroAliLabels(tg);
if (vis != tvDense)
    slSort(&tg->items, linkedFeaturesCmpStart);
}

char *transMapIdToAcc(char *id)
/* remove all unique suffixes (starting with last `-') from any TransMap 
 * id.  WARNING: static return */
{
static char acc[128];
safecpy(acc, sizeof(acc), id);
char *dash = strrchr(acc, '-');
if (dash != NULL)
    *dash = '\0';
return acc;
}

static char *transMapGetItemDataName(struct track *tg, char *itemName)
/* translate itemName to data name (source accession).
 * WARNING: static return */
{
return transMapIdToAcc(itemName);
}

static void retroAliMethods(struct track *tg)
/* Make track for retroGene psl alignments. */
{
tg->loadItems = loadRetroAli;
tg->itemName = refGeneName;
tg->mapItemName = refGeneMapName;
tg->itemDataName = transMapGetItemDataName;
}

void retroRegisterTrackHandlers()
{
//registerTrackHandler("pseudoGeneLink", retroGeneMethods);
//registerTrackHandler("pseudoGeneLink2", retroGeneMethods);
//registerTrackHandler("retroMrnaInfo", retroGeneMethods);
//registerTrackHandler("retroMrnaInfo2", retroGeneMethods);
//registerTrackHandler("retroMrnaInfo3", retroGeneMethods);
registerTrackHandler("ucscRetroInfo", retroGeneMethods);
registerTrackHandler("ucscRetroInfo1", retroGeneMethods);
registerTrackHandler("ucscRetroInfo2", retroGeneMethods);
registerTrackHandler("ucscRetroInfo3", retroGeneMethods);
//registerTrackHandler("retroCdsAli", retroAliMethods);
//registerTrackHandler("retroCdsAli3", retroAliMethods);
registerTrackHandler("ucscRetroAli", retroAliMethods);
registerTrackHandler("ucscRetroAli1", retroAliMethods);
registerTrackHandler("ucscRetroAli2", retroAliMethods);
registerTrackHandler("ucscRetroAli3", retroAliMethods);
}
