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

enum geneLabelStyle
    {
    LABEL_GENE_SYMBOL = 0,
    LABEL_GENE_ACCESSION = 1,
    LABEL_BOTH = 2
    };

static enum geneLabelStyle getLabelStyle(char *cartVar)
/* Get enum corresponding to cart var */
{
    if (sameString(GTEX_LABEL_SYMBOL, cartVar))
        return LABEL_GENE_SYMBOL;
    if (sameString(GTEX_LABEL_ACCESSION, cartVar))
        return LABEL_GENE_ACCESSION;
    if (sameString(GTEX_LABEL_BOTH, cartVar))
        return LABEL_BOTH;
    return LABEL_GENE_SYMBOL;
}

struct gtexGeneExtras 
/* Track info */
    {
    char *version;              /* Suffix to table name, e.g. 'V6' */
    boolean codingOnly;         /* User filter to limit display to coding genes */
    boolean showExons;          /* Show gene model exons */
    boolean noWhiteout;         /* Suppress whiteout of graph background (allow highlight, blue lines) */
    enum geneLabelStyle labelStyle;  /* Show gene symbol, accession, or both */ 
    double maxMedian;           /* Maximum median rpkm for all tissues */
    boolean isComparison;       /* Comparison of two sample sets (e.g. male/female). */
    boolean isDifference;       /* True if comparison is shown as a single difference graph. 
                                   False if displayed as two graphs, one oriented downward */
    char *graphType;            /* Additional info about graph (e.g. type of comparison graph */
    struct rgbColor *colors;    /* Color palette for tissues */
    boolean doLogTransform;     /* Log10(x+1) */
    struct gtexTissue *tissues; /* Tissue names, descriptions */
    int tissueCount;            /* Tissue count - derived from above */
    char **tissueNames;         /* Tissue names by id - derived from above */
    char **tissueLabels;        /* Tissue labels by id - derived from above */
    char **tissueDescriptions;  /* Tissue descriptions by id - derived from above */
    struct hash *tissueFilter;  /* For filter. NULL out excluded tissues */
    };

