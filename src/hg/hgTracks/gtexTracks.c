/* GTEx (Genotype Tissue Expression) tracks  */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hgTracks.h"
#include "hvGfx.h"
#include "rainbow.h"
#include "gtexInfo.h"
#include "gtexGeneBed.h"
#include "gtexTissue.h"
#include "gtexTissueData.h"
#include "gtexUi.h"
#include "spaceSaver.h"

struct gtexGeneExtras 
/* Track info */
    {
    char *version;              /* Suffix to table name, e.g. 'V6' */
    double maxMedian;           /* Maximum median rpkm for all tissues */
    boolean isComparison;       /* Comparison of two sample sets (e.g. male/female). */
    boolean isDifference;       /* True if comparison is shown as a single difference graph. 
                                   False if displayed as two graphs, one oriented downward */
    char *graphType;            /* Additional info about graph (e.g. type of comparison graph */
    struct rgbColor *colors;    /* Color palette for tissues */
    boolean doLogTransform;     /* Log10(x+1) */
    struct gtexTissue *tissues; /* Cache tissue names, descriptions */
    struct hash *tissueFilter;  /* For filter. NULL out excluded tissues */
    };

struct gtexGeneInfo
/* GTEx gene model, names, and expression medians */
    {
    struct gtexGeneInfo *next;  /* Next in singly linked list */
    struct gtexGeneBed *geneBed;/* Gene name, id, canonical transcript, exp count and medians 
                                        from BED table */
    struct genePred *geneModel; /* Gene structure from model table */
    char *description;          /* Gene description */
    double *medians1;            /* Computed medians */
    double *medians2;            /* Computed medians for comparison (inverse) graph */
    int height;                  /* Item height in pixels */
    };


#define MAX_DESC  200
/***********************************************/
/* Color gene models using GENCODE conventions */

static struct rgbColor codingColor = {12, 12, 120}; // #0C0C78
static struct rgbColor nonCodingColor = {0, 100, 0}; // #006400
static struct rgbColor pseudoColor = {255,51,255}; // #FF33FF
static struct rgbColor problemColor = {254, 0, 0}; // #FE0000
static struct rgbColor unknownColor = {1, 1, 1};

static struct statusColors
/* Color values for gene models */
    {
    Color coding;
    Color nonCoding;
    Color pseudo;
    Color problem;
    Color unknown;
    } statusColors = {0,0,0,0};


static void initGeneColors(struct hvGfx *hvg)
/* Get and cache indexes for color values */
{
if (statusColors.coding != 0)
    return;
statusColors.coding = hvGfxFindColorIx(hvg, codingColor.r, codingColor.g, codingColor.b);
statusColors.nonCoding = hvGfxFindColorIx(hvg, nonCodingColor.r, nonCodingColor.g, nonCodingColor.b);
statusColors.pseudo = hvGfxFindColorIx(hvg, pseudoColor.r, pseudoColor.g, pseudoColor.b);
statusColors.problem = hvGfxFindColorIx(hvg, problemColor.r, problemColor.g, problemColor.b);
statusColors.unknown = hvGfxFindColorIx(hvg, unknownColor.r, unknownColor.g, unknownColor.b);
}

static Color getGeneClassColor(struct hvGfx *hvg, struct gtexGeneBed *geneBed)
/* Find GENCODE color for gene type.
 * NOTE: this is based on defined gene biotypes from GENCODE V19, mapped to the classes
 * defined for transcripts, as follows:

 * coding: IG_C_gene, IG_D_gene, IG_J_gene, IG_V_gene, 
               TR_C_gene, TR_D_gene, TR_J_gene, TR_V_gene 
               polymorphic_pseudogene, protein_coding

 * pseudo: IG_C_pseudogene, IG_J_pseudogene, IG_V_pseudogene, TR_J_pseudogene, TR_V_pseudogene,
               pseudogene 

 * nonCoding: 3prime_overlapping_ncrna, Mt_rRNA, Mt_tRNA, antisense, lincRNA, miRNA, 
                misc_RNA, processed_transcript, rRNA, sense_intronic, sense_overlapping, 
                snRNA, snoRNA
*/

{
initGeneColors(hvg);
char *geneType = geneBed->transcriptClass;

/* keep backwards compatibility with tables (V4 having transcript classes in table) */
if (geneType == NULL)
    return statusColors.unknown;

if (sameString(geneType, "coding") || sameString(geneType, "protein_coding") || 
        sameString(geneType, "polymorphic_pseudogene") || endsWith(geneType, "_gene"))
    return statusColors.coding;

if (sameString(geneType, "pseudo") || sameString(geneType, "pseudogene") || 
        endsWith(geneType, "_pseudogene"))
    return statusColors.nonCoding;

// A bit of a cheat here -- better a mapping table
return statusColors.nonCoding;
}

