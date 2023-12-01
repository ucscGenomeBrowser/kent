/* Convert a (sub)tree with condensed nodes to JSON for Nextstrain to display, adding in sample
 * mutations, protein changes and metadata. */

/* Copyright (C) 2020 The Regents of the University of California */

#include "common.h"
#include "dnaseq.h"
#include "errCatch.h"
#include "hash.h"
#include "hui.h"
#include "jsonWrite.h"
#include "linefile.h"
#include "parsimonyProto.h"
#include "phyloPlace.h"
#include "phyloTree.h"
#include "variantProjector.h"


static void auspiceMetaColoringCategoricalStart(struct jsonWrite *jw, char *key, char *title)
/* Write key, title and type of a "categorical" coloring spec, but leave it open in case a
 * scale list needs to be added. */
{
jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, "key", key);
jsonWriteString(jw, "title", title);
jsonWriteString(jw, "type", "categorical");
}

static void auspiceMetaColoringCategoricalEnd(struct jsonWrite *jw)
/* Close out a coloring spec that was opened with auspiceMetaColoringCategoricalStart. */
{
jsonWriteObjectEnd(jw);
}

static void auspiceMetaColoringCategorical(struct jsonWrite *jw, char *key, char *title)
/* Write a "categorical" coloring spec with no scale component. */
{
auspiceMetaColoringCategoricalStart(jw, key, title);
auspiceMetaColoringCategoricalEnd(jw);
}

static void jsonWritePair(struct jsonWrite *jw, char *valA, char *valB)
/* Write a list with two string values. */
{
jsonWriteListStart(jw, NULL);
jsonWriteString(jw, NULL, valA);
jsonWriteString(jw, NULL, valB);
jsonWriteListEnd(jw);
}

static void auspiceMetaColoringSarsCov2Nextclade(struct jsonWrite *jw, char *key, char *title)
/* Write a coloring spec for SARS-CoV-2 Nextstrain clades.  (Non-VoC clades are omitted and
 * will be assigned grayscale values by Auspice. */
{
auspiceMetaColoringCategoricalStart(jw, key, title);
jsonWriteListStart(jw, "scale");
// Color hex values are words from line N of
// https://github.com/nextstrain/ncov/blob/master/defaults/color_schemes.tsv
// where N = number of clade_membership lines in
// https://github.com/nextstrain/ncov/blob/master/defaults/color_ordering.tsv
jsonWritePair(jw, "20H (Beta, V2)", "#5E1D9D");
jsonWritePair(jw, "20I (Alpha, V1)", "#492AB5");
jsonWritePair(jw, "20J (Gamma, V3)", "#4042C7");
jsonWritePair(jw, "21A (Delta)", "#3E5DD0");
jsonWritePair(jw, "21I (Delta)", "#4377CD");
jsonWritePair(jw, "21J (Delta)", "#4A8CC2");
jsonWritePair(jw, "21B (Kappa)", "#549DB2");
jsonWritePair(jw, "21C (Epsilon)", "#60AA9E");
jsonWritePair(jw, "21D (Eta)", "#6EB389");
jsonWritePair(jw, "21E (Theta)", "#80B974");
jsonWritePair(jw, "21F (Iota)", "#92BC63");
jsonWritePair(jw, "21G (Lambda)", "#A6BE55");
jsonWritePair(jw, "21H (Mu)", "#B9BC4A");
jsonWritePair(jw, "21K (Omicron)", "#CBB742");
jsonWritePair(jw, "21L (Omicron)", "#D9AD3D");
jsonWritePair(jw, "21M (Omicron)", "#E29D39");
jsonWritePair(jw, "22A (Omicron)", "#E68634");
jsonWritePair(jw, "22B (Omicron)", "#E56A2F");
jsonWritePair(jw, "22C (Omicron)", "#E04929");
jsonWritePair(jw, "22D (Omicron)", "#DB2823");
jsonWritePair(jw, "uploaded sample", "#000000");
jsonWriteListEnd(jw);
auspiceMetaColoringCategoricalEnd(jw);
}

static void writeAuspiceMetaGenomeAnnotations(struct jsonWrite *jw, struct geneInfo *geneInfoList,
                                              uint genomeSize)
/* Write out auspice genome annotations (protein-coding gene CDS and "nuc"). */
{
jsonWriteObjectStart(jw, "genome_annotations");
struct geneInfo *gi;
for (gi = geneInfoList;  gi != NULL;  gi = gi->next)
    {
    jsonWriteObjectStart(jw, gi->psl->qName);
    if (gi->psl->blockCount > 1)
        {
        jsonWriteListStart(jw, "segments");
        int i;
        for (i = 0;  i < gi->psl->blockCount;  i++)
            {
            jsonWriteObjectStart(jw, NULL);
            jsonWriteNumber(jw, "start", gi->psl->tStarts[i]+1);
            jsonWriteNumber(jw, "end", gi->psl->tStarts[i] + gi->psl->blockSizes[i]);
            jsonWriteObjectEnd(jw);
            }
        jsonWriteListEnd(jw);
        }
    else
        {
        jsonWriteNumber(jw, "start", gi->psl->tStart+1);
        jsonWriteNumber(jw, "end", gi->psl->tEnd);
        }
    jsonWriteString(jw, "strand", (pslOrientation(gi->psl) > 0) ? "+" : "-");
    jsonWriteString(jw, "type", "CDS");
    jsonWriteObjectEnd(jw);
    }
jsonWriteObjectStart(jw, "nuc");
jsonWriteNumber(jw, "start", 1);
jsonWriteNumber(jw, "end", genomeSize);
jsonWriteString(jw, "strand", "+");
jsonWriteString(jw, "type", "source");
jsonWriteObjectEnd(jw);
jsonWriteObjectEnd(jw);
}

