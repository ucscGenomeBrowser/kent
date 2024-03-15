/* Write custom tracks: one per subtree and one with user's uploaded samples. */

/* Copyright (C) 2020 The Regents of the University of California */

#include "common.h"
#include "hash.h"
#include "hdb.h"
#include "iupac.h"
#include "parsimonyProto.h"
#include "phyloPlace.h"
#include "phyloTree.h"
#include "trashDir.h"
#include "vcf.h"

static char ***imputedBasesByPosition(struct hash *samplePlacements, struct slName *sampleIds,
                                      int chromSize)
/* Unpack imputedBases into an array of (a few) arrays indexed by 0-based position and sample index,
 * for speedy retrieval while rewriting user VCF. */
{
char ***impsByPos = needMem(chromSize * sizeof(char **));
int sampleCount = slCount(sampleIds);
struct slName *sample;
int ix;
for (ix = 0, sample = sampleIds;  sample != NULL; ix++, sample = sample->next)
    {
    struct placementInfo *info = hashFindVal(samplePlacements, sample->name);
    if (!info)
        errAbort("imputedBasesByPosition: can't find placementInfo for sample '%s'", sample->name);
    struct baseVal *bv;
    for (bv = info->imputedBases;  bv != NULL;  bv = bv->next)
        {
        if (!impsByPos[bv->chromStart])
            impsByPos[bv->chromStart] = needMem(sampleCount * sizeof(char *));
        impsByPos[bv->chromStart][ix] = bv->val;
        }
    }
return impsByPos;
}

static boolean anyImputedBases(struct hash *samplePlacements, struct slName *sampleIds)
/* Return TRUE if any sample has at least one imputed base. */
{
struct slName *sample;
for (sample = sampleIds;  sample != NULL;  sample = sample->next)
    {
    struct placementInfo *info = hashFindVal(samplePlacements, sample->name);
    if (info && info->imputedBases)
        return TRUE;
    }
return FALSE;
}

struct tempName *userVcfWithImputedBases(struct tempName *userVcfTn, struct hash *samplePlacements,
                                         struct slName *sampleIds, int chromSize, int *pStartTime)