/***********************************************/
/* Cache tissue info */

struct gtexTissue *getTissues()
/* Get and cache tissue metadata from database */
{
static struct gtexTissue *gtexTissues = NULL;

if (!gtexTissues)
    gtexTissues = gtexGetTissues();
return gtexTissues;
}

int getTissueCount()
/* Get and cache the number of tissues in GTEx tissue table */
{
static int tissueCount = 0;

if (!tissueCount)
    tissueCount = slCount(getTissues());
return tissueCount;
}

char *getTissueName(int id)
/* Get tissue name from id, cacheing */
{
static char **tissueNames = NULL;

struct gtexTissue *tissue;
int count = getTissueCount();
if (!tissueNames)
    {
    struct gtexTissue *tissues = getTissues();
    AllocArray(tissueNames, count);
    for (tissue = tissues; tissue != NULL; tissue = tissue->next)
        tissueNames[tissue->id] = cloneString(tissue->name);
    }
if (id >= count)
    errAbort("GTEx tissue table problem: can't find id %d\n", id);
return tissueNames[id];
}

struct rgbColor *getGtexTissueColors()
/* Get RGB colors from tissue table */
{
struct gtexTissue *tissues = getTissues();
struct gtexTissue *tissue = NULL;
int count = slCount(tissues);
struct rgbColor *colors;
AllocArray(colors, count);
int i = 0;
for (tissue = tissues; tissue != NULL; tissue = tissue->next)
    {
    // TODO: reconcile 
    colors[i] = (struct rgbColor){.r=COLOR_32_BLUE(tissue->color), .g=COLOR_32_GREEN(tissue->color), .b=COLOR_32_RED(tissue->color)};
    //colors[i] = mgColorIxToRgb(NULL, tissue->color);
    i++;
    }
return colors;
}

/*****************************************************************/
/* Load sample data, gene info, and anything else needed to draw */

static struct hash *loadGeneModels(char *table)
/* Load gene models from table */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;
sr = hRangeQuery(conn, table, chromName, winStart, winEnd, NULL, &rowOffset);

struct hash *modelHash = newHash(0);
struct genePred *model = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    model = genePredLoad(row+rowOffset);
    hashAdd(modelHash, model->name, model);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return modelHash;
}

static void loadComputedMedians(struct gtexGeneInfo *geneInfo, struct gtexGeneExtras *extras)
/* Compute medians based on graph type.  Returns a list of 2 for comparison graph types */
{
struct gtexGeneBed *geneBed = geneInfo->geneBed;
int expCount = geneBed->expCount;
if (extras->isComparison)
    {
    // create two score hashes, one for each sample subset
    struct hash *scoreHash1 = hashNew(0), *scoreHash2 = hashNew(0);
    struct sqlConnection *conn = hAllocConn("hgFixed");
    char query[1024];
    char **row;
    sqlSafef(query, sizeof(query), "select gtexSampleData.sample, gtexDonor.gender, gtexSampleData.tissue, gtexSampleData.score from gtexSampleData, gtexSample, gtexDonor where gtexSampleData.geneId='%s' and gtexSampleData.sample=gtexSample.sampleId and gtexSample.donor=gtexDonor.name", geneBed->geneId);
    struct sqlResult *sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        char gender = *row[1];
        // TODO: generalize for other comparison graphs (this code just for M/F comparison)
        struct hash *scoreHash = ((gender == 'F') ? scoreHash1 : scoreHash2);
        char *tissue = cloneString(row[2]);
        struct slDouble *score = slDoubleNew(sqlDouble(row[3]));

        // create hash of lists of scores, keyed by tissue name
        double *tissueScores = hashFindVal(scoreHash, tissue);
        if (tissueScores)
            slAddHead(tissueScores, score);
        else
            hashAdd(scoreHash, tissue, score);
        }
    sqlFreeResult(&sr);
    hFreeConn(&conn);

    // get tissue medians for each sample subset
    double *medians1;
    double *medians2;
    AllocArray(medians1, expCount);
    AllocArray(medians2, expCount);
    int i;
    for (i=0; i<geneBed->expCount; i++)
        {
        //medians1[i] = -1, medians2[i] = -1;       // mark missing tissues ?
        struct slDouble *scores;
        scores = hashFindVal(scoreHash1, getTissueName(i));
        if (scores)
            medians1[i] = slDoubleMedian(scores);
        scores = hashFindVal(scoreHash2, getTissueName(i));
        if (scores)
            medians2[i] = slDoubleMedian(scores);
        }
    if (extras->isDifference)
        {
        for (i=0; i<geneBed->expCount; i++)
            {
            if (medians1[i] >= medians2[i])
                {
                medians1[i] -= medians2[i];
                medians2[i] = 0;
                }
            else
                {
                medians2[i] -= medians1[i];
                medians1[i] = 0;
                }
            }
        }
    geneInfo->medians1 = medians1;
    geneInfo->medians2 = medians2;
    }
}