static char *getDefaultColor(struct slName *colorFields)
/* Pick default color from available color fields from metadata.  Do not free returned string. */
{
char *colorDefault = NULL;
if (slNameInList(colorFields, "pango_lineage_usher"))
    colorDefault = "pango_lineage_usher";
else if (slNameInList(colorFields, "Nextstrain_lineage"))
    colorDefault = "Nextstrain_lineage";
else if (slNameInList(colorFields, "Nextstrain_clade"))
    colorDefault = "Nextstrain_clade";
else if (slNameInList(colorFields, "GCC_usher"))
    colorDefault = "GCC_usher";
else if (colorFields != NULL)
    colorDefault = colorFields->name;
else
    colorDefault = "userOrOld";
return colorDefault;
}

static void auspiceMetaColorings(struct jsonWrite *jw, char *source, struct slName *colorFields,
                                 char *db)
/* Write coloring specs for colorFields from metadata, locally added userOrOld, and
 * Auspice-automatic gt. */
{
jsonWriteListStart(jw, "colorings");
auspiceMetaColoringCategoricalStart(jw, "userOrOld", "Sample type");
jsonWriteListStart(jw, "scale");
jsonWritePair(jw, "uploaded sample", "#CC0000");
jsonWritePair(jw, source, "#000000");
jsonWriteListEnd(jw);
auspiceMetaColoringCategoricalEnd(jw);
auspiceMetaColoringCategorical(jw, "gt", "Genotype");
struct slName *col;
for (col = colorFields;  col != NULL;  col = col->next)
    {
    if (sameString(col->name, "Nextstrain_clade"))
        {
        if (sameString(db, "wuhCor1"))
            auspiceMetaColoringSarsCov2Nextclade(jw, col->name, "Nextstrain Clade");
        else
            auspiceMetaColoringCategorical(jw, col->name, "Clade assigned by nextclade");
        }
    else if (sameString(col->name, "Nextstrain_clade_usher"))
        auspiceMetaColoringSarsCov2Nextclade(jw, col->name, "Nextstrain Clade assigned by UShER");
    else if (sameString(col->name, "pango_lineage"))
        auspiceMetaColoringCategorical(jw, col->name, "Pango lineage");
    else if (sameString(col->name, "pango_lineage_usher"))
        auspiceMetaColoringCategorical(jw, col->name, "Pango lineage assigned by UShER");
    else if (sameString(col->name, "Nextstrain_lineage"))
        auspiceMetaColoringCategorical(jw, col->name, "Nextstrain lineage");
    //#*** RSV hacks -- colorings really should come from JSON file in config directory
    else if (sameString(col->name, "goya_nextclade"))
        auspiceMetaColoringCategorical(jw, col->name, "Goya 2020 clade assigned by nextclade");
    else if (sameString(col->name, "goya_usher"))
        auspiceMetaColoringCategorical(jw, col->name, "Goya 2020 clade assigned by UShER");
    else if (sameString(col->name, "GCC_nextclade"))
        auspiceMetaColoringCategorical(jw, col->name, "RGCC lineage assigned by nextclade");
    else if (sameString(col->name, "GCC_usher"))
        auspiceMetaColoringCategorical(jw, col->name, "RGCC lineage assigned by UShER");
    else if (sameString(col->name, "GCC_assigned_2023-11"))
        auspiceMetaColoringCategorical(jw, col->name, "RGCC designated lineage");
    else if (sameString(col->name, "country"))
        auspiceMetaColoringCategorical(jw, col->name, "Country");
    else
        auspiceMetaColoringCategorical(jw, col->name, col->name);
    }
jsonWriteListEnd(jw);
}

static void writeAuspiceMeta(struct jsonWrite *jw, struct slName *subtreeUserSampleIds, char *source,
                             char *db, struct slName *colorFields, struct geneInfo *geneInfoList,
                             uint genomeSize)