/* If usher imputed the values of ambiguous bases in user's VCF, then swap in
 * the imputed values and write a new VCF to use as the custom track.  Otherwise just use
 * the user's VCF for the custom track. */
{
struct tempName *ctVcfTn = userVcfTn;
if (anyImputedBases(samplePlacements, sampleIds))
    {
    AllocVar(ctVcfTn);
    trashDirFile(ctVcfTn, "ct", "imputed", ".vcf");
    FILE *f = mustOpen(ctVcfTn->forCgi, "w");
    struct lineFile *lf = lineFileOpen(userVcfTn->forCgi, TRUE);
    int sampleCount = slCount(sampleIds);
    int colCount = VCF_NUM_COLS_BEFORE_GENOTYPES + sampleCount;
    char ***impsByPos = imputedBasesByPosition(samplePlacements, sampleIds, chromSize);
    char *line;
    int size;
    while (lineFileNext(lf, &line, &size))
        {
        if (line[0] == '#')
            fputs(line, f);
        else
            {
            char *words[colCount];
            int wordCount = chopTabs(line, words);
            lineFileExpectWords(lf, colCount, wordCount);
            int chromStart = atoi(words[1]) - 1;
            if (impsByPos[chromStart])
                {
                verbose(3, "%d ref=%s alt=%s\n", chromStart+1, words[3], words[4]);
                int gtIx;
                for (gtIx = 0;  gtIx < sampleCount;  gtIx++)
                    if (impsByPos[chromStart][gtIx])
                        {
                        char *val = impsByPos[chromStart][gtIx];
                        // We can't just swap the value in; we need the numerical index.
                        // Is the imputed value ref, alt, or member of list of alts?
                        if (sameString(val, words[3]))
                            {
                            words[VCF_NUM_COLS_BEFORE_GENOTYPES+gtIx] = "0";
                            verbose(3, "gtIx %d: val matches ref (%s)\n", gtIx, val);
                            }
                        else if (sameString(val, words[4]))
                            {
                            words[VCF_NUM_COLS_BEFORE_GENOTYPES+gtIx] = "1";
                            verbose(3, "gtIx %d: val matches alt (%s)\n", gtIx, val);
                            }
                        else
                            {
                            // There may be multiple alts.  The alt itself might be ambiguous
                            // and require replacement.
                            boolean foundIt = FALSE;
                            struct slName *alt, *alts = slNameListFromComma(words[4]);
                            int colIx = VCF_NUM_COLS_BEFORE_GENOTYPES + gtIx;
                            int altIx;
                            for (altIx = 0, alt = alts;  alt != NULL;  altIx++, alt = alt->next)
                                {
                                if (sameString(val, alt->name))
                                    {
                                    // val is in the list of alts; substitute in its index.
                                    foundIt = TRUE;
                                    int len = strlen(words[colIx]);
                                    safef(words[colIx], len+1, "%d", altIx+1);
                                    verbose(3, "gtIx %d: val matches alts[%d] (%s)\n",
                                            gtIx, altIx, val);
                                    break;
                                    }
                                }
                            if (!foundIt)
                                {
                                for (altIx = 0, alt = alts;  alt != NULL;  altIx++, alt = alt->next)
                                    {
                                    if (alt->name[1] == '\0' && val[1] == '\0' &&
                                        isIupacAmbiguous(alt->name[0]))
                                        {
                                        char lowerV = tolower(val[0]);
                                        if (strchr(iupacAmbiguousToString(alt->name[0]), lowerV))
                                            {
                                            verbose(3,
                                                  "gtIx %d: val (%s) matches ambig alts[%d] (%s)\n",
                                                    gtIx, val, altIx, alt->name);
                                            // The alt is an ambiguous character that includes val.
                                            // Replace this alt with val.
                                            foundIt = TRUE;
                                            alt->name[0] = val[0];
                                            // Update the alts column
                                            words[4] = slNameListToString(alts, ',');
                                            int len = strlen(words[colIx]);
                                            safef(words[colIx], len+1, "%d", altIx+1);
                                            verbose(3, "gtIx %d: swapped in %s\n",
                                                    gtIx, words[colIx]);
                                            break;
                                            }
                                        }
                                    }
                                if (!foundIt)
                                    {
                                    // Add the imputed value as a new alt.
                                    int altCount = slCount(alts);
                                    verbose(3, "gtIx %d: adding val as new alts[%d] = %s\n",
                                            gtIx, altCount, val);
                                    int len = strlen(words[colIx]);
                                    safef(words[colIx], len+1, "%d", altCount+1);
                                    slAddTail(alts, slNameNew(val));
                                    words[4] = slNameListToString(alts, ',');
                                    verbose(3, "gtIx %d: swapped in %s\n", gtIx, words[colIx]);
                                    }
                                }
                            }
                        }
                }
            int i;
            fputs(words[0], f);
            for (i = 1;  i < colCount;  i++)
                {
                fputc('\t', f);
                fputs(words[i], f);
                }
            }
            fputc('\n', f);
        }
    carefulClose(&f);
    reportTiming(pStartTime, "Make user VCF with imputed bases");
    }
return ctVcfTn;
}

static int heightForSampleCount(int fontHeight, int count)
/* Return a height sufficient for this many labels stacked vertically. */
{
return (fontHeight + 1) * count;
}

static void addSampleOnlyCustomTrack(FILE *ctF, struct tempName *vcfTn,
                                     struct slName *sampleIds, struct phyloTree *sampleTree,
                                     char *geneTrack, int fontHeight, int *pStartTime)