static int gtexGeneItemHeight(struct track *tg, void *item);

static void filterTissues(struct track *tg)
/* Check cart for tissue selection.  NULL out unselected tissues in tissue list */
{
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
struct gtexTissue *tis = NULL;
extras->tissues = getTissues();
extras->tissueFilter = hashNew(0);
if (cartListVarExistsAnyLevel(cart, tg->tdb, FALSE, GTEX_TISSUE_SELECT))
    {
    struct slName *selectedValues = cartOptionalSlNameListClosestToHome(cart, tg->tdb, 
                                                        FALSE, GTEX_TISSUE_SELECT);
    if (selectedValues != NULL)
        {
        struct slName *name;
        for (name = selectedValues; name != NULL; name = name->next)
            hashAdd(extras->tissueFilter, name->name, name->name);
        return;
        }
    }
/* no filter */
for (tis = extras->tissues; tis != NULL; tis = tis->next)
    hashAdd(extras->tissueFilter, tis->name, tis->name);
}

static int filteredTissueCount(struct track *tg)
/* Count of tissues to display */
{
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
return hashNumEntries(extras->tissueFilter);
}

static boolean filterTissue(struct track *tg, char *name)
/* Does tissue pass filter */
{
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
return (hashLookup(extras->tissueFilter, name) != NULL);
}

static void gtexGeneLoadItems(struct track *tg)
/* Load method for track items */
{
/* Get track UI info */
struct gtexGeneExtras *extras;
AllocVar(extras);
tg->extraUiData = extras;

/* Get version info from track table name */
extras->version = gtexVersionSuffix(tg->table);
extras->doLogTransform = cartUsualBooleanClosestToHome(cart, tg->tdb, FALSE, GTEX_LOG_TRANSFORM, 
                                                GTEX_LOG_TRANSFORM_DEFAULT);
char *samples = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, 
                                                GTEX_SAMPLES, GTEX_SAMPLES_DEFAULT);
extras->graphType = cloneString(samples);
if (sameString(samples, GTEX_SAMPLES_COMPARE_SEX))
    extras->isComparison = TRUE;
char *comparison = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, GTEX_COMPARISON_DISPLAY,
                        GTEX_COMPARISON_DEFAULT);
extras->isDifference = sameString(comparison, GTEX_COMPARISON_DIFF) ? TRUE : FALSE;
extras->maxMedian = gtexMaxMedianScore(extras->version);

/* Get geneModels in range */
char buf[256];
char *modelTable = "gtexGeneModel";
safef(buf, sizeof(buf), "%s%s", modelTable, extras->version ? extras->version: "");
struct hash *modelHash = loadGeneModels(buf);

/* Get geneBeds (names and all-sample tissue median scores) in range */
bedLoadItem(tg, tg->table, (ItemLoader)gtexGeneBedLoad);

/* Create geneInfo items with BED and geneModels */
struct gtexGeneInfo *geneInfo = NULL, *list = NULL;
struct gtexGeneBed *geneBed = (struct gtexGeneBed *)tg->items;

/* Load tissue colors: GTEx or rainbow */
#ifdef COLOR_SCHEME
char *colorScheme = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, GTEX_COLORS, 
                        GTEX_COLORS_DEFAULT);
#else
char *colorScheme = GTEX_COLORS_DEFAULT;
#endif
if (sameString(colorScheme, GTEX_COLORS_GTEX))
    {
    extras->colors = getGtexTissueColors();
    }
else
    {
    if (geneBed)
	{
	int expCount = geneBed->expCount;
	extras->colors = getRainbow(&saturatedRainbowAtPos, expCount);
	}
    }
filterTissues(tg);

while (geneBed != NULL)
    {
    AllocVar(geneInfo);
    geneInfo->geneBed = geneBed;
    geneInfo->geneModel = hashFindVal(modelHash, geneBed->geneId); // sometimes this is missing, hash returns NULL. do we check?
    // NOTE: Consider loading all gene descriptions to save queries
    char query[256];
    sqlSafef(query, sizeof(query),
            "select kgXref.description from kgXref where geneSymbol='%s'", geneBed->name);
    struct sqlConnection *conn = hAllocConn(database);
    char *desc = sqlQuickString(conn, query);
    hFreeConn(&conn);
    if (desc)
        {
        // hg38 known genes has extra detail about source; strip it
        char *fromDetail = strstrNoCase(desc, "(from");
        if (fromDetail)
            *fromDetail = 0;
        if (strlen(desc) > MAX_DESC)
            strcpy(desc+MAX_DESC, "...");
        geneInfo->description = desc;
        }
    slAddHead(&list, geneInfo);
    geneBed = geneBed->next;
    geneInfo->geneBed->next = NULL;
    if (extras->isComparison && (tg->visibility == tvFull || tg->visibility == tvPack))
        // compute medians based on configuration (comparisons, and later, filters)
        loadComputedMedians(geneInfo, extras);
    geneInfo->height = gtexGeneItemHeight(tg, geneInfo);
    }