/* Write metadata to configure Auspice display. */
{
jsonWriteObjectStart(jw, "meta");
// Title
struct dyString *dy = dyStringCreate("Subtree with %s", subtreeUserSampleIds->name);
int sampleCount = slCount(subtreeUserSampleIds);
if (sampleCount > 10)
    dyStringPrintf(dy, " and %d other uploaded samples", sampleCount - 1);
else
    {
    struct slName *sln;
    for (sln = subtreeUserSampleIds->next;  sln != NULL;  sln = sln->next)
        dyStringPrintf(dy, ", %s", sln->name);
    }
jsonWriteString(jw, "title", dy->string);
// Description
jsonWriteStringf(jw, "description", "Dataset generated by [UShER web interface]"
                 "(%shgPhyloPlace) using the "
                 "[usher](https://github.com/yatisht/usher/) program.  "
//#*** TODO: describe input from which tree was generated: user sample, version of tree, etc.
                 "If you have metadata you wish to display, you can now drag on a CSV file and "
                 "it will be added into this view, [see here]("NEXTSTRAIN_DRAG_DROP_DOC") "
                 "for more info."
                 , hLocalHostCgiBinUrl());
// Panels: just the tree and  entropy (no map)
jsonWriteListStart(jw, "panels");
jsonWriteString(jw, NULL, "tree");
jsonWriteString(jw, NULL, "entropy");
jsonWriteListEnd(jw);
// Default label & color
jsonWriteObjectStart(jw, "display_defaults");
jsonWriteString(jw, "branch_label", "aa mutations");
jsonWriteString(jw, "color_by", getDefaultColor(colorFields));
jsonWriteObjectEnd(jw);
// Colorings: userOrOld, gt and whatever we got from metadata
auspiceMetaColorings(jw, source, colorFields, db);
// Filters didn't work when I tried them a long time ago... revisit someday.
jsonWriteListStart(jw, "filters");
jsonWriteString(jw, NULL, "userOrOld");
jsonWriteString(jw, NULL, "country");
//#*** FIXME: TODO: either pass in along with sampleMetadata, or take from JSON file specified
//#*** in config, or better yet, compute while building tree object in memory, then write the
//#*** header object, then write the tree.
if (sameString(db, "wuhCor1"))
    {
    jsonWriteString(jw, NULL, "pango_lineage_usher");
    jsonWriteString(jw, NULL, "pango_lineage");
    jsonWriteString(jw, NULL, "Nextstrain_clade_usher");
    jsonWriteString(jw, NULL, "Nextstrain_clade");
    }
else if (stringIn("GCF_000855545", db) || stringIn("GCF_002815475", db) || stringIn("RGCC", db))
    {
    jsonWriteString(jw, NULL, "GCC_usher");
    jsonWriteString(jw, NULL, "GCC_nextclade");
    jsonWriteString(jw, NULL, "GCC_assigned_2023-11");
    jsonWriteString(jw, NULL, "goya_usher");
    jsonWriteString(jw, NULL, "goya_nextclade");
    }
else if (stringIn("GCF_000865085", db) || stringIn("GCF_001343785", db))
    {
    jsonWriteString(jw, NULL, "Nextstrain_clade");
    }
else
    {
    jsonWriteString(jw, NULL, "Nextstrain_lineage");
    }
jsonWriteListEnd(jw);
// Annotations for coloring/filtering by base
writeAuspiceMetaGenomeAnnotations(jw, geneInfoList, genomeSize);
jsonWriteObjectEnd(jw);
}

static void jsonWriteObjectValueUrl(struct jsonWrite *jw, char *name, char *value, char *url)
/* Write an object with member "value" set to value, and if url is non-empty, "url" set to url. */
{
jsonWriteObjectStart(jw, name);
jsonWriteString(jw, "value", value);
if (isNotEmpty(url))
    jsonWriteString(jw, "url", url);
jsonWriteObjectEnd(jw);
}

static void jsonWriteObjectValue(struct jsonWrite *jw, char *name, char *value)
/* Write an object with one member, "value", set to value, as most Auspice node attributes are
 * formatted. */
{
jsonWriteObjectValueUrl(jw, name, value, NULL);
}

static void makeLineageUrl(char *lineage, char *lineageUrl, size_t lineageUrlSize)
/* If lineage is not "uploaded sample", make an outbreak.info link to it, otherwise just copy
 * lineage. */
{
if (sameString(lineage, "uploaded sample"))
    safecpy(lineageUrl, lineageUrlSize, lineage);
else
    safef(lineageUrl, lineageUrlSize, OUTBREAK_INFO_URLBASE "%s", lineage);
}

static void jsonWriteLeafNodeAttributes(struct jsonWrite *jw, char *name,
                                        struct sampleMetadata *met, boolean isUserSample,
                                        char *source, struct hash *sampleUrls,
                                        struct hash *samplePlacements, boolean isRsv,
                                        char **retUserOrOld, char **retNClade, char **retGClade,
                                        char **retLineage, char **retNLineage,
                                        char **retNCladeUsher, char **retLineageUsher)
