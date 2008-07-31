/* transMapTracks - transMap track methods */
#include "common.h"
#include "transMapTracks.h"
#include "hgTracks.h"
#include "hdb.h"
#include "transMapStuff.h"
#include "transMapInfo.h"
#include "transMapSrc.h"
#include "transMapGene.h"

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

static unsigned getLabelTypes(struct track *tg)
/* get set of labels to use */
{
unsigned labelSet = 0;

// label setting are on parent track
char prefix[128];
safef(prefix, sizeof(prefix), "%s.label", tg->tdb->tableName);
struct hashEl *labels = cartFindPrefix(cart, prefix);

if (labels == NULL)
    {
    // default to common name+accession and save this in cart so it makes sense in trackUi
    labelSet = useAcc;
    char setting[64];
    safef(setting, sizeof(setting), "%s.label.acc", tg->tdb->tableName);
    cartSetBoolean(cart, setting, TRUE);
    safef(setting, sizeof(setting), "%s.label.orgCommon", tg->tdb->tableName);
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
                         char *transMapInfoTbl, char *transMapGeneTbl,
                         unsigned labelSet,
                         struct linkedFeatures *lf)
/* get label for a transMap item */
{
struct transMapInfo *info = NULL;
struct transMapGene *gene = NULL;
boolean srcDbExists = FALSE;
if (labelSet & (useOrgCommon|useOrgAbbrv|useOrgDb|useGene))
    {
    info = transMapInfoQuery(conn, transMapInfoTbl, lf->name);
    srcDbExists = sqlDatabaseExists(info->srcDb);
    }
if ((labelSet & useGene) && (transMapGeneTbl != NULL))
    gene = transMapGeneQuery(conn, transMapGeneTbl,
                             info->srcDb, transMapIdToAcc(info->srcId));

struct dyString *label = dyStringNew(64);
if (labelSet & useOrgAbbrv && srcDbExists)
    addToLabel(label, orgShortForDb(info->srcDb));
if (labelSet & useOrgCommon && srcDbExists)
    addToLabel(label, hOrganism(info->srcDb));
if (labelSet & useOrgDb)
    addToLabel(label, info->srcDb);
if (labelSet & useGene) 
    {
    if ((gene != NULL) && (strlen(gene->geneName) > 0))
        addToLabel(label, gene->geneName);
    else
        labelSet |= useAcc;  // no gene, so force acc
    }
if (labelSet & useAcc)
    addToLabel(label, transMapIdToAcc(lf->name));

transMapInfoFree(&info);
transMapGeneFree(&gene);
lf->extra = dyStringCannibalize(&label);
}

static void lookupTransMapLabels(struct track *tg)
/* This converts the transMap ids to labels. */
{
struct sqlConnection *conn = hAllocConn(database);
char *transMapInfoTbl = trackDbRequiredSetting(tg->tdb, transMapInfoTblSetting);
char *transMapGeneTbl = trackDbSetting(tg->tdb, transMapGeneTblSetting);

struct linkedFeatures *lf;
unsigned labelSet = getLabelTypes(tg);

for (lf = tg->items; lf != NULL; lf = lf->next)
    getItemLabel(conn, transMapInfoTbl, transMapGeneTbl, labelSet, lf);
hFreeConn(&conn);
}

static void loadTransMap(struct track *tg)
/* Load up transMap alignments. */
{
loadXenoPsl(tg);
enum trackVisibility vis = limitVisibility(tg);
if (!((vis == tvDense) || (vis == tvSquish)))
    lookupTransMapLabels(tg);
if (vis != tvDense)
    slSort(&tg->items, linkedFeaturesCmpStart);
}

static char *transMapGetItemDataName(struct track *tg, char *itemName)
/* translate itemName to data name (source accession).
 * WARNING: static return */
{
return transMapIdToAcc(itemName);
}

static void transMapMethods(struct track *tg)
/* Make track for transMap alignments. */
{
tg->loadItems = loadTransMap;
tg->itemName = refGeneName;
tg->mapItemName = refGeneMapName;
tg->itemDataName = transMapGetItemDataName;
}

void transMapRegisterTrackHandlers()
/* register track handlers for transMap tracks */
{
registerTrackHandler("transMapAlnRefSeq", transMapMethods);
registerTrackHandler("transMapAlnMRna", transMapMethods);
registerTrackHandler("transMapAlnSplicedEst", transMapMethods);
registerTrackHandler("transMapAlnUcscGenes", transMapMethods);
// reconstruction
registerTrackHandler("reconTransMapAlnRefSeq", transMapMethods);
}