slReverse(&list);
tg->items = list;
}

/***********************************************/
/* Draw */

/* Bargraph layouts for three window sizes */
#define WIN_MAX_GRAPH 20000
#define MAX_GRAPH_HEIGHT 100
#define MAX_BAR_WIDTH 5
#define MAX_GRAPH_PADDING 2

#define WIN_MED_GRAPH 500000
#define MED_GRAPH_HEIGHT 60
#define MED_BAR_WIDTH 3
#define MED_GRAPH_PADDING 1

#define MIN_GRAPH_HEIGHT 20
#define MIN_BAR_WIDTH 1
#define MIN_GRAPH_PADDING 0

#define MARGIN_WIDTH 1

static int gtexBarWidth()
{
long winSize = virtWinBaseCount;
if (winSize < WIN_MAX_GRAPH)
    return MAX_BAR_WIDTH;
else if (winSize < WIN_MED_GRAPH)
    return MED_BAR_WIDTH;
else
    return MIN_BAR_WIDTH;
}

static enum trackVisibility gtexGeneModelVis(struct gtexGeneExtras *extras)
{
long winSize = virtWinBaseCount;
if (winSize < WIN_MED_GRAPH && !extras->isComparison)
    return tvPack;
return tvSquish;
}

static int gtexGeneModelHeight(struct gtexGeneExtras *extras)
{
long winSize = virtWinBaseCount;
if (winSize < WIN_MED_GRAPH && !extras->isComparison)
    return tl.fontHeight;
// too busy to show exon arrows if zoomed out or have double graphs
return 8;
}

static int gtexGraphPadding()
{
long winSize = virtWinBaseCount;

if (winSize < WIN_MAX_GRAPH)
    return MAX_GRAPH_PADDING;
else if (winSize < WIN_MED_GRAPH)
    return MED_GRAPH_PADDING;
else
    return MIN_GRAPH_PADDING;
}

static int gtexMaxGraphHeight()
{
long winSize = virtWinBaseCount;
if (winSize < WIN_MAX_GRAPH)
    return MAX_GRAPH_HEIGHT;
else if (winSize < WIN_MED_GRAPH)
    return MED_GRAPH_HEIGHT;
else
    return MIN_GRAPH_HEIGHT;
}

static int gtexGraphWidth(struct track *tg, struct gtexGeneInfo *geneInfo)
/* Width of GTEx graph in pixels */
{
int barWidth = gtexBarWidth();
int padding = gtexGraphPadding();
int count = filteredTissueCount(tg);
int labelWidth = geneInfo->medians2 ? tl.mWidth : 0;
return (barWidth * count) + (padding * (count-1)) + labelWidth + 2;
}

static int gtexGraphX(struct gtexGeneBed *gtex)
/* Locate graph on X, relative to viewport. */
{
int start = max(gtex->chromStart, winStart);
double scale = scaleForWindow(insideWidth, winStart, winEnd);
int x1 = round((start - winStart) * scale);
return x1;
}

static int gtexGeneMargin()
{
    return 1;
}

static int valToHeight(double val, double maxVal, int maxHeight, boolean doLogTransform)
/* Log-scale and convert a value from 0 to maxVal to 0 to maxHeight-1 */
{
if (val == 0.0)
    return 0;
double scaled = 0.0;
if (doLogTransform)
    scaled = log10(val+1.0) / log10(maxVal+1.0);
else
    scaled = val/maxVal;
if (scaled < 0)
    warn("scaled=%f\n", scaled);
return (scaled * (maxHeight-1));
}

static int valToClippedHeight(double val, double maxVal, int maxView, int maxHeight, 
                                        boolean doLogTransform)
/* Convert a value from 0 to maxVal to 0 to maxHeight-1, with clipping, or log transform the value */
{
double useVal = val;
double useMax = maxVal;
if (!doLogTransform)
    {
    useMax = maxView;
    if (val > maxView)
        useVal = maxView;
    }
return valToHeight(useVal, useMax, gtexMaxGraphHeight(), doLogTransform);
}