/* Write elements of node_attrs for a sample which may be preexisting and in our metadata hash,
 * or may be a new sample from the user.  Set rets for color categories so parent branches can
 * determine their color categories. */
{
*retUserOrOld = isUserSample ? "uploaded sample" : source;
jsonWriteObjectValue(jw, "userOrOld", *retUserOrOld);
if (met && met->date)
    jsonWriteObjectValue(jw, "date", met->date);
if (met && met->author)
    {
    jsonWriteObjectValue(jw, "author", met->author);
    // Note: Nextstrain adds paper_url and title when available; they also add author and use
    // a uniquified value (e.g. "author": "Wenjie Tan et al" / "value": "Wenjie Tan et al A")
    }
struct placementInfo *pi = (isUserSample && name) ? hashFindVal(samplePlacements, name) : NULL;

*retNClade = (met && met->nClade) ? met->nClade : isUserSample ? "uploaded sample" : NULL;
if (isNotEmpty(*retNClade))
    jsonWriteObjectValue(jw, (isRsv ? "goya_nextclade" : "Nextstrain_clade"), *retNClade);
*retGClade = (met && met->gClade) ? met->gClade : isUserSample ? "uploaded sample" : NULL;
if (isNotEmpty(*retGClade))
    jsonWriteObjectValue(jw, (isRsv ? "GCC_assigned_2023-11" : "GISAID_clade"), *retGClade);
*retLineage =  (met && met->lineage) ? met->lineage : isUserSample ? "uploaded sample" : NULL;
if (isNotEmpty(*retLineage))
    {
    char lineageUrl[1024];
    makeLineageUrl(*retLineage, lineageUrl, sizeof lineageUrl);
    jsonWriteObjectValueUrl(jw, (isRsv ? "GCC_nextclade" : "pango_lineage"),
                            *retLineage, lineageUrl);
    }
*retNLineage = (met && met->nLineage) ? met->nLineage : isUserSample ? "uploaded sample" : NULL;
if (isNotEmpty(*retNLineage))
    {
    jsonWriteObjectValue(jw, "Nextstrain_lineage", *retNLineage);
    }
if (met && met->epiId)
    jsonWriteObjectValue(jw, "gisaid_epi_isl", met->epiId);
if (met && met->gbAcc)
    jsonWriteObjectValue(jw, "genbank_accession", met->gbAcc);
if (met && met->country)
    jsonWriteObjectValue(jw, "country", met->country);
if (met && met->division)
    jsonWriteObjectValue(jw, "division", met->division);
if (met && met->location)
    jsonWriteObjectValue(jw, "location", met->location);
if (met && met->countryExp)
    jsonWriteObjectValue(jw, "country_exposure", met->countryExp);
if (met && met->divExp)
    jsonWriteObjectValue(jw, "division_exposure", met->divExp);
if (met && met->origLab)
    jsonWriteObjectValue(jw, "originating_lab", met->origLab);
if (met && met->subLab)
    jsonWriteObjectValue(jw, "submitting_lab", met->subLab);
if (met && met->region)
    jsonWriteObjectValue(jw, "region", met->region);
*retNCladeUsher = (pi && pi->nextClade) ? pi->nextClade :
                  (met && met->nCladeUsher) ? met->nCladeUsher :
                  isUserSample ? "uploaded sample" : NULL;
if (isNotEmpty(*retNCladeUsher))
    jsonWriteObjectValue(jw, (isRsv ? "goya_usher" : "Nextstrain_clade_usher"), *retNCladeUsher);
*retLineageUsher = (pi && pi->pangoLineage) ? pi->pangoLineage :
                   (met && met->lineageUsher) ? met->lineageUsher :
                   isUserSample ? "uploaded sample" : NULL;
if (isNotEmpty(*retLineageUsher))
    {
    char lineageUrl[1024];
    makeLineageUrl(*retLineageUsher, lineageUrl, sizeof lineageUrl);
    jsonWriteObjectValueUrl(jw, (isRsv ? "GCC_usher" : "pango_lineage_usher"),
                            *retLineageUsher, lineageUrl);
    }
char *sampleUrl = (sampleUrls && name) ? hashFindVal(sampleUrls, name) : NULL;
if (isNotEmpty(sampleUrl))
    {
    char *p = strstr(sampleUrl, "subtreeAuspice");
    char *subtreeNum = p + strlen("subtreeAuspice");
    if (p && isdigit(*subtreeNum))
        {
        int num = atoi(subtreeNum);
        char subtreeLabel[1024];
        safef(subtreeLabel, sizeof subtreeLabel, "view subtree %d", num);
        jsonWriteObjectValueUrl(jw, "subtree", subtreeLabel, sampleUrl);
        }
    else
        jsonWriteObjectValueUrl(jw, "subtree", sampleUrl, sampleUrl);
    }
}

static void jsonWriteBranchNodeAttributes(struct jsonWrite *jw, boolean isRsv, char *userOrOld,
                                          char *nClade, char *gClade, char *lineage, char *nLineage,
                                          char *nCladeUsher, char *lineageUsher)
/* Write elements of node_attrs for a branch. */
{
if (userOrOld)
    jsonWriteObjectValue(jw, "userOrOld", userOrOld);
if (nClade)
    jsonWriteObjectValue(jw, (isRsv ? "goya_nextclade" : "Nextstrain_clade"), nClade);
if (gClade)
    jsonWriteObjectValue(jw, (isRsv ? "GCC_assigned_2023-11" : "GISAID_clade"), gClade);
if (lineage)
    jsonWriteObjectValue(jw, (isRsv ? "GCC_nextclade" : "pango_lineage"), lineage);
if (nLineage)
    jsonWriteObjectValue(jw, "Nextstrain_lineage", lineage);
if (nCladeUsher)
    jsonWriteObjectValue(jw, (isRsv ? "goya_usher" : "Nextstrain_clade_usher"), nCladeUsher);
if (lineageUsher)
    jsonWriteObjectValue(jw, (isRsv ? "GCC_usher" : "pango_lineage_usher"), lineageUsher);
}

INLINE char maybeComplement(char base, struct psl *psl)
/* If psl is on '+' strand, return base, otherwise return the complement of base. */
{
return (pslOrientation(psl) > 0) ? base : ntCompTable[(int)base];
}

static struct slName *codonVpTxToAaChange(struct vpTx *codonVpTxList,
                                          struct singleNucChange *ancestorMuts,
                                          struct geneInfo *gi)