struct gtexGeneInfo
/* GTEx gene model, names, and expression medians */
    {
    struct gtexGeneInfo *next;  /* Next in singly linked list */
    struct gtexGeneBed *geneBed;/* Gene name, id, type, exp count and medians 
                                        from BED table */
    struct genePred *geneModel; /* Gene structure from model table */
    char *label;                /* Name, accession, or both */
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
/* Find GENCODE color for gene type. */
{
initGeneColors(hvg);
char *geneClass = gtexGeneClass(geneBed);
if (geneClass == NULL)
    return statusColors.unknown;
if (sameString(geneClass, "coding"))
    return statusColors.coding;
if (sameString(geneClass, "nonCoding"))
    return statusColors.nonCoding;
if (sameString(geneClass, "pseudo"))
    return statusColors.pseudo;
return statusColors.unknown;
}

/***********************************************/
/* Cache tissue info */

struct gtexTissue *getTissues(struct track *tg, char *version)
/* Get and cache tissue metadata from database */
{
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
if (!extras->tissues)
    extras->tissues = gtexGetTissues(version);
return extras->tissues;
}

int getTissueCount(struct track *tg, char *version)
/* Get and cache the number of tissues in GTEx tissue table */
{
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
if (!extras->tissueCount)
    extras->tissueCount = slCount(getTissues(tg, version));
return extras->tissueCount;
}

char *getTissueName(struct track *tg, int id, char *version)
/* Get tissue name from id, cacheing */
{
struct gtexTissue *tissue;
int count = getTissueCount(tg, version);
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
if (!extras->tissueNames)
    {
    struct gtexTissue *tissues = getTissues(tg, version);
    AllocArray(extras->tissueNames, count);
    for (tissue = tissues; tissue != NULL; tissue = tissue->next)
        extras->tissueNames[tissue->id] = cloneString(tissue->name);
    }
if (id >= count)
    errAbort("GTEx tissue table problem: can't find id %d\n", id);
return extras->tissueNames[id];
}

char *getTissueDescription(struct track *tg, int id, char *version)
/* Get tissue description from id, cacheing */
{
struct gtexTissue *tissue;
int count = getTissueCount(tg, version);
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
if (!extras->tissueDescriptions)
    {
    struct gtexTissue *tissues = getTissues(tg, version);
    AllocArray(extras->tissueDescriptions, count);
    for (tissue = tissues; tissue != NULL; tissue = tissue->next)
        extras->tissueDescriptions[tissue->id] = cloneString(tissue->description);
    }
if (id >= count)
    errAbort("GTEx tissue table problem: can't find id %d\n", id);
return extras->tissueDescriptions[id];
}

struct rgbColor *getGtexTissueColors(struct track *tg, char *version)
/* Get RGB colors from tissue table */
{
struct gtexTissue *tissues = getTissues(tg, version);
struct gtexTissue *tissue = NULL;
int count = getTissueCount(tg, version);
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
if (!extras->colors)
    {
    AllocArray(extras->colors, count);
    int i = 0;
    for (tissue = tissues; tissue != NULL; tissue = tissue->next)
        {
        // TODO: reconcile 
        extras->colors[i] = (struct rgbColor){.r=COLOR_32_BLUE(tissue->color), .g=COLOR_32_GREEN(tissue->color), .b=COLOR_32_RED(tissue->color)};
        //colors[i] = mgColorIxToRgb(NULL, tissue->color);
        i++;
        }
    }
return extras->colors;
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

static void loadComputedMedians(struct track *tg, struct gtexGeneInfo *geneInfo)
/* Compute medians based on graph type.  Returns a list of 2 for comparison graph types */
{
struct gtexGeneBed *geneBed = geneInfo->geneBed;
int expCount = geneBed->expCount;
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
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
        scores = hashFindVal(scoreHash1, getTissueName(tg, i, extras->version));
        if (scores)
            medians1[i] = slDoubleMedian(scores);
        scores = hashFindVal(scoreHash2, getTissueName(tg, i, extras->version));
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

static int gtexSquishItemHeight()
/* Height of squished item (request to have it larger than usual) */
{
return tl.fontHeight - tl.fontHeight/2;
}

static int gtexGeneBoxModelHeight()
/* Height of indicator box drawn under graph to show gene extent */
{
long winSize = virtWinBaseCount;
//FIXME: dupes!!
#define WIN_MAX_GRAPH 50000
#define WIN_MED_GRAPH 500000

#define MAX_GENE_BOX_HEIGHT     2
#define MED_GENE_BOX_HEIGHT     2
#define MIN_GENE_BOX_HEIGHT     1

if (winSize < WIN_MAX_GRAPH)
    return MAX_GENE_BOX_HEIGHT;
else if (winSize < WIN_MED_GRAPH)
    return MED_GENE_BOX_HEIGHT;
else
    return MIN_GENE_BOX_HEIGHT;
}

static int gtexGeneItemHeight(struct track *tg, void *item);

static void filterTissues(struct track *tg)
/* Check cart for tissue selection.  NULL out unselected tissues in tissue list */
{
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
struct gtexTissue *tis = NULL;
extras->tissues = getTissues(tg, extras->version);
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

static int maxTissueForGene(struct gtexGeneBed *geneBed)
/* Return id of highest expressed tissue for gene, if significantly higher than median.
 * If none are over threshold, return -1 */
{
double maxScore = 0.0, expScore;
double totalScore = 0.0;
int maxNum = 0, i;
int expCount = geneBed->expCount;
for (i=0; i<expCount; i++)
    {
    expScore = geneBed->expScores[i];
    if (expScore > maxScore)
        {
        maxScore = max(maxScore, expScore);
        maxNum = i;
        }
    totalScore += expScore;
    }
// threshold to consider this gene tissue specific -- a tissue contributes > 10% to 
// total expression level
if (totalScore < 1 || maxScore <= totalScore * .1)
    return -1;
return maxNum;
}

static Color gtexGeneItemColor(struct track *tg, void *item, struct hvGfx *hvg)
/* A bit of tissue-specific coloring in squish mode only, on geneBed item */
{
struct gtexGeneBed *geneBed = (struct gtexGeneBed *)item;
int id = maxTissueForGene(geneBed);
if (id < 0)
    return MG_BLACK;
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
struct rgbColor color = extras->colors[id];
return hvGfxFindColorIx(hvg, color.r, color.g, color.b);
}

static void gtexGeneLoadItems(struct track *tg)
/* Load method for track items */
{
/* Initialize colors for visibilities that don't display actual barchart */
if (tg->visibility == tvSquish || tg->limitedVis == tvSquish)
    tg->itemColor = gtexGeneItemColor;
tg->colorShades = shadesOfGray;

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
extras->codingOnly = cartUsualBooleanClosestToHome(cart, tg->tdb, FALSE, GTEX_CODING_GENE_FILTER,
                                                        GTEX_CODING_GENE_FILTER_DEFAULT);
extras->showExons = cartUsualBooleanClosestToHome(cart, tg->tdb, FALSE, GTEX_SHOW_EXONS,
                                                        GTEX_SHOW_EXONS_DEFAULT);
extras->noWhiteout = cartUsualBooleanClosestToHome(cart, tg->tdb, FALSE, GTEX_NO_WHITEOUT,
                                                        GTEX_NO_WHITEOUT_DEFAULT);
extras->labelStyle = getLabelStyle(cartUsualStringClosestToHome(cart, tg->tdb, FALSE, GTEX_LABEL,
                                                        GTEX_LABEL_DEFAULT));
/* Get geneModels in range */
char buf[256];
char *modelTable = "gtexGeneModel";
safef(buf, sizeof(buf), "%s%s", modelTable, extras->version ? extras->version: "");
struct hash *modelHash = loadGeneModels(buf);

/* Get geneBeds (names and all-sample tissue median scores) in range */
char *filter = getScoreFilterClause(cart, tg->tdb, NULL);
bedLoadItemWhere(tg, tg->table, filter, (ItemLoader)gtexGeneBedLoad);

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
    extras->colors = getGtexTissueColors(tg, extras->version);
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
    if (extras->codingOnly && !gtexGeneIsCoding(geneBed))
        {
        // apologies for messy short-circuit
        geneBed = geneBed->next;
        continue;
        }
    AllocVar(geneInfo);
    geneInfo->geneBed = geneBed;
    
    // set label
    if (extras->labelStyle == LABEL_GENE_SYMBOL)
        geneInfo->label = geneBed->name;
    else if (extras->labelStyle == LABEL_GENE_ACCESSION)
        geneInfo->label = geneBed->geneId;
    else if (extras->labelStyle == LABEL_BOTH)
        {
        char buf[256];
        safef(buf, sizeof(buf), "%s/%s", geneBed->name, geneBed->geneId);
        geneInfo->label = cloneString(buf);
        }
    else
        geneInfo->label = "";

    // get description
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
        // also strip 'homo sapiens' prefix
        #define SPECIES_PREFIX  "Homo sapiens "
        if (startsWith(SPECIES_PREFIX, desc))
            desc += strlen(SPECIES_PREFIX);
        geneInfo->description = desc;
        }
    else
        geneInfo->description = geneInfo->geneBed->name;

    slAddHead(&list, geneInfo);
    geneBed = geneBed->next;
    geneInfo->geneBed->next = NULL;

    if (extras->isComparison && (tg->visibility == tvFull || tg->visibility == tvPack))
        // compute medians based on configuration (comparisons, and later, filters)
        loadComputedMedians(tg, geneInfo);
    geneInfo->height = gtexGeneItemHeight(tg, geneInfo);

    }
slReverse(&list);
tg->items = list;
}

/***********************************************/
/* Draw */

/* Bargraph layouts for three window sizes */
#define WIN_MAX_GRAPH 50000
#define MAX_GRAPH_HEIGHT 175
#define MAX_BAR_WIDTH 5
#define MAX_GRAPH_PADDING 2

#define WIN_MED_GRAPH 500000
#define MED_GRAPH_HEIGHT 100
#define MED_BAR_WIDTH 3
#define MED_GRAPH_PADDING 1

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
if (!extras->showExons)
    return tvSquish;
long winSize = virtWinBaseCount;
if (winSize < WIN_MED_GRAPH && !extras->isComparison)
    return tvPack;
return tvSquish;
}

static int gtexGeneModelHeight(struct gtexGeneExtras *extras)
{
if (!extras->showExons)
    return gtexGeneBoxModelHeight()+3;
enum trackVisibility vis = gtexGeneModelVis(extras);
if (vis == tvSquish)
    return trunc(tl.fontHeight/2) + 1;
return tl.fontHeight;
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
    return tl.fontHeight * 4;
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
    if (!filterTissue(tg, getTissueName(tg, i, extras->version)))
        continue;
    if (doTop)
        expScore = (geneInfo->medians1 ? geneInfo->medians1[i] : geneBed->expScores[i]);
    else
        expScore = geneInfo->medians2[i];
    maxExp = max(maxExp, expScore);
    }
double viewMax = (double)cartUsualIntClosestToHome(cart, tg->tdb, FALSE, 
                                GTEX_MAX_VIEW_LIMIT, GTEX_MAX_VIEW_LIMIT_DEFAULT);
double maxMedian = ((struct gtexGeneExtras *)tg->extraUiData)->maxMedian;
return valToClippedHeight(maxExp, maxMedian, viewMax, gtexMaxGraphHeight(), extras->doLogTransform);
}