static void drawGraphBase(struct track *tg, struct gtexGeneInfo *geneInfo, struct hvGfx *hvg, int x, int y)
/* Draw faint line under graph to delineate extent when bars are missing (tissue w/ 0 expression) */
{
Color lightGray = MAKECOLOR_32(0xD1, 0xD1, 0xD1);
int graphWidth = gtexGraphWidth(tg, geneInfo);
hvGfxBox(hvg, x, y, graphWidth, 1, lightGray);
}

static int gtexGeneGraphHeight(struct track *tg, struct gtexGeneInfo *geneInfo, boolean doTop)
/* Determine height in pixels of graph.  This will be the box for tissue with highest expression
   If doTop is false, compute height of bottom graph of comparison */
{
struct gtexGeneBed *geneBed = geneInfo->geneBed;
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
int i;
double maxExp = 0.0;
int expCount = geneBed->expCount;
double expScore;
for (i=0; i<expCount; i++)
    {
    if (!filterTissue(tg, getTissueName(i)))
        continue;
    if (doTop)
        expScore = (geneInfo->medians1 ? geneInfo->medians1[i] : geneBed->expScores[i]);
    else
        expScore = geneInfo->medians2[i];
    maxExp = max(maxExp, expScore);
    }
double viewMax = (double)cartUsualIntClosestToHome(cart, tg->tdb, FALSE, 
                                GTEX_MAX_LIMIT, GTEX_MAX_LIMIT_DEFAULT);
double maxMedian = ((struct gtexGeneExtras *)tg->extraUiData)->maxMedian;
return valToClippedHeight(maxExp, maxMedian, viewMax, gtexMaxGraphHeight(), extras->doLogTransform);
}

static void gtexGeneDrawAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y, 
                double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw tissue expression bar graph over gene model. 
   Optionally, draw a second graph under gene, to compare sample sets */
{
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
struct gtexGeneInfo *geneInfo = (struct gtexGeneInfo *)item;
struct gtexGeneBed *geneBed = geneInfo->geneBed;
// Color in dense mode using transcriptClass
Color statusColor = getGeneClassColor(hvg, geneBed);
if (vis != tvFull && vis != tvPack)
    {
    bedDrawSimpleAt(tg, geneBed, hvg, xOff, y, scale, font, statusColor, vis);
    return;
    }

int heightPer = tg->heightPer;
int graphX = gtexGraphX(geneBed);
if (graphX < 0)
    return;

// draw gene model
int topGraphHeight = gtexGeneGraphHeight(tg, geneInfo, TRUE);
topGraphHeight = max(topGraphHeight, tl.fontHeight);
int yZero = topGraphHeight + y - 1;  // yZero is bottom of graph
int yGene = yZero + gtexGeneMargin() - 1;
tg->heightPer = gtexGeneModelHeight(extras) + 1;
if (geneInfo->geneModel) // some BEDs do not have a corresponding geneModel record
    {
    struct linkedFeatures *lf = linkedFeaturesFromGenePred(tg, geneInfo->geneModel, FALSE);
    lf->filterColor = statusColor;
    linkedFeaturesDrawAt(tg, lf, hvg, xOff, yGene, scale, font, color, 
                                gtexGeneModelVis(extras));
    }
tg->heightPer = heightPer;
}

static int gtexGeneNonPropPixelWidth(struct track *tg, void *item)
/* Return end chromosome coordinate of item, including graph */
{
struct gtexGeneInfo *geneInfo = (struct gtexGeneInfo *)item;
int graphWidth = gtexGraphWidth(tg, geneInfo);
return graphWidth;
}

static void gtexGeneNonPropDrawAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y,
                double scale, MgFont *font, Color color, enum trackVisibility vis)
{
if (vis != tvFull && vis != tvPack)
    return;
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
struct gtexGeneInfo *geneInfo = (struct gtexGeneInfo *)item;
struct gtexGeneBed *geneBed = geneInfo->geneBed;
int topGraphHeight = gtexGeneGraphHeight(tg, geneInfo, TRUE);
topGraphHeight = max(topGraphHeight, tl.fontHeight);
int yZero = topGraphHeight + y - 1;  // yZero is bottom of graph
int yGene = yZero + gtexGeneMargin()-1;

int graphX = gtexGraphX(geneBed);
int x1 = xOff + graphX;         // x1 is at left of graph
int keepX = x1;
drawGraphBase(tg, geneInfo, hvg, keepX, yZero+1);

int startX = x1;
struct rgbColor lineColor = {.r=0};
int lineColorIx = hvGfxFindColorIx(hvg, lineColor.r, lineColor.g, lineColor.b);
int barWidth = gtexBarWidth();
int graphPadding = gtexGraphPadding();
char *colorScheme = cartUsualStringClosestToHome(cart, tg->tdb, FALSE, GTEX_COLORS, 
                        GTEX_COLORS_DEFAULT);
Color labelColor = MG_GRAY;
Color clipColor = MG_MAGENTA;

// add labels to comparison graphs
if (geneInfo->medians2)
    {
    hvGfxText(hvg, x1, yZero - tl.fontHeight + 2, labelColor, font, "F");
    hvGfxText(hvg, x1, yZero + gtexGeneModelHeight(extras) + gtexGeneMargin() + 1, 
                labelColor, font, "M");
    startX = startX + tl.mWidth+2;
    x1 = startX;
    }

// draw bar graph
double viewMax = (double)cartUsualIntClosestToHome(cart, tg->tdb, FALSE, 
                                GTEX_MAX_LIMIT, GTEX_MAX_LIMIT_DEFAULT);
double maxMedian = ((struct gtexGeneExtras *)tg->extraUiData)->maxMedian;
int i;
int expCount = geneBed->expCount;
struct gtexTissue *tis;
for (i=0, tis=extras->tissues; i<expCount; i++, tis=tis->next)
    {
    if (!filterTissue(tg, tis->name))
        continue;
    struct rgbColor fillColor = extras->colors[i];
    if (barWidth == 1 && sameString(colorScheme, GTEX_COLORS_GTEX))
        {
        // brighten colors a bit so they'll be more visible at this scale
        fillColor = gtexTissueBrightenColor(fillColor);
        }
    int fillColorIx = hvGfxFindColorIx(hvg, fillColor.r, fillColor.g, fillColor.b);
    double expScore = (geneInfo->medians1 ? geneInfo->medians1[i] : geneBed->expScores[i]);
    int height = valToClippedHeight(expScore, maxMedian, viewMax, 
                                        gtexMaxGraphHeight(), extras->doLogTransform);
    if (graphPadding == 0 || sameString(colorScheme, GTEX_COLORS_GTEX))
        hvGfxBox(hvg, x1, yZero-height+1, barWidth, height, fillColorIx);
    else
        hvGfxOutlinedBox(hvg, x1, yZero-height+1, barWidth, height, fillColorIx, lineColorIx);
    // mark clipped bar with magenta tip
    if (!extras->doLogTransform && expScore > viewMax)
        hvGfxBox(hvg, x1, yZero-height+1, barWidth, 1, clipColor);
    x1 = x1 + barWidth + graphPadding;
    }

if (!geneInfo->medians2)
    return;

// draw comparison bar graph (upside down)
x1 = startX;
yZero = yGene + gtexGeneModelHeight(extras) + 1; // yZero is at top of graph
drawGraphBase(tg, geneInfo, hvg, keepX, yZero-1);

for (i=0, tis=extras->tissues; i<expCount; i++, tis=tis->next)
    {
    if (!filterTissue(tg, tis->name))
        continue;
    struct rgbColor fillColor = extras->colors[i];
    if (barWidth == 1 && sameString(colorScheme, GTEX_COLORS_GTEX))
        {
        // brighten colors a bit so they'll be more visible at this scale
        struct hslColor hsl = mgRgbToHsl(fillColor);
        hsl.s = min(1000, hsl.s + 300);
        fillColor = mgHslToRgb(hsl);
        }
    int fillColorIx = hvGfxFindColorIx(hvg, fillColor.r, fillColor.g, fillColor.b);
    double expScore = geneInfo->medians2[i];
    int height = valToClippedHeight(expScore, maxMedian, viewMax, gtexMaxGraphHeight(), 
                                        extras->doLogTransform);
    if (graphPadding == 0 || sameString(colorScheme, GTEX_COLORS_GTEX))
        hvGfxBox(hvg, x1, yZero, barWidth, height, fillColorIx);
    else
        hvGfxOutlinedBox(hvg, x1, yZero, barWidth, height, fillColorIx, lineColorIx);
    // mark clipped bar with magenta tip
    if (!extras->doLogTransform && expScore > viewMax)
        hvGfxBox(hvg, x1, yZero + height, barWidth, 1, clipColor);
    x1 = x1 + barWidth + graphPadding;
    }
}