/* Given a list of vpTx from the same codon, combine their changes with inherited mutations
 * in the same codon to get the amino acid change at this node.
 * Note: this assumes there is no UTR in transcript, only CDS.  True so far for pathogens... */
{
struct slName *aaChange = NULL;
if (codonVpTxList != NULL)
    {
    struct vpTx *vpTx = codonVpTxList;
    int firstAaStart = (vpTx->start.txOffset - gi->cds->start) / 3;
    int codonStart = firstAaStart * 3;
    int codonOffset = vpTx->start.txOffset - gi->cds->start - codonStart;
    char oldCodon[4];
    safencpy(oldCodon, sizeof oldCodon, gi->txSeq->dna + gi->cds->start + codonStart, 3);
    touppers(oldCodon);
    boolean isRc = (pslOrientation(gi->psl) < 0);
    int codonStartG = isRc ? vpTx->start.gOffset + codonOffset : vpTx->start.gOffset - codonOffset;
    struct singleNucChange *anc;
    for (anc = ancestorMuts;  anc != NULL;  anc = anc->next)
        {
        int ancCodonOffset = isRc ? codonStartG - (anc->chromStart + 1) : anc->chromStart - codonStartG;
        if (ancCodonOffset >= 0 && ancCodonOffset < 3)
            {
            char parBase = isRc ? ntCompTable[(int)anc->parBase] : anc->parBase;
            if (parBase != oldCodon[ancCodonOffset])
                errAbort("codonVpTxToAaChange: expected parBase for ancestral mutation at %d "
                         "(%s codon %d offset %d) to be '%c' but it's '%c'%s",
                         anc->chromStart+1, gi->psl->qName, firstAaStart, ancCodonOffset,
                         oldCodon[ancCodonOffset], parBase,
                         isRc ? " (rev-comp'd)" : "");
            oldCodon[ancCodonOffset] = maybeComplement(anc->newBase, gi->psl);
            }
        }
    char oldAa = lookupCodon(oldCodon);
    if (oldAa == '\0')
        oldAa = '*';
    char newCodon[4];
    safecpy(newCodon, sizeof newCodon, oldCodon);
    for (vpTx = codonVpTxList;  vpTx != NULL;  vpTx = vpTx->next)
        {
        int aaStart = (vpTx->start.txOffset - gi->cds->start) / 3;
        if (aaStart != firstAaStart)
            errAbort("codonVpTxToAaChange: program error: firstAaStart %d != aaStart %d",
                     firstAaStart, aaStart);
        int codonOffset = vpTx->start.txOffset - gi->cds->start - codonStart;
        // vpTx->txRef[0] is always the reference base, not like singleNucChange parBase,
        // so we can't compare it to expected value as we could for ancMuts above.
        newCodon[codonOffset] = vpTx->txAlt[0];
        }
    char newAa = lookupCodon(newCodon);
    if (newAa == '\0')
        newAa = '*';
    if (newAa != oldAa)
        {
        char aaChangeStr[32];
        safef(aaChangeStr, sizeof aaChangeStr, "%c%d%c", oldAa, firstAaStart+1, newAa);
        aaChange = slNameNew(aaChangeStr);
        }
    }
return aaChange;
}

struct slPair *getAaMutations(struct singleNucChange *sncList, struct singleNucChange *ancestorMuts,
                              struct geneInfo *geneInfoList, struct seqWindow *gSeqWin)
/* Given lists of SNVs and genes, return a list of pairs of { gene name, AA change list }. */
{
struct slPair *geneChangeList = NULL;
struct geneInfo *gi;
for (gi = geneInfoList;  gi != NULL;  gi = gi->next)
    {
    struct slName *aaChangeList = NULL;
    int prevAaStart = -1;
    struct vpTx *codonVpTxList = NULL;
    struct singleNucChange *snc;
    for (snc = sncList;  snc != NULL;  snc = snc->next)
        {
        if (snc->chromStart < gi->psl->tEnd && snc->chromStart >= gi->psl->tStart)
            {
            struct bed3 gBed3 = { NULL, gSeqWin->seqName, snc->chromStart, snc->chromStart+1 };
            char gAlt[2];
            safef(gAlt, sizeof(gAlt), "%c", snc->newBase);
            struct vpTx *vpTx = vpGenomicToTranscript(gSeqWin, &gBed3, gAlt, gi->psl, gi->txSeq);
            if (vpTx->start.region == vpExon && vpTx->start.txOffset < gi->cds->end &&
                vpTx->end.txOffset > gi->cds->start)
                {
                int aaStart = (vpTx->start.txOffset - gi->cds->start) / 3;
                // Accumulate vpTxs in the same codon
                if (aaStart == prevAaStart || prevAaStart == -1)
                    {
                    slAddHead(&codonVpTxList, vpTx);
                    }
                else
                    {
                    // Done with previous codon; find change from accumulated mutations
                    struct slName *aaChange = codonVpTxToAaChange(codonVpTxList, ancestorMuts, gi);
                    if (aaChange)
                        slAddHead(&aaChangeList, aaChange);
                    slFreeListWithFunc(&codonVpTxList, vpTxFree);
                    codonVpTxList = vpTx;
                    }
                prevAaStart = aaStart;
                }
            }
        }
    // Find change from final set of accumulated mutations
    if (codonVpTxList != NULL)
        {
        struct slName *aaChange = codonVpTxToAaChange(codonVpTxList, ancestorMuts, gi);
        if (aaChange)
            slAddHead(&aaChangeList, aaChange);
        slFreeListWithFunc(&codonVpTxList, vpTxFree);
        }
    if (aaChangeList != NULL)
        {
        slReverse(&aaChangeList);
        slAddHead(&geneChangeList, slPairNew(gi->psl->qName, aaChangeList));
        }
    }
slReverse(&geneChangeList);
return geneChangeList;
}