static void drawGraphBox(struct track *tg, struct gtexGeneInfo *geneInfo, struct hvGfx *hvg, int x, int y)
/* Draw white background for graph */
{
Color lighterGray = MAKECOLOR_32(0xF3, 0xF3, 0xF3);
int width = gtexGraphWidth(tg, geneInfo);
int height = gtexGeneGraphHeight(tg, geneInfo, TRUE);
hvGfxOutlinedBox(hvg, x, y-height, width, height, MG_WHITE, lighterGray);
}

static void drawGraphBase(struct track *tg, struct gtexGeneInfo *geneInfo, struct hvGfx *hvg, int x, int y)
/* Draw faint line under graph to delineate extent when bars are missing (tissue w/ 0 expression) */
{
Color lightGray = MAKECOLOR_32(0xD1, 0xD1, 0xD1);
int graphWidth = gtexGraphWidth(tg, geneInfo);
hvGfxBox(hvg, x, y, graphWidth, 1, lightGray);
}

static void gtexGeneDrawAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y, 
                double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw tissue expression bar graph over gene model. 
   Optionally, draw a second graph under gene, to compare sample sets */
{
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
struct gtexGeneInfo *geneInfo = (struct gtexGeneInfo *)item;
struct gtexGeneBed *geneBed = geneInfo->geneBed;
if (vis == tvDense)
    {
    bedDrawSimpleAt(tg, geneBed, hvg, xOff, y, scale, font, MG_WHITE, vis);     // color ignored (using grayscale)
    return;
    }
if (vis == tvSquish)
    {
    Color color = gtexGeneItemColor(tg, geneBed, hvg);
    int height = gtexSquishItemHeight();
    drawScaledBox(hvg, geneBed->chromStart, geneBed->chromEnd, scale, xOff, y, height, color);
    return;
    }

int graphX = gtexGraphX(geneBed);
if (graphX < 0)
    return;

// draw gene model
int topGraphHeight = gtexGeneGraphHeight(tg, geneInfo, TRUE);
topGraphHeight = max(topGraphHeight, tl.fontHeight);
int yZero = topGraphHeight + y - 1;  // yZero is bottom of graph
int yGene = yZero + gtexGeneMargin();
int heightPer = tg->heightPer;
tg->heightPer = gtexGeneModelHeight(extras);
Color statusColor = getGeneClassColor(hvg, geneBed);
if (geneInfo->geneModel && extras->showExons)
    {
    struct linkedFeatures *lf = linkedFeaturesFromGenePred(tg, geneInfo->geneModel, FALSE);
    lf->filterColor = statusColor;
    linkedFeaturesDrawAt(tg, lf, hvg, xOff, yGene-1, scale, font, color, gtexGeneModelVis(extras));
    }
else
    {
    int height = gtexGeneBoxModelHeight();
    drawScaledBox(hvg, geneBed->chromStart, geneBed->chromEnd, scale, xOff, yGene+1, height, statusColor);
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

if (!extras->noWhiteout)
    drawGraphBox(tg, geneInfo, hvg, keepX, yZero+1);

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
                                GTEX_MAX_VIEW_LIMIT, GTEX_MAX_VIEW_LIMIT_DEFAULT);
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
        hvGfxBox(hvg, x1, yZero-height+1, barWidth, 2, clipColor);
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
        hsl.s = min(1000, hsl.s + 200);
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
        hvGfxBox(hvg, x1, yZero + height-1, barWidth, 2, clipColor);
    x1 = x1 + barWidth + graphPadding;
    }
}