static int gtexGeneItemHeightOptionalMax(struct track *tg, void *item, boolean isMax)
{
if (tg->visibility == tvSquish || tg->visibility == tvDense)
    return 0;
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
if (isMax)
    {
    int extra = 0;
    if (((struct gtexGeneExtras *)tg->extraUiData)->isComparison)
        extra = gtexMaxGraphHeight() + 2;
    return gtexMaxGraphHeight() + gtexGeneMargin() + gtexGeneModelHeight(extras) + extra;
    }
if (item == NULL)
    return 0;
struct gtexGeneInfo *geneInfo = (struct gtexGeneInfo *)item;

if (geneInfo->height != 0)
    return geneInfo->height;
int topGraphHeight = gtexGeneGraphHeight(tg, geneInfo, TRUE);
topGraphHeight = max(topGraphHeight, tl.fontHeight);
int bottomGraphHeight = 0;
boolean isComparison = ((struct gtexGeneExtras *)tg->extraUiData)->isComparison;
if (isComparison)
    {
    bottomGraphHeight = max(gtexGeneGraphHeight(tg, geneInfo, FALSE),
                                tl.fontHeight) + gtexGeneMargin();
    }
int height = topGraphHeight + bottomGraphHeight + gtexGeneMargin() + 
                gtexGeneModelHeight(extras);
return height;
}

static int gtexGeneMaxHeight(struct track *tg)
/* Maximum height in pixels of a gene graph */
{
return gtexGeneItemHeightOptionalMax(tg, NULL, TRUE);
}

static int gtexGeneItemHeight(struct track *tg, void *item)
{
return gtexGeneItemHeightOptionalMax(tg, item, FALSE);
}

static char *tissueExpressionText(struct gtexTissue *tissue, double expScore, 
                                        boolean doLogTransform, char *qualifier)
/* Construct mouseover text for tissue graph */
{
static char buf[128];
safef(buf, sizeof(buf), "%s (%.1f %s%s%sRPKM)", tissue->description, 
                                doLogTransform ? log10(expScore+1.0) : expScore,
                                qualifier != NULL ? qualifier : "",
                                qualifier != NULL ? " " : "",
                                doLogTransform ? "log " : "");
return buf;
}

static void gtexGeneMapItem(struct track *tg, struct hvGfx *hvg, void *item, char *itemName, 
                        char *mapItemName, int start, int end, int x, int y, int width, int height)
/* Create a map box on gene model and label, and one for each tissue (bar in the graph) in
 * pack or full mode.  Just single map for squish/dense modes */
{
if (tg->visibility == tvDense || tg->visibility == tvSquish)
    {
    genericMapItem(tg, hvg, item, itemName, itemName, start, end, x, y, width, height);
    return;
    }
struct gtexGeneInfo *geneInfo = item;
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
struct gtexGeneBed *geneBed = geneInfo->geneBed;
int topGraphHeight = gtexGeneGraphHeight(tg, geneInfo, TRUE);
topGraphHeight = max(topGraphHeight, tl.fontHeight);        // label
int yZero = topGraphHeight + y - 1;  // yZero is bottom of graph
//int yGene = yZero + gtexGeneMargin() - 1;
int x1 = insideX;


// add maps to tissue bars in expresion graph
struct gtexTissue *tissues = getTissues();
struct gtexTissue *tissue = NULL;
int barWidth = gtexBarWidth();
int padding = gtexGraphPadding();
double maxMedian = ((struct gtexGeneExtras *)tg->extraUiData)->maxMedian;

int graphX = gtexGraphX(geneBed);
if (graphX < 0)
    return;

// x1 is at left of graph
x1 = insideX + graphX;

if (geneInfo->medians2)
    {
    // skip over labels in comparison graphs
    x1 = x1 + tl.mWidth+ 2;
    }
int i = 0;

double viewMax = (double)cartUsualIntClosestToHome(cart, tg->tdb, FALSE, 
                                GTEX_MAX_LIMIT, GTEX_MAX_LIMIT_DEFAULT);
for (tissue = tissues; tissue != NULL; tissue = tissue->next, i++)
    {
    if (!filterTissue(tg, tissue->name))
        continue;
    double expScore =  (geneInfo->medians1 ? geneInfo->medians1[i] : geneBed->expScores[i]);
    int height = valToClippedHeight(expScore, maxMedian, viewMax, 
                                        gtexMaxGraphHeight(), extras->doLogTransform);
    char *qualifier = NULL;
    if (extras->isComparison && extras->isDifference)
        qualifier = "F-M";
    mapBoxHc(hvg, start, end, x1, yZero-height, barWidth, height, tg->track, mapItemName,  
                tissueExpressionText(tissue, expScore, extras->doLogTransform, qualifier));
    // add map box to comparison graph
    if (geneInfo->medians2)
        {
        double expScore = geneInfo->medians2[i];
        int height = valToClippedHeight(expScore, maxMedian, viewMax, 
                                        gtexMaxGraphHeight(), extras->doLogTransform);
        int y = yZero + gtexGeneModelHeight(extras) + gtexGeneMargin();  // y is top of bottom graph
        if (extras->isComparison && extras->isDifference)
            qualifier = "M-F";
        mapBoxHc(hvg, start, end, x1, y, barWidth, height, tg->track, mapItemName,
                tissueExpressionText(tissue, expScore, extras->doLogTransform, qualifier));
        }
    x1 = x1 + barWidth + padding;
    }

// add map box with description to gene model
// NOTE: this is "under" the tissue map boxes
if (geneInfo->geneModel && geneInfo->description)
    {
    double scale = scaleForWindow(insideWidth, winStart, winEnd);
    int geneStart = max(geneInfo->geneModel->txStart, winStart);
    int geneEnd = min(geneInfo->geneModel->txEnd, winEnd);
    int geneX = round((geneStart-winStart)*scale);
    int w = round((geneEnd-winStart)*scale) - geneX;
    x1 = insideX + geneX;
    int labelWidth = mgFontStringWidth(tl.font, itemName);
    if (x1-labelWidth <= insideX)
        labelWidth = 0;
    mapBoxHc(hvg, start, end, x1-labelWidth, y, w+labelWidth, geneInfo->height, tg->track, 
                    mapItemName, geneInfo->description);
    }
}