static void jsonWriteBranchAttrs(struct jsonWrite *jw, struct phyloTree *node,
                                 struct singleNucChange *ancestorMuts,
                                 struct geneInfo *geneInfoList, struct seqWindow *gSeqWin)
/* Write mutations (if any).  If node is not a leaf, write label for mutations at this node. */
{
if (node->priv != NULL)
    {
    struct singleNucChange *sncList = node->priv;
    struct slPair *geneAaMutations = getAaMutations(sncList, ancestorMuts, geneInfoList, gSeqWin);
    jsonWriteObjectStart(jw, "branch_attrs");
    if (node->numEdges > 0)
        {
        jsonWriteObjectStart(jw, "labels");
        if (node->ident->name)
            jsonWriteString(jw, "id", node->ident->name);
        struct singleNucChange *snc = sncList;
        struct dyString *dy = dyStringCreate("%c%d%c",
                                             snc->parBase, snc->chromStart+1, snc->newBase);
        for (snc = sncList->next;  snc != NULL;  snc = snc->next)
            dyStringPrintf(dy, ",%c%d%c", snc->parBase, snc->chromStart+1, snc->newBase);
        jsonWriteString(jw, "nuc mutations", dy->string);
        dyStringClear(dy);
        for (snc = sncList;  snc != NULL;  snc = snc->next)
            {
            char ref[2];
            seqWindowCopy(gSeqWin, snc->chromStart, 1, ref, sizeof(ref));
            if (snc->newBase == ref[0])
                {
                dyStringAppendSep(dy, ",");
                dyStringPrintf(dy, "%c%d%c", snc->parBase, snc->chromStart+1, snc->newBase);
                }
            }
        jsonWriteString(jw, "back-mutations", dy->string);
        dyStringClear(dy);
        struct dyString *dyS = dyStringNew(0);
        struct slPair *geneAaMut;
        for (geneAaMut = geneAaMutations;  geneAaMut != NULL;  geneAaMut = geneAaMut->next)
            {
            struct slName *aaMut;
            for (aaMut = geneAaMut->val;  aaMut != NULL;  aaMut = aaMut->next)
                {
                dyStringAppendSep(dy, ",");
                dyStringPrintf(dy, "%s:%s", geneAaMut->name, aaMut->name);
                }
            if (sameString("S", geneAaMut->name))
                {
                for (aaMut = geneAaMut->val;  aaMut != NULL;  aaMut = aaMut->next)
                    {
                    dyStringAppendSep(dyS, ",");
                    dyStringPrintf(dyS, "%s:%s", geneAaMut->name, aaMut->name);
                    }
                }
            }
        if (isNotEmpty(dy->string))
            jsonWriteString(jw, "aa mutations", dy->string);
        if (isNotEmpty(dyS->string))
            jsonWriteString(jw, "Spike mutations", dyS->string);
        dyStringFree(&dy);
        dyStringFree(&dyS);
        jsonWriteObjectEnd(jw);
        }
    jsonWriteObjectStart(jw, "mutations");
    struct slPair *geneAaMut;
    for (geneAaMut = geneAaMutations;  geneAaMut != NULL;  geneAaMut = geneAaMut->next)
        {
        jsonWriteListStart(jw, geneAaMut->name);
        struct slName *aaMut;
        for (aaMut = geneAaMut->val;  aaMut != NULL;  aaMut = aaMut->next)
            jsonWriteString(jw, NULL, aaMut->name);
        jsonWriteListEnd(jw);
        }
    jsonWriteListStart(jw, "nuc");
    struct singleNucChange *snc;
    for (snc = sncList;  snc != NULL;  snc = snc->next)
        jsonWriteStringf(jw, NULL, "%c%d%c", snc->parBase, snc->chromStart+1, snc->newBase);
    jsonWriteListEnd(jw);
    jsonWriteObjectEnd(jw);  // mutations
    jsonWriteObjectEnd(jw); // branch_attrs
    }
}

struct auspiceJsonInfo
/* Collection of a bunch of things used when writing out auspice JSON for a subtree, so the
 * recursive function doesn't need a dozen args. */
    {
    struct jsonWrite *jw;
    struct slName *subtreeUserSampleIds;  // Subtree node names for user samples (not from big tree)
    struct geneInfo *geneInfoList;        // Transcript seq & alignment for predicting AA change
    struct seqWindow *gSeqWin;            // Reference genome seq for predicting AA change
    struct hash *sampleMetadata;          // Sample metadata for decorating tree
    struct hash *sampleUrls;              // URLs for samples, if applicable
    struct hash *samplePlacements;        // Sample placement info e.g. clade/lineage from usher
    int nodeNum;                          // For generating sequential node ID (in absence of name)
    char *source;                         // Source of non-user sequences in tree (GISAID or public)
    };

static int cmpstringp(const void *p1, const void *p2)
/* strcmp on pointers to strings, as in 'man qsort' but tolerate NULLs */
{
char *s1 = *(char * const *)p1;
char *s2 = *(char * const *)p2;
if (s1 && s2)
    return strcmp(s1, s2);
else if (s1 && !s2)
    return 1;
else if (s2 && !s1)
    return -1;
return 0;
}