/* Make custom track with uploaded VCF.  If there are enough samples to make a non-trivial tree,
 * then make the tree by pruning down the big tree plus samples to only the uploaded samples. */
{
fprintf(ctF, "track name=uploadedSamples description='Uploaded sample genotypes' "
        "type=vcf visibility=pack "
        "hapClusterEnabled=on hapClusterHeight=%d ",
        heightForSampleCount(fontHeight, slCount(sampleIds)));
if (sampleTree != NULL)
    {
    // Write tree of uploaded samples to a file so we can draw it in left label area.
    struct tempName sampleTreeTn;
    trashDirFile(&sampleTreeTn, "ct", "uploadedSamples", ".nwk");
    FILE *f = mustOpen(sampleTreeTn.forCgi, "w");
    phyloPrintTree(sampleTree, f);
    carefulClose(&f);
    fprintf(ctF, "hapClusterMethod='treeFile %s' ", sampleTreeTn.forCgi);
    }
else
    fprintf(ctF, "hapClusterMethod=fileOrder ");
if (isNotEmpty(geneTrack))
    fprintf(ctF, "hapClusterColorBy=function geneTrack=%s", geneTrack);
fputc('\n', ctF);
struct lineFile *vcfLf = lineFileOpen(vcfTn->forCgi, TRUE);
char *line;
while (lineFileNext(vcfLf, &line, NULL))
    fprintf(ctF, "%s\n",line);
lineFileClose(&vcfLf);
reportTiming(pStartTime, "add custom track for uploaded samples");
}

static struct vcfFile *parseUserVcf(char *userVcfFile, int chromSize, int *pStartTime)
/* Read in user VCF file and parse genotypes. */
{
struct vcfFile *userVcf = vcfFileMayOpen(userVcfFile, NULL, 0, chromSize, 0, chromSize, TRUE);
if (! userVcf)
    return NULL;
struct vcfRecord *rec;
for (rec = userVcf->records;  rec != NULL;  rec = rec->next)
    vcfParseGenotypesGtOnly(rec);
reportTiming(pStartTime, "read userVcf and parse genotypes");
return userVcf;
}

static void writeSubtreeTrackLine(FILE *ctF, struct subtreeInfo *ti, int subtreeNum, char *source,
                                  char *geneTrack, int fontHeight)
/* Write a custom track line to ctF for one subtree. */
{
fprintf(ctF, "track name=subtree%d", subtreeNum);
// Keep the description from being too long in order to avoid buffer overflow in hgTrackUi
// (and limited space for track title in hgTracks)
int descLen = fprintf(ctF, " description='Subtree %d: uploaded sample%s %s",
                      subtreeNum, (slCount(ti->subtreeUserSampleIds) > 1 ? "s" : ""),
                      ti->subtreeUserSampleIds->name);
struct slName *id;
for (id = ti->subtreeUserSampleIds->next;  id != NULL;  id = id->next)
    {
    if (descLen > 100)
        {
        fprintf(ctF, " and %d other samples", slCount(id));
        break;
        }
    descLen += fprintf(ctF, ", %s", id->name);
    }
int height = heightForSampleCount(fontHeight, slCount(ti->subtreeNameList));
fprintf(ctF, " and nearest neighboring %s sequences' type=vcf visibility=dense "
        "hapClusterEnabled=on hapClusterHeight=%d hapClusterMethod='treeFile %s' "
        "highlightIds=%s",
        source, height, ti->subtreeTn->forHtml,
        slNameListToString(ti->subtreeUserSampleIds, ','));
if (isNotEmpty(geneTrack))
    fprintf(ctF, " hapClusterColorBy=function geneTrack=%s", geneTrack);
fputc('\n', ctF);
}

static void writeVcfHeader(FILE *f, struct slName *sampleNames)
/* Write a minimal VCF header with sample names for genotype columns. */
{
fprintf(f, "##fileformat=VCFv4.2\n");
fprintf(f, "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\tFORMAT");
struct slName *s;
for (s = sampleNames;  s != NULL;  s = s->next)
    fprintf(f, "\t%s", s->name);
fputc('\n', f);
}

static struct slRef *slRefListCopyReverse(struct slRef *inList)
/* Return a clone of inList but in reverse order. */
{
struct slRef *outList = NULL;
struct slRef *ref;
for (ref = inList;  ref != NULL;  ref = ref->next)
    slAddHead(&outList, slRefNew(ref->val));
return outList;
}

