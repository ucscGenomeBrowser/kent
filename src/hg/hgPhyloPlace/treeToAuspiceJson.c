/* Convert a (sub)tree with condensed nodes to JSON for Nextstrain to display, adding in sample
 * mutations, protein changes and metadata. */

/* Copyright (C) 2020 The Regents of the University of California */

#include "common.h"
#include "hash.h"
#include "hui.h"
#include "jsonWrite.h"
#include "linefile.h"
#include "parsimonyProto.h"
#include "phyloPlace.h"
#include "phyloTree.h"
#include "variantProjector.h"

// Globals
extern char *chrom;
extern int chromSize;

static void writeAuspiceMeta(FILE *outF, struct slName *subtreeUserSampleIds, char *source)
/* Write metadata to configure Auspice display. */
{
fprintf(outF,
        "\"meta\": { "
        "\"title\": \"Subtree with %s", subtreeUserSampleIds->name);
int sampleCount = slCount(subtreeUserSampleIds);
if (sampleCount > 10)
    fprintf(outF, " and %d other uploaded samples", sampleCount - 1);
else
    {
    struct slName *sln;
    for (sln = subtreeUserSampleIds->next;  sln != NULL;  sln = sln->next)
        fprintf(outF, ", %s", sln->name);
    }
fputs("\", "
      "\"panels\": [ \"tree\"] , "
      "\"colorings\": [ "
      "  { \"key\": \"pango_lineage\", "
      "    \"title\": \"Pango lineage\", \"type\": \"categorical\" },"
      "  { \"key\": \"Nextstrain_clade\","
      "    \"scale\": [ "
      "        [ \"20H (Beta, V2)\", \"#571EA2\" ],"
      "        [ \"20I (Alpha, V1)\", \"#4334BF\" ],"
      "        [ \"20J (Gamma, V3)\", \"#3F55CE\" ],"
      "        [ \"21A (Delta)\", \"#4376CD\" ],"
      "        [ \"21I (Delta)\", \"#4C91C0\" ],"
      "        [ \"21J (Delta)\", \"#59A4A9\" ],"
      "        [ \"21C (Epsilon)\", \"#7FB975\" ],"
      "        [ \"21D (Eta)\", \"#97BD5F\" ],"
      "        [ \"21F (Iota)\", \"#C7B944\" ],"
      "        [ \"21G (Lambda)\", \"#D9AD3D\" ],"
      "        [ \"21H (Mu)\", \"#E49838\" ],"
      "        [ \"21K (Omicron)\", \"#DB2823\" ],"
      "        [ \"21L (Omicron)\", \"#DB2823\" ],"
      "        [ \"21M (Omicron)\", \"#DB2823\" ],"
      "        [ \"uploaded sample\", \"#FF0000\" ] ],"
      "    \"title\": \"Nextstrain Clade\", \"type\": \"categorical\" },"
      , outF);
if (sameString(source, "GISAID"))
    fputs("  { \"key\": \"GISAID_clade\","
      "    \"scale\": [ [ \"S\", \"#EC676D\" ], [ \"L\", \"#F79E43\" ], [ \"O\", \"#F9D136\" ],"
      "        [ \"V\", \"#FAEA95\" ], [ \"G\", \"#B6D77A\" ], [ \"GH\", \"#8FD4ED\" ],"
      "        [ \"GR\", \"#A692C3\" ] ],"
      "    \"title\": \"GISAID Clade\", \"type\": \"categorical\" },"
          , outF);
fprintf(outF,
      "  { \"key\": \"pango_lineage_usher\", "
      "    \"title\": \"Pango lineage assigned by UShER\", \"type\": \"categorical\" },"
      "  { \"key\": \"Nextstrain_clade_usher\","
      "    \"scale\": [ "
      "        [ \"20H (Beta, V2)\", \"#571EA2\" ],"
      "        [ \"20I (Alpha, V1)\", \"#4334BF\" ],"
      "        [ \"20J (Gamma, V3)\", \"#3F55CE\" ],"
      "        [ \"20H (Beta,V2)\", \"#571EA2\" ],"
      "        [ \"20I (Alpha,V1)\", \"#4334BF\" ],"
      "        [ \"20J (Gamma,V3)\", \"#3F55CE\" ],"
      "        [ \"21A (Delta)\", \"#4376CD\" ],"
      "        [ \"21I (Delta)\", \"#4C91C0\" ],"
      "        [ \"21J (Delta)\", \"#59A4A9\" ],"
      "        [ \"21C (Epsilon)\", \"#7FB975\" ],"
      "        [ \"21D (Eta)\", \"#97BD5F\" ],"
      "        [ \"21F (Iota)\", \"#C7B944\" ],"
      "        [ \"21G (Lambda)\", \"#D9AD3D\" ],"
      "        [ \"21H (Mu)\", \"#E49838\" ],"
      "        [ \"21K (Omicron)\", \"#DB2823\" ],"
      "        [ \"21L\", \"#DB2823\" ],"
      "        [ \"21L (Omicron)\", \"#DB2823\" ],"
      "        [ \"21M (Omicron)\", \"#DB2823\" ],"
      "        [ \"uploaded sample\", \"#FF0000\" ] ],"
      "    \"title\": \"Nextstrain Clade assigned by UShER\", \"type\": \"categorical\" },"
        "  { \"key\": \"userOrOld\", "
        "    \"scale\": [ [ \"uploaded sample\", \"#CC0000\"] , [ \"%s\", \"#000000\"] ],"
        "    \"title\": \"Sample type\", \"type\": \"categorical\" },"
        "  {\"key\": \"gt\", \"title\": \"Genotype\", \"type\": \"categorical\"},"
        "  {\"key\": \"country\", \"title\": \"Country\", \"type\": \"categorical\"}"
        , source);
fputs("  ] , "
//#*** Filters didn't seem to work... maybe something about the new fetch feature, or do I need to spcify in some other way?
//#***      "\"filters\": [ \"GISAID_clade\", \"region\", \"country\", \"division\", \"author\" ], "
      "\"filters\": [ ], "
      "\"genome_annotations\":"
      "{\"E\":{\"end\":26472,\"start\":26245,\"strand\":\"+\",\"type\":\"CDS\"},"
      " \"M\":{\"end\":27191,\"start\":26523,\"strand\":\"+\",\"type\":\"CDS\"},"
      " \"N\":{\"end\":29533,\"start\":28274,\"strand\":\"+\",\"type\":\"CDS\"},"
      " \"ORF1ab\":{\"end\":21555,\"start\":266,\"strand\":\"+\",\"type\":\"CDS\"},"
//      " \"ORF1a\":{\"end\":13468,\"start\":266,\"strand\":\"+\",\"type\":\"CDS\"},"
//      " \"ORF1b\":{\"end\":21555,\"start\":13468,\"strand\":\"+\",\"type\":\"CDS\"},"
      " \"ORF3a\":{\"end\":26220,\"start\":25393,\"strand\":\"+\",\"type\":\"CDS\"},"
      " \"ORF6\":{\"end\":27387,\"start\":27202,\"strand\":\"+\",\"type\":\"CDS\"},"
      " \"ORF7a\":{\"end\":27759,\"start\":27394,\"strand\":\"+\",\"type\":\"CDS\"},"
      " \"ORF7b\":{\"end\":27887,\"start\":27756,\"strand\":\"+\",\"type\":\"CDS\"},"
      " \"ORF8\":{\"end\":28259,\"start\":27894,\"strand\":\"+\",\"type\":\"CDS\"},"
      " \"ORF9b\":{\"end\":28577,\"start\":28284,\"strand\":\"+\",\"type\":\"CDS\"},"
      " \"S\":{\"end\":25384,\"start\":21563,\"strand\":\"+\",\"type\":\"CDS\"},"
      " \"nuc\":{\"end\":29903,\"start\":1,\"strand\":\"+\",\"type\":\"source\"}},"
      "\"display_defaults\": { "
      "  \"branch_label\": \"none\", "
      "  \"color_by\": \"Nextstrain_clade\" "
      "}, "
      , outF);
fprintf(outF,
        "\"description\": \"Dataset generated by [UShER web interface]"
        "(%shgPhyloPlace) using the "
        "[usher](https://github.com/yatisht/usher/) program.  "
//#*** TODO: describe input from which tree was generated: user sample, version of tree, etc.
        , hLocalHostCgiBinUrl());
fputs("If you have metadata you wish to display, you can now drag on a CSV file and it will be "
      "added into this view, [see here]("NEXTSTRAIN_DRAG_DROP_DOC") "
      "for more info.\"} ,"
      , outF);
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
                                        char **retUserOrOld, char **retNClade, char **retGClade,
                                        char **retLineage,
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
*retNClade = isUserSample ? "uploaded sample" : (met && met->nClade) ? met->nClade : NULL;
if (isNotEmpty(*retNClade))
    jsonWriteObjectValue(jw, "Nextstrain_clade", *retNClade);
*retGClade = isUserSample ? "uploaded sample" : (met && met->gClade) ? met->gClade : NULL;
if (isNotEmpty(*retGClade))
    jsonWriteObjectValue(jw, "GISAID_clade", *retGClade);
*retLineage = isUserSample ? "uploaded sample" :
                             (met && met->lineage) ? met->lineage : NULL;
if (isNotEmpty(*retLineage))
    {
    char lineageUrl[1024];
    makeLineageUrl(*retLineage, lineageUrl, sizeof lineageUrl);
    jsonWriteObjectValueUrl(jw, "pango_lineage", *retLineage, lineageUrl);
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
*retNCladeUsher = isUserSample ? "uploaded sample" :
                                 (met && met->nCladeUsher) ? met->nCladeUsher : NULL;
if (isNotEmpty(*retNCladeUsher))
    jsonWriteObjectValue(jw, "Nextstrain_clade_usher", *retNCladeUsher);
*retLineageUsher = isUserSample ? "uploaded sample" :
                                  (met && met->lineageUsher) ? met->lineageUsher : NULL;
if (isNotEmpty(*retLineageUsher))
    {
    char lineageUrl[1024];
    makeLineageUrl(*retLineageUsher, lineageUrl, sizeof lineageUrl);
    jsonWriteObjectValueUrl(jw, "pango_lineage_usher", *retLineageUsher, lineageUrl);
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

static void jsonWriteBranchNodeAttributes(struct jsonWrite *jw, char *userOrOld,
                                          char *nClade, char *gClade, char *lineage,
                                          char *nCladeUsher, char *lineageUsher)
/* Write elements of node_attrs for a branch. */
{
if (userOrOld)
    jsonWriteObjectValue(jw, "userOrOld", userOrOld);
if (nClade)
    jsonWriteObjectValue(jw, "Nextstrain_clade", nClade);
if (gClade)
    jsonWriteObjectValue(jw, "GISAID_clade", gClade);
if (lineage)
    jsonWriteObjectValue(jw, "pango_lineage", lineage);
if (nCladeUsher)
    jsonWriteObjectValue(jw, "Nextstrain_clade_usher", nCladeUsher);
if (lineageUsher)
    jsonWriteObjectValue(jw, "pango_lineage_usher", lineageUsher);
}

static boolean changesProtein(struct singleNucChange *snc, struct geneInfo *gi,
                              struct seqWindow *gSeqWin,
                              int *retAaStart, char *retOldAa, char *retNewAa)
/* If snc changes the coding sequence of gene, return TRUE and set ret values accordingly
 * (note amino acid values are single-base not strings). */
{
boolean isCodingChange = FALSE;
if (snc->chromStart < gi->psl->tEnd && snc->chromStart >= gi->psl->tStart)
    {
    struct bed3 gBed3 = { NULL, chrom, snc->chromStart, snc->chromStart+1 };
    char gAlt[2];
    safef(gAlt, sizeof(gAlt), "%c", snc->newBase);
    if (!sameString(gi->psl->strand, "+"))
        errAbort("changesProtein: only works for psl->strand='+', but gene '%s' has strand '%s'",
                 gi->psl->qName, gi->psl->strand);
    struct vpTx *vpTx = vpGenomicToTranscript(gSeqWin, &gBed3, gAlt, gi->psl, gi->txSeq);
    if (vpTx->start.region == vpExon)
        {
        int aaStart = vpTx->start.txOffset / 3;
        int codonStart = aaStart * 3;
        char newCodon[4];
        safencpy(newCodon, sizeof newCodon, gi->txSeq->dna + codonStart, 3);
        int codonOffset = vpTx->start.txOffset - codonStart;
        assert(codonOffset < sizeof newCodon);
        newCodon[codonOffset] = snc->newBase;
        char newAa = lookupCodon(newCodon);
        char oldAa;
        if (snc->parBase == snc->refBase)
            oldAa = lookupCodon(gi->txSeq->dna + codonStart);
        else
            {
            char oldCodon[4];
            safencpy(oldCodon, sizeof oldCodon, gi->txSeq->dna + codonStart, 3);
            oldCodon[codonOffset] = snc->parBase;
            oldAa = lookupCodon(oldCodon);
            }
        // Watch out for lookupCodon's null-character return value for stop codon; we want '*'.
        if (newAa == '\0')
            newAa = '*';
        if (oldAa == '\0')
            oldAa = '*';
        if (newAa != oldAa)
            {
            isCodingChange = TRUE;
            *retAaStart = aaStart;
            *retOldAa = oldAa;
            *retNewAa = newAa;
            }
        }
    vpTxFree(&vpTx);
    }
return isCodingChange;
}

struct slPair *getAaMutations(struct singleNucChange *sncList, struct geneInfo *geneInfoList,
                              struct seqWindow *gSeqWin)
/* Given lists of SNVs and genes, return a list of pairs of { gene name, AA change list }. */
{
struct slPair *geneChangeList = NULL;
struct geneInfo *gi;
for (gi = geneInfoList;  gi != NULL;  gi = gi->next)
    {
    struct slName *aaChangeList = NULL;
    struct singleNucChange *snc;
    for (snc = sncList;  snc != NULL;  snc = snc->next)
        {
        if (snc->chromStart < gi->psl->tEnd && snc->chromStart >= gi->psl->tStart)
            {
            int aaStart;
            char oldAa, newAa;
            if (changesProtein(snc, gi, gSeqWin, &aaStart, &oldAa, &newAa))
                {
                char aaChange[32];
                safef(aaChange, sizeof aaChange, "%c%d%c", oldAa, aaStart+1, newAa);
                slNameAddHead(&aaChangeList, aaChange);
                }
            }
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
                                 struct geneInfo *geneInfoList, struct seqWindow *gSeqWin)
/* Write mutations (if any).  If node is not a leaf, write label for mutations at this node. */
{
if (node->priv != NULL)
    {
    struct singleNucChange *sncList = node->priv;
    struct slPair *geneAaMutations = getAaMutations(sncList, geneInfoList, gSeqWin);
    jsonWriteObjectStart(jw, "branch_attrs");
    if (node->numEdges > 0)
        {
        jsonWriteObjectStart(jw, "labels");
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
                               char **retUserOrOld, char **retNClade, char **retGClade,
                               char **retLineage, char **retNCladeUsher, char **retLineageUsher)
/* Write Augur/Auspice V2 JSON for tree.  Enclosing object start and end are written by caller. */
{
if (node->priv)
    {
    struct singleNucChange *sncList = node->priv;
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
jsonWriteBranchAttrs(aji->jw, node, aji->geneInfoList, aji->gSeqWin);
if (node->numEdges > 0)
    {
    jsonWriteListStart(aji->jw, "children");
    char *kidUserOrOld[node->numEdges];
    char *kidNClade[node->numEdges];
    char *kidGClade[node->numEdges];
    char *kidLineage[node->numEdges];
    char *kidNCladeUsher[node->numEdges];
    char *kidLineageUsher[node->numEdges];
    // Step through children in reverse order because nextstrain/Auspice draws upside-down. :)
    int i;
    for (i = node->numEdges - 1;  i >= 0;  i--)
        {
        jsonWriteObjectStart(aji->jw, NULL);
        rTreeToAuspiceJson(node->edges[i], depth, aji,
                           &kidUserOrOld[i], &kidNClade[i], &kidGClade[i], &kidLineage[i],
                           &kidNCladeUsher[i], &kidLineageUsher[i]);
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
    }
jsonWriteObjectStart(aji->jw, "node_attrs");
jsonWriteDouble(aji->jw, "div", depth);
if (node->numEdges == 0)
    jsonWriteLeafNodeAttributes(aji->jw, name, met, isUserSample, aji->source, aji->sampleUrls,
                                retUserOrOld, retNClade, retGClade, retLineage,
                                retNCladeUsher, retLineageUsher);
else if (retUserOrOld && retGClade && retLineage)
    jsonWriteBranchNodeAttributes(aji->jw, *retUserOrOld, *retNClade, *retGClade, *retLineage,
                                  *retNCladeUsher, *retLineageUsher);
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
if (isNotEmpty(bigGenePredFile) && fileExists(bigGenePredFile))
    {
    struct bbiFile *bbi = bigBedFileOpen(bigGenePredFile);
    struct lm *lm = lmInit(0);
    struct bigBedInterval *bb, *bbList = bigBedIntervalQuery(bbi, chrom, 0, chromSize, 0, lm);
    for (bb = bbList;  bb != NULL;  bb = bb->next)
        {
        struct genePredExt *gp = genePredFromBigGenePred(chrom, bb);
        if (gp->strand[0] != '+')
            errAbort("getGeneInfoList: strand must be '+' but got '%s' for gene %s",
                     gp->strand, gp->name);
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
        struct geneInfo *gi;
        AllocVar(gi);
        gi->psl = genePredToPsl((struct genePred *)gp, chromSize, txLen);
        gi->txSeq = newDnaSeq(seq, txLen, gp->name2);
        slAddHead(&geneInfoList, gi);
        }
    lmCleanup(&lm);
    bigBedFileClose(&bbi);
    }
slReverse(&geneInfoList);
return geneInfoList;
}

void treeToAuspiceJson(struct subtreeInfo *sti, char *db, struct geneInfo *geneInfoList,
                       struct seqWindow *gSeqWin, struct hash *sampleMetadata,
                       struct hash *sampleUrls, char *jsonFile, char *source)
/* Write JSON for tree in Nextstrain's Augur/Auspice V2 JSON format
 * (https://github.com/nextstrain/augur/blob/master/augur/data/schema-export-v2.json). */
{
struct phyloTree *tree = sti->subtree;
FILE *outF = mustOpen(jsonFile, "w");
fputs("{ \"version\": \"v2\", ", outF);
writeAuspiceMeta(outF, sti->subtreeUserSampleIds, source);
// The meta part is mostly constant & easier to just write out, but jsonWrite is better for the
// nested tree structure.
struct jsonWrite *jw = jsonWriteNew();
jsonWriteObjectStart(jw, "tree");
int nodeNum = 10000; // Auspice.us starting node number for newick -> json
int depth = 0;

// Add an extra root node because otherwise Auspice won't draw branch from big tree root to subtree
struct phyloTree *root = phyloTreeNewNode("wrapper");
phyloAddEdge(root, tree);
tree = root;
struct auspiceJsonInfo aji = { jw, sti->subtreeUserSampleIds, geneInfoList, gSeqWin,
                               sampleMetadata, sampleUrls, nodeNum, source };
rTreeToAuspiceJson(tree, depth, &aji, NULL, NULL, NULL, NULL, NULL, NULL);
jsonWriteObjectEnd(jw);
fputs(jw->dy->string, outF);
jsonWriteFree(&jw);
fputs("}", outF);
carefulClose(&outF);
}