static int gtexGeneItemHeightOptionalMax(struct track *tg, void *item, boolean isMax)
{
// It seems that this can be called early or late
enum trackVisibility vis = tg->visibility;
if (tg->limitedVisSet)
    vis = tg->limitedVis;

int height;
if (vis == tvSquish || vis == tvDense)
    {
    if (vis == tvSquish)
        {
        tg->lineHeight = gtexSquishItemHeight();
        tg->heightPer = tg->lineHeight;
        }
    height = tgFixedItemHeight(tg, item);
    return height;
    }
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
if (isMax)
    {
    int extra = 0;
    if (((struct gtexGeneExtras *)tg->extraUiData)->isComparison)
        extra = gtexMaxGraphHeight() + 2;
    height= gtexMaxGraphHeight() + gtexGeneMargin() + gtexGeneModelHeight(extras) + extra;
    return height;
    }
struct gtexGeneInfo *geneInfo = (struct gtexGeneInfo *)item;

if (geneInfo->height != 0)
    {
    return geneInfo->height;
    }
    int topGraphHeight = gtexGeneGraphHeight(tg, geneInfo, TRUE);
topGraphHeight = max(topGraphHeight, tl.fontHeight);
int bottomGraphHeight = 0;
boolean isComparison = ((struct gtexGeneExtras *)tg->extraUiData)->isComparison;
if (isComparison)
    {
    bottomGraphHeight = max(gtexGeneGraphHeight(tg, geneInfo, FALSE),
                                tl.fontHeight) + gtexGeneMargin();
    }
height = topGraphHeight + bottomGraphHeight + gtexGeneMargin() + 
                gtexGeneModelHeight(extras);
return height;
}