static void treeToBaseAlleles(struct phyloTree *node, char *refBases,
                              char **sampleBases, struct hash *sampleToIx, struct slRef *parentMuts)
/* Traverse tree, building up an array of reference bases indexed by position, and an array
 * (indexed by pos) of sample allele arrays indexed by sampleToIx. */
{
struct slRef *nodeMuts = parentMuts;
if (node->priv != NULL)
    {
    // Start out with a copy of parent node mutations in reverse order so we can add at head
    // and then reverse all.
    struct singleNucChange *sncs = node->priv;
    nodeMuts = slRefListCopyReverse(parentMuts);
    slAddHead(&nodeMuts, slRefNew(sncs));
    slReverse(&nodeMuts);
    }
if (node->numEdges > 0)
    {
    int i;
    for (i = 0;  i < node->numEdges;  i++)
        treeToBaseAlleles(node->edges[i], refBases, sampleBases, sampleToIx, nodeMuts);
    }
else
    {
    // Leaf node: if in sampleToIx, then for each mutation (beware back-mutations),
    // set refBase if it has not already been set and set sample allele.
    int ix = hashIntValDefault(sampleToIx, node->ident->name, -1);
    if (ix >= 0)
        {
        struct slRef *mlRef;
        for (mlRef = nodeMuts;  mlRef != NULL;  mlRef = mlRef->next)
            {
            struct singleNucChange *snc, *sncs = mlRef->val;
            for (snc = sncs;  snc != NULL;  snc = snc->next)
                {
                // Don't let a back-mutation overwrite the true ref value:
                if (!refBases[snc->chromStart])
                    refBases[snc->chromStart] = snc->refBase;
                if (sampleBases[snc->chromStart] == NULL)
                    AllocArray(sampleBases[snc->chromStart], sampleToIx->elCount);
                sampleBases[snc->chromStart][ix] = snc->newBase;
                }
            }
        }
    }
if (node->priv != NULL)
    slFreeList(&nodeMuts);
}

static void vcfToBaseAlleles(struct vcfFile *vcff, char *refBases, char **sampleBases,
                             struct hash *sampleToIx)
/* Build up an array of reference bases indexed by position, and an array (indexed by pos)
 * of sample allele arrays indexed by sampleToIx, from variants & genotypes in vcff. */
{
int gtIxToSampleIx[vcff->genotypeCount];
int gtIx;
for (gtIx = 0;  gtIx < vcff->genotypeCount;  gtIx++)
    {
    int ix = hashIntValDefault(sampleToIx, vcff->genotypeIds[gtIx], -1);
    gtIxToSampleIx[gtIx] = ix;
    }
struct vcfRecord *rec;
for (rec = vcff->records;  rec != NULL;  rec = rec->next)
    {
    for (gtIx = 0;  gtIx < vcff->genotypeCount;  gtIx++)
        {
        int ix = gtIxToSampleIx[gtIx];
        if (ix >= 0)
            {
            // If refBases was already set by tree, go with that.
            if (!refBases[rec->chromStart])
                refBases[rec->chromStart] = rec->alleles[0][0];
            if (sampleBases[rec->chromStart] == NULL)
                AllocArray(sampleBases[rec->chromStart], sampleToIx->elCount);
            int alIx = rec->genotypes[gtIx].hapIxA;
            if (alIx < 0)
                sampleBases[rec->chromStart][ix] = '.';
            else if (alIx > 0)
                sampleBases[rec->chromStart][ix] = rec->alleles[alIx][0];
            // If alIx == 0 (ref), leave the sample base at '\0', the default assumption.
            }
        }
    }
}

static int tallyAlts(char *sampleAlleles, char ref, int sampleCount, char *altAlleles,
                     int *altCounts, int maxAltCount, int *retNoCallCount)