static char *gtexGeneItemName(struct track *tg, void *item)
/* Return gene name */
{
struct gtexGeneInfo *geneInfo = (struct gtexGeneInfo *)item;
struct gtexGeneBed *geneBed = geneInfo->geneBed;
return geneBed->name;
}

static int gtexGeneHeight(void *item)
{
struct gtexGeneInfo *geneInfo = (struct gtexGeneInfo *)item;
assert(geneInfo->height != 0);
return geneInfo->height;
}

static int gtexGeneTotalHeight(struct track *tg, enum trackVisibility vis)
/* Figure out total height of track. Set in track and also return it */
{
int height = 0;

if (tg->visibility == tvSquish || tg->visibility == tvDense)
    {
    height = tgFixedTotalHeightOptionalOverflow(tg, vis, tl.fontHeight+1, tl.fontHeight, FALSE);
    }
else if ((tg->visibility == tvPack) || (tg->visibility == tvFull))
    {
    if (!tg->ss)
        {
        // layout -- initially as fixed height
        int height = gtexGeneMaxHeight(tg);
        tgFixedTotalHeightOptionalOverflow(tg, vis, height, height, FALSE); // TODO: allow oflow ?
        }
    // set variable height rows
    if (tg->ss->rowCount != 0)
        {
        if (!tg->ss->rowSizes)
	    {
	    // collect the rowSizes data across all windows
	    assert(currentWindow==windows); // first window
	    assert(tg->ss->vis == vis); // viz matches, we have the right one
	    struct spaceSaver *ssHold; 
	    AllocVar(ssHold);
	    struct track *tgSave = tg;
	    for(tg=tgSave; tg; tg=tg->nextWindow)
		{
		assert(tgSave->ss->vis == tg->ss->vis); // viz matches, we have the right one
		spaceSaverSetRowHeights(tg->ss, ssHold, gtexGeneHeight);
		}
	    // share the rowSizes data across all windows
	    for(tg=tgSave; tg; tg=tg->nextWindow)
		{
		tg->ss->rowSizes = ssHold->rowSizes;
		}
	    tg = tgSave;
	    }
	height = spaceSaverGetRowHeightsTotal(tg->ss);
        }
    }
tg->height = height;

return height;
}

static int gtexGeneItemStart(struct track *tg, void *item)
/* Return end chromosome coordinate of item, including graph */
{
struct gtexGeneInfo *geneInfo = (struct gtexGeneInfo *)item;
struct gtexGeneBed *geneBed = geneInfo->geneBed;
return geneBed->chromStart;
}

static int gtexGeneItemEnd(struct track *tg, void *item)
/* Return end chromosome coordinate of item, including graph */
{
struct gtexGeneInfo *geneInfo = (struct gtexGeneInfo *)item;
struct gtexGeneBed *geneBed = geneInfo->geneBed;
double scale = scaleForWindow(insideWidth, winStart, winEnd);
int graphWidth = gtexGraphWidth(tg, geneInfo);
return max(geneBed->chromEnd, max(winStart, geneBed->chromStart) + graphWidth/scale);
}

void gtexGeneMethods(struct track *tg)
{
tg->drawItemAt = gtexGeneDrawAt;
tg->loadItems = gtexGeneLoadItems;
tg->mapItem = gtexGeneMapItem;
tg->itemName = gtexGeneItemName;
tg->mapItemName = gtexGeneItemName;
tg->itemHeight = gtexGeneItemHeight;
tg->itemStart = gtexGeneItemStart;
tg->itemEnd = gtexGeneItemEnd;
tg->totalHeight = gtexGeneTotalHeight;
tg->nonPropDrawItemAt = gtexGeneNonPropDrawAt;
tg->nonPropPixelWidth = gtexGeneNonPropPixelWidth;
}



