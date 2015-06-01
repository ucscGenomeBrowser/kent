/* transMapTracks - transMap track methods */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
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

static boolean isLabelTypeEnabled(struct track *tg, char *labelNameSuffix,
                                  boolean defaultVal) 
/* determine if a particular type of label is enabled for this track.
 * check cart, settings, and then use default.
 */
{
/* some how the closest to home magic prepends the track name for cart  */
char settingName[64];
safef(settingName, sizeof(settingName), "label.%s", labelNameSuffix);
fprintf(stderr, "%s\n", tg->tdb->settings);
return cartOrTdbBoolean(cart, tg->tdb, settingName, defaultVal);
}

static unsigned getLabelTypes(struct track *tg)
/* get set of labels to use */
{
unsigned labelSet = 0;

if (isLabelTypeEnabled(tg, "orgCommon", transMapLabelDefaultOrgCommon))
    labelSet |= useOrgCommon;
if (isLabelTypeEnabled(tg, "orgAbbrv", transMapLabelDefaultOrgAbbrv))
    labelSet |= useOrgAbbrv;
if (isLabelTypeEnabled(tg, "db", transMapLabelDefaultDb))
    labelSet |= useOrgDb;
if (isLabelTypeEnabled(tg, "gene", transMapLabelDefaultGene))
    labelSet |= useGene;
if (isLabelTypeEnabled(tg, "acc", transMapLabelDefaultAcc))
    labelSet |= useAcc;
return labelSet;
}

static void getItemLabel(struct sqlConnection *conn, char *transMapInfoTbl, 
                         struct sqlConnection *geneConn, char *transMapGeneTbl,
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
if ((labelSet & useGene) && (geneConn != NULL))
    {
    gene = transMapGeneQuery(geneConn, transMapGeneTbl,
                             info->srcDb, transMapIdToSeqId(info->srcId));
    }

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
struct sqlConnection *geneConn = NULL;
if (transMapGeneTbl != NULL)
    geneConn = hAllocConnDbTbl(transMapGeneTbl, &transMapGeneTbl, database);
    

struct linkedFeatures *lf;
unsigned labelSet = getLabelTypes(tg);

for (lf = tg->items; lf != NULL; lf = lf->next)
    getItemLabel(conn, transMapInfoTbl, geneConn, transMapGeneTbl, labelSet, lf);
hFreeConn(&geneConn);
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
return transMapIdToSeqId(itemName);
}

static void transMapMethods(struct track *tg)
/* Make track for transMap alignments. */
{
tg->loadItems = loadTransMap;
tg->itemName = refGeneName;
tg->mapItemName = linkedFeaturesName;
tg->itemDataName = transMapGetItemDataName;
}

void transMapRegisterTrackHandlers()
/* register track handlers for transMap tracks */
{
registerTrackHandler("transMap", transMapMethods);
registerTrackHandler("transMapAlnRefSeq", transMapMethods);
registerTrackHandler("transMapAlnMRna", transMapMethods);
registerTrackHandler("transMapAlnSplicedEst", transMapMethods);
registerTrackHandler("transMapAlnUcscGenes", transMapMethods);
// reconstruction
registerTrackHandler("reconTransMapAlnRefSeq", transMapMethods);
}