/* Scan through sampleAlleles looking for alternate alleles.  Return the number of alternate
 * alleles and fill in altAlleles and altCounts (number of observations of each alt allele).
 * Also simplify sampleAlleles by replacing ref alleles (from back-mutations) with '\0'. */
{
int altCount = 0;
int noCallCount = 0;
memset(altAlleles, 0, sizeof(*altAlleles) * maxAltCount);
memset(altCounts, 0, sizeof(*altCounts) * maxAltCount);
int ix;
for (ix = 0;  ix < sampleCount;  ix++)
    {
    if (sampleAlleles[ix] == ref)
        sampleAlleles[ix] = '\0';
    char sampleAl = sampleAlleles[ix];
    if (sampleAl == '.')
        noCallCount++;
    else if (sampleAl != '\0')
        {
        int altIx;
        for (altIx = 0;  altIx < altCount;  altIx++)
            if (altAlleles[altIx] == sampleAl)
                break;
        if (altIx == altCount)
            {
            if (altCount == maxAltCount)
                errAbort("Too many alternate alleles for maxAltCount=%d; increase it.",
                         maxAltCount);
            // Add new alt allele.
            altCount++;
            altAlleles[altIx] = sampleAl;
            }
        altCounts[altIx]++;
        }
    }
*retNoCallCount = noCallCount;
return altCount;
}

static void altsToGenotypes(char *sampleAlleles, int sampleCount, char *altAlleles, int altCount,
                            char *genotypes)
/* Fill in genotypes with '0' for ref allele, '1' for first alternate allele, etc. */
{
int ix;
for (ix = 0;  ix < sampleCount;  ix++)
    {
    char sampleAl = sampleAlleles[ix];
    if (sampleAl == '\0')
        genotypes[ix] = '0';
    else if (sampleAl == '.')
        genotypes[ix] = '.';
    else
        {
        int altIx;
        for (altIx = 0;  altIx < altCount;  altIx++)
            if (altAlleles[altIx] == sampleAl)
                break;
        if (altIx == altCount)
            errAbort("altsToGenotypes: sample %d allele '%c' not found in array of %d alts.",
                     ix, sampleAl, altCount);
        genotypes[ix] = '1' + altIx;
        }
    }
}

static void baseAllelesToVcf(char *refBases, char **sampleBases, char *chrom, int baseCount,
                             int sampleCount, FILE *f)
/* Write VCF rows with per-sample genotypes from refBases and sampleBases. */
{
int chromStart;
for (chromStart = 0;  chromStart < baseCount;  chromStart++)
    {
    char ref = refBases[chromStart];
    if (ref != '\0')
        {
        // This position has a variant.  Tally up alternate alleles and counts and determine
        // allele indices to use when printing out genotypes.
        int maxAlts = 16;
        char altAlleles[maxAlts];
        int altCounts[maxAlts];
        int noCallCount;
        int altCount = tallyAlts(sampleBases[chromStart], ref, sampleCount,
                                 altAlleles, altCounts, maxAlts, &noCallCount);
        if (altCount == 0)
            continue;
        char genotypes[sampleCount];
        altsToGenotypes(sampleBases[chromStart], sampleCount, altAlleles, altCount, genotypes);
        int pos = chromStart+1;
        fprintf(f, "%s\t%d\t%c%d%c",
                chrom, pos, ref, pos, altAlleles[0]);
        int altIx;
        for (altIx = 1;  altIx < altCount;  altIx++)
            fprintf(f, ",%c%d%c", ref, pos, altAlleles[altIx]);
        fprintf(f, "\t%c\t%c", ref, altAlleles[0]);
        for (altIx = 1;  altIx < altCount;  altIx++)
        fprintf(f, ",%c", toupper(altAlleles[altIx]));
        fputs("\t.\t.\t", f);
        fprintf(f, "AC=%d", altCounts[0]);
        for (altIx = 1;  altIx < altCount;  altIx++)
            fprintf(f, ",%d", altCounts[altIx]);
        fprintf(f, ";AN=%d\tGT", sampleCount - noCallCount);
        int gtIx;
        for (gtIx = 0;  gtIx < sampleCount;  gtIx++)
            fprintf(f, "\t%c", genotypes[gtIx]);
        fputc('\n', f);
        }
    }
}