static int gtexGeneItemHeight(struct track *tg, void *item)
{
int height = gtexGeneItemHeightOptionalMax(tg, item, FALSE);
return height;
}

static char *tissueExpressionText(struct gtexTissue *tissue, double expScore, 
                                        boolean doLogTransform, char *qualifier)
/* Construct mouseover text for tissue graph */
{
static char buf[128];
doLogTransform = FALSE; // for now, always display expression level on graph as raw RPKM
safef(buf, sizeof(buf), "%s (%.1f %s%s%sRPKM)", tissue->description, 
                                doLogTransform ? log10(expScore+1.0) : expScore,
                                qualifier != NULL ? qualifier : "",
                                qualifier != NULL ? " " : "",
                                doLogTransform ? "log " : "");
return buf;
}

static void getItemX(int start, int end, int *x1, int *x2)
/* Return startX, endX based on item coordinates and current window */
// Residual (largely replaced by drawScaledBox -- still used by gene model bmap box
{
int s = max(start, winStart);
int e = min(end, winEnd);
double scale = scaleForWindow(insideWidth, winStart, winEnd);
assert(x1);
*x1 = round((double)((int)s-winStart)*scale + insideX);
assert(x2);
*x2 = round((double)((int)e-winStart)*scale + insideX);
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

static void gtexGeneMapItem(struct track *tg, struct hvGfx *hvg, void *item, char *itemName, 
                        char *mapItemName, int start, int end, int x, int y, int width, int height)
/* Create a map box on gene model and label, and one for each tissue (bar in the graph) in
 * pack or full mode.  Just single map for squish/dense modes */
{
if (tg->limitedVis == tvDense)
    {
    genericMapItem(tg, hvg, item, itemName, itemName, start, end, x, y, width, height);
    return;
    }
struct gtexGeneInfo *geneInfo = item;
struct gtexGeneBed *geneBed = geneInfo->geneBed;
struct gtexGeneExtras *extras = (struct gtexGeneExtras *)tg->extraUiData;
int geneStart = geneBed->chromStart;
int geneEnd = geneBed->chromEnd;
if (tg->limitedVis == tvSquish)
    {
    int tisId = maxTissueForGene(geneBed);
    char *maxTissue = "";
    if (tisId > 1)
        maxTissue = getTissueDescription(tg, tisId, extras->version);
    char buf[128];
    safef(buf, sizeof buf, "%s %s", geneBed->name, maxTissue);
    int x1, x2;
    getItemX(geneStart, geneEnd, &x1, &x2);
    int width = max(1, x2-x1);
    mapBoxHc(hvg, geneStart, geneEnd, x1, y, width, height, 
                 tg->track, mapItemName, buf);
    return;
    }
int topGraphHeight = gtexGeneGraphHeight(tg, geneInfo, TRUE);
topGraphHeight = max(topGraphHeight, tl.fontHeight);        // label
int yZero = topGraphHeight + y - 1;  // yZero is bottom of graph
int x1 = insideX;


// add maps to tissue bars in expresion graph
struct gtexTissue *tissues = getTissues(tg, extras->version);
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
                                GTEX_MAX_VIEW_LIMIT, GTEX_MAX_VIEW_LIMIT_DEFAULT);
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
    mapBoxHc(hvg, geneStart, geneEnd, x1, yZero-height, barWidth, height, tg->track, mapItemName,  
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
        mapBoxHc(hvg, geneStart, geneEnd, x1, y, barWidth, height, tg->track, mapItemName,
                tissueExpressionText(tissue, expScore, extras->doLogTransform, qualifier));
        }
    x1 = x1 + barWidth + padding;
    }