static char *majorityMaybe(char *array[], int arraySize)
/* Sort array and if a majority of its values are the same then return that value; otherwise NULL. */
{
if (arraySize < 1)
    return NULL;
qsort(array, arraySize, sizeof array[0], cmpstringp);
char *maxRunVal = array[0];
int runLength = 1, maxRunLength = 1;
int i;
for (i = 1;  i < arraySize;  i++)
    {
    if (sameOk(array[i], array[i-1]))
        {
        runLength++;
        if (runLength > maxRunLength)
            {
            maxRunLength = runLength;
            maxRunVal = array[i];
            }
        }
    else
        runLength = 1;
    }
return (maxRunLength > (arraySize >> 1)) ? maxRunVal : NULL;
}

static void rTreeToAuspiceJson(struct phyloTree *node, int depth, struct auspiceJsonInfo *aji,
                               struct singleNucChange *ancestorMuts, boolean isRsv,
                               char **retUserOrOld, char **retNClade, char **retGClade,
                               char **retLineage, char **retNLineage,
                               char **retNCladeUsher, char **retLineageUsher)
/* Write Augur/Auspice V2 JSON for tree.  Enclosing object start and end are written by caller. */
{
struct singleNucChange *sncList = node->priv;
if (sncList)
    {
    depth += slCount(sncList);
    }
boolean isUserSample = FALSE;
if (node->ident->name)
    isUserSample = slNameInList(aji->subtreeUserSampleIds, node->ident->name);
char *name = node->ident->name;
struct sampleMetadata *met = name ? metadataForSample(aji->sampleMetadata, name) : NULL;
if (name)
    jsonWriteString(aji->jw, "name", name);
else
    jsonWriteStringf(aji->jw, "name", "NODE%d", aji->nodeNum++);
jsonWriteBranchAttrs(aji->jw, node, ancestorMuts, aji->geneInfoList, aji->gSeqWin);
if (node->numEdges > 0)
    {
    struct singleNucChange *allMuts = ancestorMuts;
    struct singleNucChange *ancLast = slLastEl(ancestorMuts);
    if (ancLast != NULL)
        ancLast->next = sncList;
    else
        allMuts = sncList;
    jsonWriteListStart(aji->jw, "children");
    char *kidUserOrOld[node->numEdges];
    char *kidNClade[node->numEdges];
    char *kidGClade[node->numEdges];
    char *kidLineage[node->numEdges];
    char *kidNCladeUsher[node->numEdges];
    char *kidLineageUsher[node->numEdges];
    char *kidNLineage[node->numEdges];
    // Step through children in reverse order because nextstrain/Auspice draws upside-down. :)
    int i;
    for (i = node->numEdges - 1;  i >= 0;  i--)
        {
        jsonWriteObjectStart(aji->jw, NULL);
        rTreeToAuspiceJson(node->edges[i], depth, aji, allMuts, isRsv,
                           &kidUserOrOld[i], &kidNClade[i], &kidGClade[i], &kidLineage[i],
                           &kidNLineage[i], &kidNCladeUsher[i], &kidLineageUsher[i]);
        jsonWriteObjectEnd(aji->jw);
        }
    jsonWriteListEnd(aji->jw);
    if (retUserOrOld)
        *retUserOrOld = majorityMaybe(kidUserOrOld, node->numEdges);
    if (retNClade)
        *retNClade = majorityMaybe(kidNClade, node->numEdges);
    if (retGClade)
        *retGClade = majorityMaybe(kidGClade, node->numEdges);
    if (retLineage)
        *retLineage = majorityMaybe(kidLineage, node->numEdges);
    if (retNCladeUsher)
        *retNCladeUsher = majorityMaybe(kidNCladeUsher, node->numEdges);
    if (retLineageUsher)
        *retLineageUsher = majorityMaybe(kidLineageUsher, node->numEdges);
    if (retNLineage)
        *retNLineage = majorityMaybe(kidNLineage, node->numEdges);
    if (ancLast)
        ancLast->next = NULL;
    }
jsonWriteObjectStart(aji->jw, "node_attrs");
jsonWriteDouble(aji->jw, "div", depth);
if (node->numEdges == 0)
    jsonWriteLeafNodeAttributes(aji->jw, name, met, isUserSample, aji->source, aji->sampleUrls,
                                aji->samplePlacements, isRsv,
                                retUserOrOld, retNClade, retGClade, retLineage, retNLineage,
                                retNCladeUsher, retLineageUsher);
else if (retUserOrOld && retGClade && retLineage)
    jsonWriteBranchNodeAttributes(aji->jw, isRsv, *retUserOrOld, *retNClade, *retGClade, *retLineage,
                                  *retNLineage, *retNCladeUsher, *retLineageUsher);
jsonWriteObjectEnd(aji->jw);
}

struct phyloTree *phyloTreeNewNode(char *name)
/* Alloc & return a new node with no children. */
{
struct phyloTree *node;
AllocVar(node);
AllocVar(node->ident);
node->ident->name = cloneString(name);
return node;
}