static void writeSubtreeVcf(FILE *f, struct hash *sampleToIx, char *chrom, int chromSize,
                            struct vcfFile *userVcf, struct phyloTree *subtree)
/* Write rows of VCF with genotypes for samples in sampleToIx (with order determined by sampleToIx)
 * with some samples found in userVcf and the rest found in subtree which has been annotated
 * with single-nucleotide variants. */
{
char *refBases = NULL;
AllocArray(refBases, chromSize);
char **sampleBases = NULL;
AllocArray(sampleBases, chromSize);
treeToBaseAlleles(subtree, refBases, sampleBases, sampleToIx, NULL);
vcfToBaseAlleles(userVcf, refBases, sampleBases, sampleToIx);
int sampleCount = sampleToIx->elCount;
baseAllelesToVcf(refBases, sampleBases, chrom, chromSize, sampleCount, f);
int i;
for (i = 0;  i < chromSize;  i++)
    freeMem(sampleBases[i]);
freeMem(sampleBases);
freeMem(refBases);
}

static void addSubtreeCustomTracks(FILE *ctF, char *userVcfFile, struct subtreeInfo *subtreeInfoList,
                                   struct hash *samplePlacements, char *chrom, int chromSize,
                                   char *source, char *geneTrack, int fontHeight, int *pStartTime)
/* For each subtree trashfile, write VCF+treeFile custom track text to ctF. */
{
struct vcfFile *userVcf = parseUserVcf(userVcfFile, chromSize, pStartTime);
if (! userVcf)
    {
    warn("Problem parsing VCF file with user variants '%s'; can't make subtree subtracks.",
         userVcfFile);
    return;
    }
struct subtreeInfo *ti;
int subtreeNum;
for (subtreeNum = 1, ti = subtreeInfoList;  ti != NULL;  ti = ti->next, subtreeNum++)
    {
    writeSubtreeTrackLine(ctF, ti, subtreeNum, source, geneTrack, fontHeight);
    writeVcfHeader(ctF, ti->subtreeNameList);
    writeSubtreeVcf(ctF, ti->subtreeIdToIx, chrom, chromSize, userVcf, ti->subtree);
    fputc('\n', ctF);
    }
vcfFileFree(&userVcf);
reportTiming(pStartTime, "write subtree custom tracks");
}

struct tempName *writeCustomTracks(char *db, struct tempName *vcfTn, struct usherResults *ur,
                                   struct slName *sampleIds, char *source, int fontHeight,
                                   struct phyloTree *sampleTree, int *pStartTime)
/* Write one custom track per subtree, and one custom track with just the user's uploaded samples. */
{
char *chrom = hDefaultChrom(db);
int chromSize = hChromSize(db, chrom);
char *geneTrack = phyloPlaceDbSetting(db, "geneTrack");
// Default to SARS-CoV-2 or hMPXV values if setting is missing from config.ra.
if (isEmpty(geneTrack))
    geneTrack = sameString(db, "wuhCor1") ? "ncbiGeneBGP" : "ncbiGene";
struct tempName *ctVcfTn = userVcfWithImputedBases(vcfTn, ur->samplePlacements, sampleIds,
                                                   chromSize, pStartTime);
struct tempName *ctTn;
AllocVar(ctTn);
trashDirFile(ctTn, "ct", "ct_pp", ".ct");
FILE *ctF = mustOpen(ctTn->forCgi, "w");
int subtreeCount = slCount(ur->subtreeInfoList);
if (subtreeCount <= MAX_SUBTREE_CTS)
    addSubtreeCustomTracks(ctF, ctVcfTn->forCgi, ur->subtreeInfoList, ur->samplePlacements,
                           chrom, chromSize, source, geneTrack, fontHeight, pStartTime);
else
    printf("<p>Subtree custom tracks are added when there are at most %d subtrees, "
           "but %d subtrees were found.</p>\n",
           MAX_SUBTREE_CTS, subtreeCount);
addSampleOnlyCustomTrack(ctF, ctVcfTn, sampleIds, sampleTree, geneTrack, fontHeight, pStartTime);
carefulClose(&ctF);
return ctTn;
}