// add map boxes with description to gene model
if (geneInfo->geneModel && geneInfo->description)
    {
    // perhaps these are just start, end ?
    int itemStart = geneInfo->geneModel->txStart;
    int itemEnd = gtexGeneItemEnd(tg, item);
    int x1, x2;
    getItemX(itemStart, itemEnd, &x1, &x2);
    int w = x2-x1;
    int labelWidth = mgFontStringWidth(tl.font, itemName);
    if (x1-labelWidth <= insideX)
        labelWidth = 0;
    // map over label
    int itemHeight = geneInfo->height;
    mapBoxHc(hvg, geneStart, geneEnd, x1-labelWidth, y, labelWidth, itemHeight-3, 
                        tg->track, mapItemName, geneInfo->description);
    // map over gene model (extending to end of item)
    int geneModelHeight = gtexGeneModelHeight(extras);
    mapBoxHc(hvg, geneStart, geneEnd, x1, y+itemHeight-geneModelHeight-3, w, geneModelHeight,
                        tg->track, mapItemName, geneInfo->description);
    } 
}

static char *gtexGeneItemName(struct track *tg, void *item)
/* Return gene name */
{
struct gtexGeneInfo *geneInfo = (struct gtexGeneInfo *)item;
return geneInfo->label;
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
int lineHeight = 0;
int heightPer = 0;

if (vis == tvDense)
    {
    heightPer = tl.fontHeight;
    lineHeight=heightPer+1;
    }
else if (vis == tvSquish)
    {
    // for visibility, set larger than the usual squish, which is half font height
    heightPer = gtexSquishItemHeight() * 2;  // the squish packer halves this
    lineHeight=heightPer+1;
    }
else if ((vis == tvPack) || (vis == tvFull))
    {
    // layout -- initially as fixed height
    heightPer = tl.fontHeight;
    lineHeight=heightPer;
    }

height = tgFixedTotalHeightOptionalOverflow(tg, vis, lineHeight, heightPer, FALSE);

if ((vis == tvPack) || (vis == tvFull))
    {
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
	struct spaceSaver *ss = findSpaceSaver(tg, vis); // ss is a list now
	assert(ss); // viz matches, we have the right one
	height = spaceSaverGetRowHeightsTotal(ss);
        }
    }
tg->height = height;

return height;
}

static void gtexGenePreDrawItems(struct track *tg, int seqStart, int seqEnd,
                                struct hvGfx *hvg, int xOff, int yOff, int width,
                                MgFont *font, Color color, enum trackVisibility vis)
{
if (vis == tvSquish || vis == tvDense)
    {
    // NonProp routines not relevant to these modes, and they interfere
    // NOTE: they must be installed by gtexGeneMethods() for pack mode
    tg->nonPropDrawItemAt = NULL;
    tg->nonPropPixelWidth = NULL;
    }
}

void gtexGeneMethods(struct track *tg)
{
tg->drawItemAt = gtexGeneDrawAt;
tg->preDrawItems = gtexGenePreDrawItems;
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