struct geneInfo *getGeneInfoList(char *bigGenePredFile, struct dnaSeq *refGenome)
/* If config.ra has a source of gene annotations, then return the gene list. */
{
struct geneInfo *geneInfoList = NULL;
struct bbiFile *bbi = NULL;
struct errCatch *errCatch = errCatchNew();
if (isNotEmpty(bigGenePredFile))
    {
    if (errCatchStart(errCatch))
        {
        bbi = bigBedFileOpen(bigGenePredFile);
        }
    errCatchEnd(errCatch);
    }
if (bbi)
    {
    struct lm *lm = lmInit(0);
    struct bigBedInterval *bb, *bbList = bigBedIntervalQuery(bbi, refGenome->name, 0,
                                                             refGenome->size, 0, lm);
    for (bb = bbList;  bb != NULL;  bb = bb->next)
        {
        struct genePredExt *gp = genePredFromBigGenePred(refGenome->name, bb);
        int txLen = 0;
        int ex;
        for (ex = 0;  ex < gp->exonCount;  ex++)
            txLen += (gp->exonEnds[ex] - gp->exonStarts[ex]);
        char *seq = needMem(txLen+1);
        int txOffset = 0;
        for (ex = 0;  ex < gp->exonCount;  ex++)
            {
            int exonLen = gp->exonEnds[ex] - gp->exonStarts[ex];
            safencpy(seq+txOffset, txLen+1-txOffset, refGenome->dna+gp->exonStarts[ex], exonLen);
            txOffset += exonLen;
            }
        if (sameString(gp->strand, "-"))
            reverseComplement(seq, txLen);
        struct geneInfo *gi;
        AllocVar(gi);
        gi->psl = genePredToPsl((struct genePred *)gp, refGenome->size, txLen);
        gi->psl->qName = cloneString(gp->name2);
        gi->txSeq = newDnaSeq(seq, txLen, gp->name2);
        AllocVar(gi->cds);
        genePredToCds((struct genePred *)gp, gi->cds);
        int cdsLen = gi->cds->end - gi->cds->start;
        // Skip genes with no CDS (like RNA genes) or obviously incomplete/incorrect CDS.
        if (cdsLen > 0 && (cdsLen % 3) == 0)
            {
            slAddHead(&geneInfoList, gi);
            }
        }
    lmCleanup(&lm);
    bigBedFileClose(&bbi);
    }
slReverse(&geneInfoList);
return geneInfoList;
}

void treeToAuspiceJson(struct subtreeInfo *sti, char *db, struct geneInfo *geneInfoList,
                       struct seqWindow *gSeqWin, struct hash *sampleMetadata,
                       struct hash *sampleUrls, struct hash *samplePlacements,
                       char *jsonFile, char *source)
/* Write JSON for tree in Nextstrain's Augur/Auspice V2 JSON format
 * (https://github.com/nextstrain/augur/blob/master/augur/data/schema-export-v2.json). */
{
struct phyloTree *tree = sti->subtree;
FILE *outF = mustOpen(jsonFile, "w");
struct jsonWrite *jw = jsonWriteNew();
jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, "version", "v2");
//#*** FIXME: TODO: either pass in along with sampleMetadata, or take from JSON file specified
//#*** in config, or better yet, compute while building tree object in memory, then write the
//#*** header object, then write the tree.
boolean isRsv = (stringIn("GCF_000855545", db) || stringIn("GCF_002815475", db) || stringIn("RGCC", db));
struct slName *colorFields = NULL;
if (sameString(db, "wuhCor1"))
    {
    slNameAddHead(&colorFields, "country");
    slNameAddHead(&colorFields, "Nextstrain_clade_usher");
    slNameAddHead(&colorFields, "pango_lineage_usher");
    slNameAddHead(&colorFields, "Nextstrain_clade");
    slNameAddHead(&colorFields, "pango_lineage");
    }
else if (isRsv)
    {
    slNameAddHead(&colorFields, "country");
    slNameAddHead(&colorFields, "GCC_nextclade");
    slNameAddHead(&colorFields, "GCC_usher");
    slNameAddHead(&colorFields, "GCC_assigned_2023-11");
    slNameAddHead(&colorFields, "goya_nextclade");
    slNameAddHead(&colorFields, "goya_usher");
    }
else if (stringIn("GCF_000865085", db) || stringIn("GCF_001343785", db))
    {
    slNameAddHead(&colorFields, "country");
    slNameAddHead(&colorFields, "Nextstrain_clade");
    }
else
    {
    slNameAddHead(&colorFields, "country");
    slNameAddHead(&colorFields, "Nextstrain_lineage");
    }
//#*** END FIXME
writeAuspiceMeta(jw, sti->subtreeUserSampleIds, source, db, colorFields, geneInfoList,
                 gSeqWin->end);
jsonWriteObjectStart(jw, "tree");
int nodeNum = 10000; // Auspice.us starting node number for newick -> json
int depth = 0;

// Add an extra root node because otherwise Auspice won't draw branch from big tree root to subtree
struct phyloTree *root = phyloTreeNewNode("wrapper");
phyloAddEdge(root, tree);
tree = root;
struct auspiceJsonInfo aji = { jw, sti->subtreeUserSampleIds, geneInfoList, gSeqWin,
                               sampleMetadata, sampleUrls, samplePlacements, nodeNum, source };
rTreeToAuspiceJson(tree, depth, &aji, NULL, isRsv, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
jsonWriteObjectEnd(jw); // tree
jsonWriteObjectEnd(jw); // top-level object
fputs(jw->dy->string, outF);
jsonWriteFree(&jw);
carefulClose(&outF);
}

