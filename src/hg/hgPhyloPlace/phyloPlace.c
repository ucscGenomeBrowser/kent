/* Place SARS-CoV-2 sequences in phylogenetic tree using usher program. */

/* Copyright (C) 2020 The Regents of the University of California */

#include "common.h"
#include "bigBed.h"
#include "cheapcgi.h"
#include "errCatch.h"
#include "fa.h"
#include "genePred.h"
#include "hCommon.h"
#include "hash.h"
#include "hgConfig.h"
#include "htmshell.h"
#include "hui.h"
#include "iupac.h"
#include "jsHelper.h"
#include "linefile.h"
#include "obscure.h"
#include "parsimonyProto.h"
#include "phyloPlace.h"
#include "phyloTree.h"
#include "pipeline.h"
#include "psl.h"
#include "ra.h"
#include "regexHelper.h"
#include "trashDir.h"
#include "vcf.h"

// Globals:
static boolean measureTiming = FALSE;

// wuhCor1-specific:
char *chrom = "NC_045512v2";
int chromSize = 29903;

// Parameter constants:
int maxGenotypes = 100;        // Upper limit on number of samples user can upload at once.
boolean showBestNodePaths = FALSE;
boolean showParsimonyScore = FALSE;


char *phyloPlaceDbSetting(char *db, char *settingName)
/* Return a setting from hgPhyloPlaceData/<db>/config.ra or NULL if not found. */
{
static struct hash *configHash = NULL;
static char *configDb = NULL;
if (!sameOk(db, configDb))
    {
    char configFile[1024];
    safef(configFile, sizeof configFile, PHYLOPLACE_DATA_DIR "/%s/config.ra", db);
    if (fileExists(configFile))
        {
        configHash = raReadSingle(configFile);
        configDb = cloneString(db);
        }
    }
if (sameOk(db, configDb))
    return cloneString(hashFindVal(configHash, settingName));
return NULL;
}

char *phyloPlaceDbSettingPath(char *db, char *settingName)
/* Return path to a file named by a setting from hgPhyloPlaceData/<db>/config.ra,
 * or NULL if not found.  (Append hgPhyloPlaceData/<db>/ to the beginning of relative path) */
{
char *fileName = phyloPlaceDbSetting(db, settingName);
if (isNotEmpty(fileName) && fileName[0] != '/' && !fileExists(fileName))
    {
    struct dyString *dy = dyStringCreate(PHYLOPLACE_DATA_DIR "/%s/%s", db, fileName);
    if (fileExists(dy->string))
        return dyStringCannibalize(&dy);
    else
        return NULL;
    }
return fileName;
}

char *getUsherPath(boolean abortIfNotFound)
/* Return hgPhyloPlaceData/usher if it exists, else NULL.  Do not free the returned value. */
{
char *usherPath = PHYLOPLACE_DATA_DIR "/usher";
if (fileExists(usherPath))
    return usherPath;
else if (abortIfNotFound)
    errAbort("Missing required file %s", usherPath);
return NULL;
}

char *getUsherAssignmentsPath(char *db, boolean abortIfNotFound)
/* If <db>/config.ra specifies the file for use by usher --load-assignments and the file exists,
 * return the path, else NULL.  Do not free the returned value. */
{
char *usherAssignmentsPath = phyloPlaceDbSettingPath(db, "usherAssignmentsFile");
if (isNotEmpty(usherAssignmentsPath) && fileExists(usherAssignmentsPath))
    return usherAssignmentsPath;
else if (abortIfNotFound)
    errAbort("Missing required file %s", usherAssignmentsPath);
return NULL;
}

//#*** This needs to go in a lib so CGIs know whether to include it in the menu. needs better name.
boolean hgPhyloPlaceEnabled()
/* Return TRUE if hgPhyloPlace is enabled in hg.conf and db wuhCor1 exists. */
{
char *cfgSetting = cfgOption("hgPhyloPlaceEnabled");
boolean isEnabled = (isNotEmpty(cfgSetting) &&
                     differentWord(cfgSetting, "off") && differentWord(cfgSetting, "no"));
return (isEnabled && hDbExists("wuhCor1"));
}

static void addPathIfNecessary(struct dyString *dy, char *db, char *fileName)
/* If fileName exists, copy it into dy, else try hgPhyloPlaceData/<db>/fileName */
{
dyStringClear(dy);
if (fileExists(fileName))
    dyStringAppend(dy, fileName);
else
    dyStringPrintf(dy, PHYLOPLACE_DATA_DIR "/%s/%s", db, fileName);
}

struct treeChoices *loadTreeChoices(char *db)
/* If <db>/config.ra specifies a treeChoices file, load it up, else return NULL. */
{
struct treeChoices *treeChoices = NULL;
char *filename = phyloPlaceDbSettingPath(db, "treeChoices");
if (isNotEmpty(filename) && fileExists(filename))
    {
    AllocVar(treeChoices);
    int maxChoices = 128;
    AllocArray(treeChoices->protobufFiles, maxChoices);
    AllocArray(treeChoices->metadataFiles, maxChoices);
    AllocArray(treeChoices->sources, maxChoices);
    AllocArray(treeChoices->descriptions, maxChoices);
    struct lineFile *lf = lineFileOpen(filename, TRUE);
    char *line;
    while (lineFileNextReal(lf, &line))
        {
        char *words[5];
        int wordCount = chopTabs(line, words);
        lineFileExpectWords(lf, 4, wordCount);
        if (treeChoices->count >= maxChoices)
            {
            warn("File %s has too many lines, only showing first %d phylogenetic tree choices",
                 filename, maxChoices);
            break;
            }
        struct dyString *dy = dyStringNew(0);
        addPathIfNecessary(dy, db, words[0]);
        treeChoices->protobufFiles[treeChoices->count] = cloneString(dy->string);
        addPathIfNecessary(dy, db, words[1]);
        treeChoices->metadataFiles[treeChoices->count] = cloneString(dy->string);
        treeChoices->sources[treeChoices->count] = cloneString(words[2]);
        // Description can be either a file or just some text.
        addPathIfNecessary(dy, db, words[3]);
        if (fileExists(dy->string))
            {
            char *desc = NULL;
            readInGulp(dy->string, &desc, NULL);
            treeChoices->descriptions[treeChoices->count] = desc;
            }
        else
            treeChoices->descriptions[treeChoices->count] = cloneString(words[3]);
        treeChoices->count++;
        dyStringFree(&dy);
        }
    lineFileClose(&lf);
    }
return treeChoices;
}

static char *urlFromTn(struct tempName *tn)
/* Make a full URL to a trash file that our net.c code will be able to follow, for when we can't
 * just leave it up to the user's web browser to do the right thing with "../". */
{
struct dyString *dy = dyStringCreate("%s%s", hLocalHostCgiBinUrl(), tn->forHtml);
return dyStringCannibalize(&dy);
}

void reportTiming(int *pStartTime, char *message)
/* Print out a report to stderr of how much time something took. */
{
if (measureTiming)
    {
    int now = clock1000();
    fprintf(stderr, "%dms to %s\n", now - *pStartTime, message);
    *pStartTime = now;
    }
}

static boolean lfLooksLikeFasta(struct lineFile *lf)
/* Peek at file to see if it looks like FASTA, i.e. begins with a >header. */
{
boolean hasFastaHeader = FALSE;
char *line;
if (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '>')
        hasFastaHeader = TRUE;
    lineFileReuse(lf);
    }
return hasFastaHeader;
}

static void rInformativeBasesFromTree(struct phyloTree *node, boolean *informativeBases)
/* For each variant associated with a non-leaf node, set informativeBases[chromStart]. */
{
if (node->numEdges > 0)
    {
    if (node->priv)
        {
        struct singleNucChange *snc, *sncs = node->priv;
        for (snc = sncs;  snc != NULL;  snc = snc->next)
            informativeBases[snc->chromStart] = TRUE;
        }
    int i;
    for (i = 0;  i < node->numEdges;  i++)
        rInformativeBasesFromTree(node->edges[i], informativeBases);
    }
}

static boolean *informativeBasesFromTree(struct phyloTree *bigTree, struct slName **maskSites)
/* Return an array indexed by reference position with TRUE at positions that have a non-leaf
 * variant in bigTree. */
{
boolean *informativeBases;
AllocArray(informativeBases, chromSize);
if (bigTree)
    {
    rInformativeBasesFromTree(bigTree, informativeBases);
    int i;
    for (i = 0;  i < chromSize;  i++)
        {
        struct slName *maskedReasons = maskSites[i];
        if (maskedReasons && informativeBases[i])
            {
            warn("protobuf tree contains masked mutation at %d (%s)", i+1, maskedReasons->name);
            informativeBases[i] = FALSE;
            }
        }
    }
return informativeBases;
}

static boolean lfLooksLikeVcf(struct lineFile *lf)
/* Peek at file to see if it looks like VCF, i.e. begins with a ##fileformat=VCF header. */
{
boolean hasVcfHeader = FALSE;
char *line;
if (lineFileNext(lf, &line, NULL))
    {
    if (startsWith("##fileformat=VCF", line))
        hasVcfHeader = TRUE;
    lineFileReuse(lf);
    }
return hasVcfHeader;
}

static struct tempName *checkAndSaveVcf(struct lineFile *lf, struct dnaSeq *refGenome,
                                        struct slName **maskSites, struct seqInfo **retSeqInfoList,
                                        struct slName **retSampleIds)
/* Save the contents of lf to a trash file.  If it has a reasonable number of genotype columns
 * with recognizable genotypes, and the coordinates seem to be in range, then return the path
 * to the trash file.  Otherwise complain and return NULL. */
{
struct tempName *tn;
AllocVar(tn);
trashDirFile(tn, "ct", "ct_pp", ".vcf");
FILE *f = mustOpen(tn->forCgi, "w");
struct seqInfo *seqInfoList = NULL;
struct slName *sampleIds = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    char *line;
    int lineSize;
    int sampleCount = 0;
    while (lineFileNext(lf, &line, &lineSize))
        {
        if (startsWith("#CHROM\t", line))
            {
            //#*** TODO: if the user uploads a sample with the same ID as one already in the
            //#*** saved assignment file, then usher will ignore it!
            //#*** Better check for that and warn the user.
            int colCount = chopTabs(line, NULL);
            if (colCount == 1)
                {
                lineFileAbort(lf, "VCF requires tab-separated columns, but no tabs found");
                }
            sampleCount = colCount - VCF_NUM_COLS_BEFORE_GENOTYPES;
            if (sampleCount < 1 || sampleCount > maxGenotypes)
                {
                if (sampleCount < 1)
                    lineFileAbort(lf, "VCF header #CHROM line has %d columns; expecting at least %d "
                                  "columns including sample IDs for genotype columns",
                                  colCount, 10);
                else
                    lineFileAbort(lf, "VCF header #CHROM line defines %d samples but only up to %d "
                                  "are supported",
                                  sampleCount, maxGenotypes);
                }
            char lineCopy[lineSize+1];
            safecpy(lineCopy, sizeof lineCopy, line);
            char *words[colCount];
            chopTabs(lineCopy, words);
            struct hash *uniqNames = hashNew(0);
            int i;
            for (i = VCF_NUM_COLS_BEFORE_GENOTYPES;  i < colCount;  i++)
                {
                if (hashLookup(uniqNames, words[i]))
                    lineFileAbort(lf, "VCF sample names in #CHROM line must be unique, but '%s' "
                                  "appears more than once", words[i]);
                hashAdd(uniqNames, words[i], NULL);
                slNameAddHead(&sampleIds, words[i]);
                struct seqInfo *si;
                AllocVar(si);
                si->seq = cloneDnaSeq(refGenome);
                si->seq->name = cloneString(words[i]);
                slAddHead(&seqInfoList, si);
                }
            slReverse(&seqInfoList);
            slReverse(&sampleIds);
            hashFree(&uniqNames);
            fputs(line, f);
            fputc('\n', f);
            }
        else if (line[0] == '#')
            {
            fputs(line, f);
            fputc('\n', f);
            }
        else
            {
            if (sampleCount < 1)
                {
                lineFileAbort(lf, "VCF header did not include #CHROM line defining sample IDs for "
                              "genotype columns");
                }
            int colCount = chopTabs(line, NULL);
            int genotypeCount = colCount - VCF_NUM_COLS_BEFORE_GENOTYPES;
            if (genotypeCount != sampleCount)
                {
                lineFileAbort(lf, "VCF header defines %d samples but there are %d genotype columns",
                              sampleCount, genotypeCount);
                }
            char *words[colCount];
            chopTabs(line, words);
            //#*** TODO: check that POS is sorted
            int pos = strtol(words[1], NULL, 10);
            if (pos > chromSize)
                {
                lineFileAbort(lf, "VCF POS value %d exceeds size of reference sequence (%d)",
                              pos, chromSize);
                }
            // make sure REF value (if given) matches reference genome
            int chromStart = pos - 1;
            struct slName *maskedReasons = maskSites[chromStart];
            char *ref = words[3];
            if (strlen(ref) != 1)
                {
                // Not an SNV -- skip it.
                //#*** should probably report or at least count these...
                continue;
                }
            char refBase = toupper(refGenome->dna[chromStart]);
            if (ref[0] == '*' || ref[0] == '.')
                ref[0] = refBase;
            else if (ref[0] != refBase)
                lineFileAbort(lf, "VCF REF value at position %d is '%s', expecting '%c' "
                              "(or '*' or '.')",
                              pos, ref, refBase);
            char altStrCopy[strlen(words[4])+1];
            safecpy(altStrCopy, sizeof altStrCopy, words[4]);
            char *alts[strlen(words[4])+1];
            chopCommas(altStrCopy, alts);
            //#*** Would be nice to trim out indels from ALT column -- but that would require
            //#*** adjusting genotype codes below.
            struct seqInfo *si = seqInfoList;
            int i;
            for (i = VCF_NUM_COLS_BEFORE_GENOTYPES;  i < colCount;  i++, si = si->next)
                {
                if (words[i][0] != '.' && !isdigit(words[i][0]))
                    {
                    lineFileAbort(lf, "VCF genotype columns must contain numeric allele codes; "
                                  "can't parse '%s'", words[i]);
                    }
                else
                    {
                    if (words[i][0] == '.')
                        {
                        si->seq->dna[chromStart] = 'n';
                        si->nCountMiddle++;
                        }
                    else
                        {
                        int alIx = atol(words[i]);
                        if (alIx > 0)
                            {
                            char *alt = alts[alIx-1];
                            if (strlen(alt) == 1)
                                {
                                si->seq->dna[chromStart] = alt[0];
                                struct singleNucChange *snc = sncNew(chromStart, ref[0], '\0',
                                                                     alt[0]);
                                if (maskedReasons)
                                    {
                                    slAddHead(&si->maskedSncList, snc);
                                    slAddHead(&si->maskedReasonsList, slRefNew(maskedReasons));
                                    }
                                else
                                    {
                                    if (isIupacAmbiguous(alt[0]))
                                        si->ambigCount++;
                                    slAddHead(&si->sncList, snc);
                                    }
                                }
                            }
                        }
                    }
                }
            if (!maskedReasons)
                {
                fputs(chrom, f);
                for (i = 1;  i < colCount;  i++)
                    {
                    fputc('\t', f);
                    fputs(words[i], f);
                    }
                fputc('\n', f);
                }
            }
        }
    }
errCatchEnd(errCatch);
carefulClose(&f);
if (errCatch->gotError)
    {
    warn("%s", errCatch->message->string);
    unlink(tn->forCgi);
    freez(&tn);
    }
errCatchFree(&errCatch); 
struct seqInfo *si;
for (si = seqInfoList;  si != NULL;  si = si->next)
    slReverse(&si->sncList);
*retSeqInfoList = seqInfoList;
*retSampleIds = sampleIds;
return tn;
}

static void displaySampleMuts(struct placementInfo *info)
{
printf("<p>Differences from the reference genome "
       "(<a href='https://www.ncbi.nlm.nih.gov/nuccore/NC_045512.2' target=_blank>"
       "NC_045512.2</a>): ");
if (info->sampleMuts == NULL)
    printf("(None; identical to reference)");
else
    {
    struct slName *sln;
    for (sln = info->sampleMuts;  sln != NULL;  sln = sln->next)
        {
        if (sln != info->sampleMuts)
            printf(", ");
        printf("%s", sln->name);
        }
    }
puts("</p>");
}

static void variantPathPrint(struct variantPathNode *variantPath)
/* Print out a variantPath; print nodeName only if non-numeric
 * (i.e. a sample ID not internal node) */
{
struct variantPathNode *vpn;
for (vpn = variantPath;  vpn != NULL;  vpn = vpn->next)
    {
    if (vpn != variantPath)
        printf(" > ");
    if (!isAllDigits(vpn->nodeName))
        printf("%s: ", vpn->nodeName);
    struct singleNucChange *snc;
    for (snc = vpn->sncList;  snc != NULL;  snc = snc->next)
        {
        if (snc != vpn->sncList)
            printf(", ");
        printf("%c%d%c", snc->parBase, snc->chromStart+1, snc->newBase);
        }
    }
}

static void displayVariantPath(struct variantPathNode *variantPath, char *sampleId)
/* Display mutations on the path to this sample. */
{
printf("<p>Mutations along the path from the root of the phylogenetic tree to %s:<br>",
       sampleId);
if (variantPath)
    {
    variantPathPrint(variantPath);
    puts("<br>");
    }
else
    puts("(None; your sample was placed at the root of the phylogenetic tree)");
puts("</p>");
}

static boolean isInternalNodeName(char *nodeName, int minNewNode)
/* Return TRUE if nodeName looks like an internal node ID from the protobuf tree, i.e. is numeric
 * and less than minNewNode. */
{
return isAllDigits(nodeName) && (atoi(nodeName) < minNewNode);
}

static struct variantPathNode *findLastInternalNode(struct variantPathNode *variantPath,
                                                    int minNewNode)
/* Return the last node in variantPath with a numeric name less than minNewNode, or NULL. */
{
if (!variantPath || !isInternalNodeName(variantPath->nodeName, minNewNode))
    return NULL;
while (variantPath->next && isInternalNodeName(variantPath->next->nodeName, minNewNode))
    variantPath = variantPath->next;
if (variantPath && isInternalNodeName(variantPath->nodeName, minNewNode))
    return variantPath;
return NULL;
}

static int mutCountCmp(const void *a, const void *b)
/* Compare number of mutations of phyloTree nodes a and b. */
{
const struct phyloTree *nodeA = *(struct phyloTree * const *)a;
const struct phyloTree *nodeB = *(struct phyloTree * const *)b;
return slCount(nodeA->priv) - slCount(nodeB->priv);
}

static struct slName *findNearestNeighbor(struct mutationAnnotatedTree *bigTree, char *sampleId,
                                          struct variantPathNode *variantPath)
/* Use the sequence of mutations in variantPath to find sampleId's parent node in bigTree,
 * then look for most similar leaf sibling(s). */
{
struct slName *neighbors = NULL;
int bigTreeINodeCount = phyloCountInternalNodes(bigTree->tree);
int minNewNode = bigTreeINodeCount + 1; // 1-based
struct variantPathNode *lastOldNode = findLastInternalNode(variantPath, minNewNode);
struct phyloTree *node = lastOldNode ? hashFindVal(bigTree->nodeHash, lastOldNode->nodeName) :
                                       bigTree->tree;
if (lastOldNode && !node)
    errAbort("Can't find last internal node for sample %s", sampleId);
// Look for a leaf kid with no mutations relative to the parent, should be closest.
if (node->numEdges == 0)
    {
    struct slName *nodeList = hashFindVal(bigTree->condensedNodes, node->ident->name);
    if (nodeList)
        slNameAddHead(&neighbors, nodeList->name);
    else
        slNameAddHead(&neighbors, node->ident->name);
    }
else
    {
    int leafCount = 0;
    int i;
    for (i = 0;  i < node->numEdges;  i++)
        {
        struct phyloTree *kid = node->edges[i];
        if (kid->numEdges == 0)
            {
            leafCount++;
            struct singleNucChange *kidMuts = kid->priv;
            if (!kidMuts)
                {
                struct slName *nodeList = hashFindVal(bigTree->condensedNodes, kid->ident->name);
                if (nodeList)
                    slNameAddHead(&neighbors, nodeList->name);
                else
                    slNameAddHead(&neighbors, kid->ident->name);
                break;
                }
            }
        }
    if (neighbors == NULL && leafCount)
        {
        // Pick the leaf with the fewest mutations.
        struct phyloTree *leafKids[leafCount];
        int leafIx = 0;
        for (i = 0;  i < node->numEdges;  i++)
            {
            struct phyloTree *kid = node->edges[i];
            if (kid->numEdges == 0)
                leafKids[leafIx++] = kid;
            }
        qsort(leafKids, leafCount, sizeof(leafKids[0]), mutCountCmp);
        neighbors = slNameNew(leafKids[0]->ident->name);
        }
    }
return neighbors;
}

static void printVariantPathNoNodeNames(FILE *f, struct variantPathNode *variantPath)
/* Print out variant path with no node names (even if non-numeric) to f. */
{
struct variantPathNode *vpn;
for (vpn = variantPath;  vpn != NULL;  vpn = vpn->next)
    {
    if (vpn != variantPath)
        fprintf(f, " > ");
    struct singleNucChange *snc;
    for (snc = vpn->sncList;  snc != NULL;  snc = snc->next)
        {
        if (snc != vpn->sncList)
            fprintf(f, ", ");
        fprintf(f, "%c%d%c", snc->parBase, snc->chromStart+1, snc->newBase);
        }
    }
}

static struct hash *getSampleMetadata(char *metadataFile)
/* If config.ra defines a metadataFile, load its contents into a hash indexed by EPI ID and return;
 * otherwise return NULL. */
{
struct hash *sampleMetadata = NULL;
if (isNotEmpty(metadataFile) && fileExists(metadataFile))
    {
    sampleMetadata = hashNew(0);
    struct lineFile *lf = lineFileOpen(metadataFile, TRUE);
    int headerWordCount = 0;
    char **headerWords = NULL;
    char *line;
    // Check for header line
    if (lineFileNext(lf, &line, NULL))
        {
        if (startsWithWord("strain", line))
            {
            char *headerLine = cloneString(line);
            headerWordCount = chopString(headerLine, "\t", NULL, 0);
            AllocArray(headerWords, headerWordCount);
            chopString(headerLine, "\t", headerWords, headerWordCount);
            }
        else
            errAbort("Missing header line from metadataFile %s", metadataFile);
        }
    int strainIx = stringArrayIx("strain", headerWords, headerWordCount);
    int epiIdIx = stringArrayIx("gisaid_epi_isl", headerWords, headerWordCount);
    int genbankIx = stringArrayIx("genbank_accession", headerWords, headerWordCount);
    int dateIx = stringArrayIx("date", headerWords, headerWordCount);
    int authorIx = stringArrayIx("authors", headerWords, headerWordCount);
    int nCladeIx = stringArrayIx("Nextstrain_clade", headerWords, headerWordCount);
    int gCladeIx = stringArrayIx("GISAID_clade", headerWords, headerWordCount);
    int lineageIx = stringArrayIx("pangolin_lineage", headerWords, headerWordCount);
    int countryIx = stringArrayIx("country", headerWords, headerWordCount);
    int divisionIx = stringArrayIx("division", headerWords, headerWordCount);
    int locationIx = stringArrayIx("location", headerWords, headerWordCount);
    int countryExpIx = stringArrayIx("country_exposure", headerWords, headerWordCount);
    int divExpIx = stringArrayIx("division_exposure", headerWords, headerWordCount);
    int origLabIx = stringArrayIx("originating_lab", headerWords, headerWordCount);
    int subLabIx = stringArrayIx("submitting_lab", headerWords, headerWordCount);
    int regionIx = stringArrayIx("region", headerWords, headerWordCount);
    while (lineFileNext(lf, &line, NULL))
        {
        char *words[headerWordCount];
        int wordCount = chopTabs(line, words);
        lineFileExpectWords(lf, headerWordCount, wordCount);
        struct sampleMetadata *met;
        AllocVar(met);
        if (strainIx >= 0)
            met->strain = cloneString(words[strainIx]);
        if (epiIdIx >= 0)
            met->epiId = cloneString(words[epiIdIx]);
        if (genbankIx >= 0 && !sameString("?", words[genbankIx]))
            met->gbAcc = cloneString(words[genbankIx]);
        if (dateIx >= 0)
            met->date = cloneString(words[dateIx]);
        if (authorIx >= 0)
            met->author = cloneString(words[authorIx]);
        if (nCladeIx >= 0)
            met->nClade = cloneString(words[nCladeIx]);
        if (gCladeIx >= 0)
            met->gClade = cloneString(words[gCladeIx]);
        if (lineageIx >= 0)
            met->lineage = cloneString(words[lineageIx]);
        if (countryIx >= 0)
            met->country = cloneString(words[countryIx]);
        if (divisionIx >= 0)
            met->division = cloneString(words[divisionIx]);
        if (locationIx >= 0)
            met->location = cloneString(words[locationIx]);
        if (countryExpIx >= 0)
            met->countryExp = cloneString(words[countryExpIx]);
        if (divExpIx >= 0)
            met->divExp = cloneString(words[divExpIx]);
        if (origLabIx >= 0)
            met->origLab = cloneString(words[origLabIx]);
        if (subLabIx >= 0)
            met->subLab = cloneString(words[subLabIx]);
        if (regionIx >= 0)
            met->region = cloneString(words[regionIx]);
        // If epiId and/or genbank ID is included, we'll probably be using that to look up items.
        if (epiIdIx >= 0 && !isEmpty(words[epiIdIx]))
            hashAdd(sampleMetadata, words[epiIdIx], met);
        if (genbankIx >= 0 && !isEmpty(words[genbankIx]) && !sameString("?", words[genbankIx]))
            {
            if (strchr(words[genbankIx], '.'))
                {
                // Index by versionless accession
                char copy[strlen(words[genbankIx])+1];
                safecpy(copy, sizeof copy, words[genbankIx]);
                char *dot = strchr(copy, '.');
                *dot = '\0';
                hashAdd(sampleMetadata, copy, met);
                }
            else
                hashAdd(sampleMetadata, words[genbankIx], met);
            }
        if (strainIx >= 0 && !isEmpty(words[strainIx]))
            hashAdd(sampleMetadata, words[strainIx], met);
        }
    lineFileClose(&lf);
    }
return sampleMetadata;
}

char *epiIdFromSampleName(char *sampleId)
/* If an EPI_ISL_# ID is present somewhere in sampleId, extract and return it, otherwise NULL. */
{
char *epiId = cloneString(strstr(sampleId, "EPI_ISL_"));
if (epiId)
    {
    char *p = epiId + strlen("EPI_ISL_");
    while (isdigit(*p))
        p++;
    *p = '\0';
    }
return epiId;
}

char *gbIdFromSampleName(char *sampleId)
/* If a GenBank accession is present somewhere in sampleId, extract and return it, otherwise NULL. */
{
char *gbId = NULL;
regmatch_t substrs[2];
if (regexMatchSubstr(sampleId, "([A-Z][A-Z][0-9]{6})", substrs, ArraySize(substrs)))
    {
    // Make sure there are word boundaries around the match
    if ((substrs[1].rm_so == 0 || !isalnum(sampleId[substrs[1].rm_so-1])) &&
        !isalnum(sampleId[substrs[1].rm_eo]))
        gbId = cloneStringZ(sampleId+substrs[1].rm_so, substrs[1].rm_eo - substrs[1].rm_so);
    }
return gbId;
}

struct sampleMetadata *metadataForSample(struct hash *sampleMetadata, char *sampleId)
/* Look up sampleId in sampleMetadata, by accession if sampleId seems to include an accession. */
{
struct sampleMetadata *met = NULL;
if (sampleMetadata == NULL)
    return NULL;
char *epiId = epiIdFromSampleName(sampleId);
if (epiId)
    met = hashFindVal(sampleMetadata, epiId);
if (!met)
    {
    char *gbId = gbIdFromSampleName(sampleId);
    if (gbId)
        met = hashFindVal(sampleMetadata, gbId);
    }
if (!met)
    met = hashFindVal(sampleMetadata, sampleId);
if (!met && strchr(sampleId, '|'))
    {
    char copy[strlen(sampleId)+1];
    safecpy(copy, sizeof copy, sampleId);
    char *words[4];
    int wordCount = chopString(copy, "|", words, ArraySize(words));
    if (isNotEmpty(words[0]))
        met = hashFindVal(sampleMetadata, words[0]);
    if (met == NULL && wordCount > 1 && isNotEmpty(words[1]))
        met = hashFindVal(sampleMetadata, words[1]);
    }
// If it's one of our collapsed node names, dig out the example name and try that.
if (!met && isdigit(sampleId[0]) && strstr(sampleId, "_from_"))
    {
    char *eg = strstr(sampleId, "_eg_");
    if (eg)
        met = hashFindVal(sampleMetadata, eg+strlen("_eg_"));
    }
return met;
}

static char *lineageForSample(struct hash *sampleMetadata, char *sampleId)
/* Look up sampleId's lineage in epiToLineage file. Return NULL if we don't find a match. */
{
char *lineage = NULL;
struct sampleMetadata *met = metadataForSample(sampleMetadata, sampleId);
if (met)
    lineage = met->lineage;
return lineage;
}

static void displayNearestNeighbors(struct mutationAnnotatedTree *bigTree, char *source,
                                    struct placementInfo *info, struct hash *sampleMetadata)
/* Use info->variantPaths to find sample's nearest neighbor(s) in tree. */
{
if (bigTree)
    {
    printf("<p>Nearest neighboring %s sequence already in phylogenetic tree: ", source);
    struct slName *neighbors = findNearestNeighbor(bigTree, info->sampleId, info->variantPath);
    struct slName *neighbor;
    for (neighbor = neighbors;  neighbor != NULL;  neighbor = neighbor->next)
        {
        if (neighbor != neighbors)
            printf(", ");
        printf("%s", neighbor->name);
        char *lineage = lineageForSample(sampleMetadata, neighbor->name);
        if (isNotEmpty(lineage))
            printf(": lineage %s", lineage);
        }
    puts("</p>");
    }
}

static void displayBestNodes(struct placementInfo *info, struct hash *sampleMetadata)
/* Show the node(s) most closely related to sample. */
{
if (info->bestNodeCount == 1)
    printf("<p>The placement in the tree is unambiguous; "
           "there are no other parsimony-optimal placements in the phylogenetic tree.</p>\n");
else if (info->bestNodeCount > 1)
    printf("<p>This placement is not the only parsimony-optimal placement in the tree; "
           "%d other placements exist.</p>\n", info->bestNodeCount - 1);
if (showBestNodePaths && info->bestNodes)
    {
    if (info->bestNodeCount != slCount(info->bestNodes))
        errAbort("Inconsistent bestNodeCount (%d) and number of bestNodes (%d)",
                 info->bestNodeCount, slCount(info->bestNodes));
    if (info->bestNodeCount > 1)
        printf("<ul><li><b>used for placement</b>: ");
    if (differentString(info->bestNodes->name, "?") && !isAllDigits(info->bestNodes->name))
        printf("%s ", info->bestNodes->name);
    printVariantPathNoNodeNames(stdout, info->bestNodes->variantPath);
    struct bestNodeInfo *bn;
    for (bn = info->bestNodes->next;  bn != NULL;  bn = bn->next)
        {
        printf("\n<li>");
        if (differentString(bn->name, "?") && !isAllDigits(bn->name))
            printf("%s ", bn->name);
        printVariantPathNoNodeNames(stdout, bn->variantPath);
        char *lineage = lineageForSample(sampleMetadata, bn->name);
        if (isNotEmpty(lineage))
            printf(": lineage %s", lineage);
        }
    if (info->bestNodeCount > 1)
        puts("</ul>");
    puts("</p>");
    }
}

static int placementInfoRefCmpSampleMuts(const void *va, const void *vb)
/* Compare slRef->placementInfo->sampleMuts lists.  Shorter lists first.  Using alpha sort
 * to distinguish between different sampleMuts contents arbitrarily; the purpose is to
 * clump samples with identical lists. */
{
struct slRef * const *rra = va;
struct slRef * const *rrb = vb;
struct placementInfo *pa = (*rra)->val;
struct placementInfo *pb = (*rrb)->val;
int diff = slCount(pa->sampleMuts) - slCount(pb->sampleMuts);
if (diff == 0)
    {
    struct slName *slnA, *slnB;
    for (slnA = pa->sampleMuts, slnB = pb->sampleMuts;  slnA != NULL;
         slnA = slnA->next, slnB = slnB->next)
        {
        diff = strcmp(slnA->name, slnB->name);
        if (diff != 0)
            break;
        }
    }
return diff;
}

static struct slRef *getPlacementRefList(struct slName *sampleIds, struct hash *samplePlacements)
/* Look up sampleIds in samplePlacements and return ref list of placements. */
{
struct slRef *placementRefs = NULL;
struct slName *sample;
for (sample = sampleIds;  sample != NULL;  sample = sample->next)
    {
    struct placementInfo *info = hashFindVal(samplePlacements, sample->name);
    if (!info)
        errAbort("getPlacementRefList: can't find placement info for sample '%s'",
                 sample->name);
    slAddHead(&placementRefs, slRefNew(info));
    }
slReverse(&placementRefs);
return placementRefs;
}

static int countIdentical(struct slRef *placementRefs)
/* Return the number of placements that have identical sampleMuts lists. */
{
int clumpCount = 0;
struct slRef *ref;
for (ref = placementRefs;  ref != NULL;  ref = ref->next)
    {
    clumpCount++;
    if (ref->next == NULL || placementInfoRefCmpSampleMuts(&ref, &ref->next))
        break;
    }
return clumpCount;
}

static void asciiTree(struct phyloTree *node, char *indent, boolean isLast)
/* Until we can make a real graphic, at least print an ascii tree. */
{
if (isNotEmpty(indent) || isNotEmpty(node->ident->name))
    {
    if (node->ident->name && !isAllDigits(node->ident->name))
        printf("%s %s\n", indent, node->ident->name);
    }
int indentLen = strlen(indent);
char indentForKids[indentLen+1];
safecpy(indentForKids, sizeof indentForKids, indent);
if (indentLen >= 4)
    {
    if (isLast)
        safecpy(indentForKids+indentLen - 4, 4 + 1, "    ");
    else
        safecpy(indentForKids+indentLen - 4, 4 + 1, "|   ");
    }
if (node->numEdges > 0)
    {
    char kidIndent[strlen(indent)+5];
    safef(kidIndent, sizeof kidIndent, "%s%s", indentForKids, "+---");
    int i;
    for (i = 0;  i < node->numEdges;  i++)
        asciiTree(node->edges[i], kidIndent, (i == node->numEdges - 1));
    }
}

static void describeSamplePlacements(struct slName *sampleIds, struct hash *samplePlacements,
                                     struct phyloTree *subtree, struct hash *sampleMetadata,
                                     struct mutationAnnotatedTree *bigTree, char *source)
/* Report how each sample fits into the big tree. */
{
// Sort sample placements by sampleMuts so we can group identical samples.
struct slRef *placementRefs = getPlacementRefList(sampleIds, samplePlacements);
slSort(&placementRefs, placementInfoRefCmpSampleMuts);
int relatedCount = slCount(placementRefs);
int clumpSize = countIdentical(placementRefs);
if (clumpSize < relatedCount && relatedCount > 2)
    {
    // Not all of the related sequences are identical, so they will be broken down into
    // separate "clumps".  List all related samples first to avoid confusion.
    puts("<pre>");
    asciiTree(subtree, "", TRUE);
    puts("</pre>");
    }
struct slRef *refsToGo = placementRefs;
while ((clumpSize = countIdentical(refsToGo)) > 0)
    {
    struct slRef *ref = refsToGo;
    struct placementInfo *info = ref->val;
    if (clumpSize > 1)
        {
        // Sort identical samples alphabetically:
        struct slName *sortedSamples = NULL;
        int i;
        for (i = 0, ref = refsToGo;  ref != NULL && i < clumpSize;  ref = ref->next, i++)
            {
            info = ref->val;
            slNameAddHead(&sortedSamples, info->sampleId);
            }
        slNameSort(&sortedSamples);
        printf("<b>%d identical samples:</b>\n<ul>\n", clumpSize);
        struct slName *sln;
        for (sln = sortedSamples;  sln != NULL;  sln = sln->next)
            printf("<li><b>%s</b>\n", sln->name);
        puts("</ul>");
        }
    else
        {
        printf("<b>%s</b>\n", info->sampleId);
        ref = ref->next;
        }
    refsToGo = ref;
    displaySampleMuts(info);
    if (info->imputedBases)
        {
        puts("<p>Base values imputed by parsimony:\n<ul>");
        struct baseVal *bv;
        for (bv = info->imputedBases;  bv != NULL;  bv = bv->next)
            printf("<li>%d: %s\n", bv->chromStart+1, bv->val);
        puts("</ul>");
        puts("</p>");
        }
    displayVariantPath(info->variantPath, clumpSize == 1 ? info->sampleId : "samples");
    displayBestNodes(info, sampleMetadata);
    if (!showBestNodePaths)
        displayNearestNeighbors(bigTree, source, info, sampleMetadata);
    if (showParsimonyScore && info->parsimonyScore > 0)
        printf("<p>Parsimony score added by your sample: %d</p>\n", info->parsimonyScore);
        //#*** TODO: explain parsimony score
    }
}

struct phyloTree *phyloPruneToIds(struct phyloTree *node, struct slName *sampleIds)
/* Prune all descendants of node that have no leaf descendants in sampleIds. */
{
if (node->numEdges)
    {
    struct phyloTree *prunedKids = NULL;
    int i;
    for (i = 0;  i < node->numEdges;  i++)
        {
        struct phyloTree *kidIntersected = phyloPruneToIds(node->edges[i], sampleIds);
        if (kidIntersected)
            slAddHead(&prunedKids, kidIntersected);
        }
    int kidCount = slCount(prunedKids);
    assert(kidCount <= node->numEdges);
    if (kidCount > 1)
        {
        slReverse(&prunedKids);
        // There is no phyloTreeFree, but if we ever add one, should use it here.
        node->numEdges = kidCount;
        struct phyloTree *kid;
        for (i = 0, kid = prunedKids;  i < kidCount;  i++, kid = kid->next)
            {
            node->edges[i] = kid;
            kid->parent = node;
            }
        }
    else
        return prunedKids;
    }
else if (! (node->ident->name && slNameInList(sampleIds, node->ident->name)))
    node = NULL;
return node;
}

static struct subtreeInfo *subtreeInfoForSample(struct subtreeInfo *subtreeInfoList, char *name,
                                                int *retIx)
/* Find the subtree that contains sample name and set *retIx to its index in the list.
 * If we can't find it, return NULL and set *retIx to -1. */
{
struct subtreeInfo *ti;
int ix;
for (ti = subtreeInfoList, ix = 0;  ti != NULL;  ti = ti->next, ix++)
    if (slNameInList(ti->subtreeUserSampleIds, name))
        break;
if (ti == NULL)
    ix = -1;
*retIx = ix;
return ti;
}

static void lookForCladesAndLineages(struct seqInfo *seqInfoList, struct hash *samplePlacements,
                                     boolean *retGotClades, boolean *retGotLineages)
/* See if UShER has annotated any clades and/or lineages for seqs. */
{
boolean gotClades = FALSE, gotLineages = FALSE;
struct seqInfo *si;
for (si = seqInfoList;  si != NULL;  si = si->next)
    {
    struct placementInfo *pi = hashFindVal(samplePlacements, si->seq->name);
    if (pi)
        {
        if (isNotEmpty(pi->nextClade))
            gotClades = TRUE;
        if (isNotEmpty(pi->pangoLineage))
            gotLineages = TRUE;
        if (gotClades && gotLineages)
            break;
        }
    }
*retGotClades = gotClades;
*retGotLineages = gotLineages;
}

static char *nextstrainHost()
/* Return the nextstrain hostname from an hg.conf param, or NULL if missing. */
{
return cfgOption("nextstrainHost");
}

static char *nextstrainUrlFromTn(struct tempName *jsonTn)
/* Return a link to Nextstrain to view an annotated subtree. */
{
char *jsonUrlForNextstrain = urlFromTn(jsonTn);
char *protocol = strstr(jsonUrlForNextstrain, "://");
if (protocol)
    jsonUrlForNextstrain = protocol + strlen("://");
struct dyString *dy = dyStringCreate("%s/fetch/%s", nextstrainHost(), jsonUrlForNextstrain);
return dyStringCannibalize(&dy);
}

static void makeNextstrainButton(char *id, struct tempName *tn, char *label)
/* Make a button to view an auspice JSON file in Nextstrain. */
{
char *nextstrainUrl = nextstrainUrlFromTn(tn);
struct dyString *js = dyStringCreate("window.open('%s');", nextstrainUrl);
cgiMakeOnClickButton(id, js->string, label);
dyStringFree(&js);
freeMem(nextstrainUrl);
}

static void makeNextstrainButtonN(char *idBase, int ix, struct tempName *jsonTns[])
/* Make a button to view one subtree in Nextstrain.  idBase is a short string and
 * ix is 0-based subtree number. */
{
char buttonId[256];
safef(buttonId, sizeof buttonId, "%s%d", idBase, ix+1);
char buttonLabel[256];
safef(buttonLabel, sizeof buttonLabel, "view subtree %d in Nextstrain", ix+1);
makeNextstrainButton(buttonId, jsonTns[ix], buttonLabel);
}

static void makeNsSingleTreeButton(struct tempName *tn)
/* Make a button to view single subtree (with all uploaded samples) in Nextstrain. */
{
makeNextstrainButton("viewNextstrainSingleSubtree", tn, "view comprehensive subtree in Nextstrain");
}

static void makeButtonRow(struct tempName *singleSubtreeJsonTn, struct tempName *jsonTns[],
                          int subtreeCount, boolean isFasta)
/* Russ's suggestion: row of buttons at the top to view results in GB, Nextstrain, Nextclade. */
{
puts("<p>");
cgiMakeButton("submit", "view in Genome Browser");
if (nextstrainHost())
    {
    printf("&nbsp;");
    makeNsSingleTreeButton(singleSubtreeJsonTn);
    int ix;
    for (ix = 0;  ix < subtreeCount;  ix++)
        {
        printf("&nbsp;");
        makeNextstrainButtonN("viewNextstrainTopRow", ix, jsonTns);
        }
    }
if (0 && isFasta)
    {
    printf("&nbsp;");
    struct dyString *js = dyStringCreate("window.open('https://master.clades.nextstrain.org/"
                                         "?input-fasta=%s');",
                                         "needATn");  //#*** TODO: save FASTA to file
    cgiMakeOnClickButton("viewNextclade", js->string, "view sequences in Nextclade");
    }
puts("</p>");
}

#define TOOLTIP(text) " <div class='tooltip'>(?)<span class='tooltiptext'>" text "</span></div>"

static void printSummaryHeader(boolean isFasta, boolean gotClades, boolean gotLineages)
/* Print the summary table header row with tooltips explaining columns. */
{
puts("<thead><tr>");
if (isFasta)
    puts("<th>Fasta Sequence</th>\n"
         "<th>Size"
         TOOLTIP("Length of uploaded sequence in bases, excluding runs of N bases at "
                 "beginning and/or end")
         "</th>\n<th>#Ns"
         TOOLTIP("Number of 'N' bases in uploaded sequence, excluding runs of N bases at "
                 "beginning and/or end")
         "</th>");
else
    puts("<th>VCF Sample</th>\n"
         "<th>#Ns"
         TOOLTIP("Number of no-call variants for this sample in uploaded VCF, "
                 "i.e. '.' used in genotype column")
         "</th>");
puts("<th>#Mixed"
     TOOLTIP("Number of IUPAC ambiguous bases, e.g. 'R' for 'A or G'")
     "</th>");
if (isFasta)
    puts("<th>Bases aligned"
         TOOLTIP("Number of bases aligned to reference NC_045512.2 Wuhan/Hu-1, including "
                 "matches and mismatches")
         "</th>\n<th>Inserted bases"
         TOOLTIP("Number of bases in aligned portion of uploaded sequence that are not present in "
                 "reference NC_045512.2 Wuhan/Hu-1")
         "</th>\n<th>Deleted bases"
         TOOLTIP("Number of bases in reference NC_045512.2 Wuhan/Hu-1 that are not "
                 "present in aligned portion of uploaded sequence")
         "</th>");
puts("<th>#SNVs used for placement"
     TOOLTIP("Number of single-nucleotide variants in uploaded sample "
             "(does not include N's or mixed bases) used by UShER to place sample "
             "in phylogenetic tree")
     "</th>\n<th>#Masked SNVs"
     TOOLTIP("Number of single-nucleotide variants in uploaded sample that are masked "
             "(not used for placement) because they occur at known "
             "<a href='https://virological.org/t/issues-with-sars-cov-2-sequencing-data/473/12' "
             "target=_blank>Problematic Sites</a>"));;
if (gotClades)
    puts("</th>\n<th>Nextstrain clade"
     TOOLTIP("The <a href='https://nextstrain.org/blog/2021-01-06-updated-SARS-CoV-2-clade-naming' "
             "target=_blank>Nextstrain clade</a> assigned to the sample by UShER"));
if (gotLineages)
    puts("</th>\n<th>Pango lineage"
     TOOLTIP("The <a href='https://cov-lineages.org/' "
             "target=_blank>Pango lineage</a> assigned to the sample by UShER"));
puts("</th>\n<th>Neighboring sample in tree"
     TOOLTIP("A sample already in the tree that is a child of the node at which the uploaded "
             "sample was placed, to give an example of a closely related sample")
     "</th>\n<th>Lineage of neighbor"
     TOOLTIP("The <a href='https://cov-lineages.org/' target=_blank>"
             "Pango lineage</a> assigned to the nearest neighboring sample already in the tree")
     "</th>\n<th>#Imputed values for mixed bases"
     TOOLTIP("If the uploaded sequence contains mixed/ambiguous bases, then UShER may assign "
             "values based on maximum parsimony")
     "</th>\n<th>#Maximally parsimonious placements"
     TOOLTIP("Number of potential placements in the tree with minimal parsimony score; "
             "the higher the number, the less confident the placement")
     "</th>\n<th>Parsimony score"
     TOOLTIP("Number of mutations/changes that must be added to the tree when placing the "
             "uploaded sample; the higher the number, the more diverged the sample")
     "</th>\n<th>Subtree number"
     TOOLTIP("Sequence number of subtree that contains this sample")
     "</th></tr></thead>");
}


// Default QC thresholds for calling an input sequence excellent/good/fair/bad [/fail]:
static int qcThresholdsMinLength[] = { 29750, 29500, 29000, 28000 };
static int qcThresholdsMaxNs[] = { 0, 5, 20, 100 };
static int qcThresholdsMaxMixed[] = { 0, 5, 20, 100 };
static int qcThresholdsMaxIndel[] = { 9, 18, 24, 36 };
static int qcThresholdsMaxSNVs[] = { 25, 35, 45, 55 };
static int qcThresholdsMaxMaskedSNVs[] = { 0, 1, 2, 3 };
static int qcThresholdsMaxImputed[] = { 0, 5, 20, 100 };
static int qcThresholdsMaxPlacements[] = { 1, 2, 3, 4 };
static int qcThresholdsMaxPScore[] = { 0, 2, 5, 10 };

static void wordsToQcThresholds(char **words, int *thresholds)
/* Parse words from file into thresholds array.  Caller must ensure words and thresholds each
 * have 4 items. */
{
int i;
for (i = 0;  i < 4;  i++)
    thresholds[i] = atoi(words[i]);
}

static void readQcThresholds(char *db)
/* If config.ra specifies a file with QC thresholds for excellent/good/fair/bad [/fail],
 * parse it and replace the default values in qcThresholds arrays.  */
{
char *qcThresholdsFile = phyloPlaceDbSettingPath(db, "qcThresholds");
if (isNotEmpty(qcThresholdsFile))
    {
    if (fileExists(qcThresholdsFile))
        {
        struct lineFile *lf = lineFileOpen(qcThresholdsFile, TRUE);
        char *line;
        while (lineFileNext(lf, &line, NULL))
            {
            char *words[16];
            int wordCount = chopTabs(line, words);
            lineFileExpectWords(lf, 5, wordCount);
            if (sameWord(words[0], "length"))
                wordsToQcThresholds(words+1, qcThresholdsMinLength);
            else if (sameWord(words[0], "nCount"))
                wordsToQcThresholds(words+1, qcThresholdsMaxNs);
            else if (sameWord(words[0], "mixedCount"))
                wordsToQcThresholds(words+1, qcThresholdsMaxMixed);
            else if (sameWord(words[0], "indelCount"))
                wordsToQcThresholds(words+1, qcThresholdsMaxIndel);
            else if (sameWord(words[0], "snvCount"))
                wordsToQcThresholds(words+1, qcThresholdsMaxSNVs);
            else if (sameWord(words[0], "maskedSnvCount"))
                wordsToQcThresholds(words+1, qcThresholdsMaxMaskedSNVs);
            else if (sameWord(words[0], "imputedBases"))
                wordsToQcThresholds(words+1, qcThresholdsMaxImputed);
            else if (sameWord(words[0], "placementCount"))
                wordsToQcThresholds(words+1, qcThresholdsMaxPlacements);
            else if (sameWord(words[0], "parsimony"))
                wordsToQcThresholds(words+1, qcThresholdsMaxPScore);
            else
                warn("qcThresholds file %s: unrecognized parameter '%s', skipping",
                     qcThresholdsFile, words[0]);
            }
        lineFileClose(&lf);
        }
    else
        warn("qcThresholds %s: file not found", qcThresholdsFile);
    }
}

static char *qcClassForIntMin(int n, int thresholds[])
/* Return {qcExcellent, qcGood, qcMeh, qcBad or qcFail} depending on how n compares to the
 * thresholds. Don't free result. */
{
if (n >= thresholds[0])
    return "qcExcellent";
else if (n >= thresholds[1])
    return "qcGood";
else if (n >= thresholds[2])
    return "qcMeh";
else if (n >= thresholds[3])
    return "qcBad";
else
    return "qcFail";
}

static char *qcClassForLength(int length)
/* Return qc class for length of sequence. */
{
return qcClassForIntMin(length, qcThresholdsMinLength);
}

static char *qcClassForIntMax(int n, int thresholds[])
/* Return {qcExcellent, qcGood, qcMeh, qcBad or qcFail} depending on how n compares to the
 * thresholds. Don't free result. */
{
if (n <= thresholds[0])
    return "qcExcellent";
else if (n <= thresholds[1])
    return "qcGood";
else if (n <= thresholds[2])
    return "qcMeh";
else if (n <= thresholds[3])
    return "qcBad";
else
    return "qcFail";
}

static char *qcClassForNs(int nCount)
/* Return qc class for #Ns in sample. */
{
return qcClassForIntMax(nCount, qcThresholdsMaxNs);
}

static char *qcClassForMixed(int mixedCount)
/* Return qc class for #ambiguous bases in sample. */
{
return qcClassForIntMax(mixedCount, qcThresholdsMaxMixed);
}

static char *qcClassForIndel(int indelCount)
/* Return qc class for #inserted or deleted bases. */
{
return qcClassForIntMax(indelCount, qcThresholdsMaxIndel);
}

static char *qcClassForSNVs(int snvCount)
/* Return qc class for #SNVs in sample. */
{
return qcClassForIntMax(snvCount, qcThresholdsMaxSNVs);
}

static char *qcClassForMaskedSNVs(int maskedCount)
/* Return qc class for #SNVs at problematic sites. */
{
return qcClassForIntMax(maskedCount, qcThresholdsMaxMaskedSNVs);
}

static char *qcClassForImputedBases(int imputedCount)
/* Return qc class for #ambiguous bases for which UShER imputed values based on placement. */
{
return qcClassForIntMax(imputedCount, qcThresholdsMaxImputed);
}

static char *qcClassForPlacements(int placementCount)
/* Return qc class for number of equally parsimonious placements. */
{
return qcClassForIntMax(placementCount, qcThresholdsMaxPlacements);
}

static char *qcClassForPScore(int parsimonyScore)
/* Return qc class for parsimonyScore. */
{
return qcClassForIntMax(parsimonyScore, qcThresholdsMaxPScore);
}

static void printTooltip(char *text)
/* Print a tooltip with explanatory text. */
{
printf(TOOLTIP("%s"), text);
}

static void appendExcludingNs(struct dyString *dy, struct seqInfo *si)
/* Append a note to dy about how many N bases and start and/or end are excluded from statistic. */
{
dyStringAppend(dy, "excluding ");
if (si->nCountStart)
    dyStringPrintf(dy, "%d N bases at start", si->nCountStart);
if (si->nCountStart && si->nCountEnd)
    dyStringAppend(dy, " and ");
if (si->nCountEnd)
    dyStringPrintf(dy, "%d N bases at end", si->nCountEnd);
}

static void summarizeSequences(struct seqInfo *seqInfoList, boolean isFasta,
                               struct usherResults *ur, struct tempName *jsonTns[],
                               struct hash *sampleMetadata, struct mutationAnnotatedTree *bigTree,
                               struct dnaSeq *refGenome)
/* Show a table with composition & alignment stats for each sequence that passed basic QC. */
{
if (seqInfoList)
    {
    puts("<table class='seqSummary'>");
    boolean gotClades = FALSE, gotLineages = FALSE;
    lookForCladesAndLineages(seqInfoList, ur->samplePlacements, &gotClades, &gotLineages);
    printSummaryHeader(isFasta, gotClades, gotLineages);
    puts("<tbody>");
    struct dyString *dy = dyStringNew(0);
    struct seqInfo *si;
    for (si = seqInfoList;  si != NULL;  si = si->next)
        {
        puts("<tr>");
        printf("<th>%s</td>", replaceChars(si->seq->name, "|", " | "));
        if (isFasta)
            {
            if (si->nCountStart || si->nCountEnd)
                {
                int effectiveLength = si->seq->size - (si->nCountStart + si->nCountEnd);
                dyStringClear(dy);
                dyStringPrintf(dy, "%d ", effectiveLength);
                appendExcludingNs(dy, si);
                dyStringPrintf(dy, " (original size %d)", si->seq->size);
                printf("<td class='%s'>%d", qcClassForLength(effectiveLength), effectiveLength);
                printTooltip(dy->string);
                printf("</td>");
                }
            else
                printf("<td class='%s'>%d</td>", qcClassForLength(si->seq->size), si->seq->size);
            }
        printf("<td class='%s'>%d",
               qcClassForNs(si->nCountMiddle), si->nCountMiddle);
        if (si->nCountStart || si->nCountEnd)
            {
            dyStringClear(dy);
            dyStringPrintf(dy, "%d Ns ", si->nCountMiddle);
            appendExcludingNs(dy, si);
            printTooltip(dy->string);
            }
        printf("</td><td class='%s'>%d ", qcClassForMixed(si->ambigCount), si->ambigCount);
        int alignedAmbigCount = 0;
        if (si->ambigCount > 0)
            {
            dyStringClear(dy);
            struct singleNucChange *snc;
            for (snc = si->sncList;  snc != NULL;  snc = snc->next)
                {
                if (isIupacAmbiguous(snc->newBase))
                    {
                    dyStringAppendSep(dy, ", ");
                    dyStringPrintf(dy, "%c%d%c", snc->refBase, snc->chromStart+1, snc->newBase);
                    alignedAmbigCount++;
                    }
                }
            if (isEmpty(dy->string))
                dyStringAppend(dy, "(Masked or not aligned to reference)");
            else if (alignedAmbigCount != si->ambigCount)
                dyStringPrintf(dy, " (%d masked or not aligned to reference)",
                               si->ambigCount - alignedAmbigCount);
            printTooltip(dy->string);
            }
        printf("</td>");
        if (isFasta)
            {
            struct psl *psl = si->psl;
            if (psl)
                {
                int aliCount = psl->match + psl->misMatch + psl->repMatch;
                printf("<td class='%s'>%d ", qcClassForLength(aliCount), aliCount);
                dyStringClear(dy);
                dyStringPrintf(dy, "bases %d - %d align to reference bases %d - %d",
                               psl->qStart+1, psl->qEnd, psl->tStart+1, psl->tEnd);
                printTooltip(dy->string);
                printf("</td><td class='%s'>%d ",
                       qcClassForIndel(si->insBases), si->insBases);
                if (si->insBases)
                    {
                    printTooltip(si->insRanges);
                    }
                printf("</td><td class='%s'>%d ",
                       qcClassForIndel(si->delBases), si->delBases);
                if (si->delBases)
                    {
                    printTooltip(si->delRanges);
                    }
                printf("</td>");
                }
            else
                printf("<td colspan=3 class='%s'> not alignable </td>",
                       qcClassForLength(0));
            }
        int snvCount = slCount(si->sncList) - alignedAmbigCount;
        printf("<td class='%s'>%d", qcClassForSNVs(snvCount), snvCount);
        if (snvCount > 0)
            {
            dyStringClear(dy);
            struct singleNucChange *snc;
            for (snc = si->sncList;  snc != NULL;  snc = snc->next)
                {
                if (!isIupacAmbiguous(snc->newBase))
                    {
                    dyStringAppendSep(dy, ", ");
                    dyStringPrintf(dy, "%c%d%c", snc->refBase, snc->chromStart+1, snc->newBase);
                    }
                }
            printTooltip(dy->string);
            }
        int maskedCount = slCount(si->maskedSncList);
        printf("</td><td class='%s'>%d", qcClassForMaskedSNVs(maskedCount), maskedCount);
        if (maskedCount > 0)
            {
            dyStringClear(dy);
            struct singleNucChange *snc;
            struct slRef *reasonsRef;
            for (snc = si->maskedSncList, reasonsRef = si->maskedReasonsList;  snc != NULL;
                 snc = snc->next, reasonsRef = reasonsRef->next)
                {
                dyStringAppendSep(dy, ", ");
                struct slName *reasonList = reasonsRef->val, *reason;
                replaceChar(reasonList->name, '_', ' ');
                dyStringPrintf(dy, "%c%d%c (%s",
                               snc->refBase, snc->chromStart+1, snc->newBase, reasonList->name);
                for (reason = reasonList->next;  reason != NULL;  reason = reason->next)
                    {
                    replaceChar(reason->name, '_', ' ');
                    dyStringPrintf(dy, ", %s", reason->name);
                    }
                dyStringAppendC(dy, ')');
                }
            printTooltip(dy->string);
            }
        printf("</td>");
        struct placementInfo *pi = hashFindVal(ur->samplePlacements, si->seq->name);
        if (pi)
            {
            if (gotClades)
                printf("<td>%s</td>", pi->nextClade ? pi->nextClade : "n/a");
            if (gotLineages)
                printf("<td>%s</td>", pi->pangoLineage ? pi->pangoLineage : "n/a");
            struct slName *neighbor = findNearestNeighbor(bigTree, pi->sampleId, pi->variantPath);
            char *lineage = neighbor ?  lineageForSample(sampleMetadata, neighbor->name) : "?";
            printf("<td>%s</td><td>%s</td>",
                   neighbor ? replaceChars(neighbor->name, "|", " | ") : "?",
                   lineage ? lineage : "?");
            int imputedCount = slCount(pi->imputedBases);
            printf("<td class='%s'>%d",
                   qcClassForImputedBases(imputedCount), imputedCount);
            if (imputedCount > 0)
                {
                dyStringClear(dy);
                struct baseVal *bv;
                for (bv = pi->imputedBases;  bv != NULL;  bv = bv->next)
                    {
                    dyStringAppendSep(dy, ", ");
                    dyStringPrintf(dy, "%d: %s", bv->chromStart+1, bv->val);
                    }
                printTooltip(dy->string);
                }
            printf("</td><td class='%s'>%d",
                   qcClassForPlacements(pi->bestNodeCount), pi->bestNodeCount);
            printf("</td><td class='%s'>%d",
                   qcClassForPScore(pi->parsimonyScore), pi->parsimonyScore);
            printf("</td>");
            }
        else
            {
            if (gotClades)
                printf("<td>n/a></td>");
            if (gotLineages)
                printf("<td>n/a></td>");
            printf("<td>n/a</td><td>n/a</td><td>n/a</td><td>n/a</td><td>n/a</td>");
            }
        int ix;
        struct subtreeInfo *ti = subtreeInfoForSample(ur->subtreeInfoList, si->seq->name, &ix);
        if (ix < 0)
            //#*** Probably an error.
            printf("<td>n/a</td>");
        else
            {
            printf("<td>%d", ix+1);
            if (ti && nextstrainHost())
                {
                char *nextstrainUrl = nextstrainUrlFromTn(jsonTns[ix]);
                printf(" (<a href='%s' target=_blank>view in Nextstrain<a>)", nextstrainUrl);
                }
            printf("</td>");
            }
        puts("</tr>");
        }
    puts("</tbody></table><p></p>");
    }
}

static struct singleNucChange *sncListFromSampleMutsAndImputed(struct slName *sampleMuts,
                                                               struct baseVal *imputedBases)
/* Convert a list of "<ref><pos><alt>" names to struct singleNucChange list.
 * However, if <alt> is ambiguous, skip it because variantProjector doesn't like it.
 * Add imputed base predictions. */
{
struct singleNucChange *sncList = NULL;
struct slName *mut;
for (mut = sampleMuts;  mut != NULL;  mut = mut->next)
    {
    char ref = mut->name[0];
    if (ref < 'A' || ref > 'Z')
        errAbort("sncListFromSampleMuts: expected ref base value, got '%c' in '%s'",
                 ref, mut->name);
    int pos = atoi(&(mut->name[1]));
    if (pos < 1 || pos > chromSize)
        errAbort("sncListFromSampleMuts: expected pos between 1 and %d, got %d in '%s'",
                 chromSize, pos, mut->name);
    char alt = mut->name[strlen(mut->name)-1];
    if (alt < 'A' || alt > 'Z')
        errAbort("sncListFromSampleMuts: expected alt base value, got '%c' in '%s'",
                 alt, mut->name);
    if (isIupacAmbiguous(alt))
        continue;
    struct singleNucChange *snc;
    AllocVar(snc);
    snc->chromStart = pos-1;
    snc->refBase = ref;
    snc->newBase = alt;
    slAddHead(&sncList, snc);
    }
struct baseVal *bv;
for (bv = imputedBases;  bv != NULL;  bv = bv->next)
    {
    struct singleNucChange *snc;
    AllocVar(snc);
    snc->chromStart = bv->chromStart;
    snc->refBase = '?';
    snc->newBase = bv->val[0];
    slAddHead(&sncList, snc);
    }
slReverse(&sncList);
return sncList;
}

struct aaMutInfo
// A protein change, who has it, and how important we think it is.
{
    char *name;                 // The name that people are used to seeing, e.g. "E484K', "N501Y"
    struct slName *sampleIds;   // The uploaded samples that have it
    int priority;               // For sorting; lower number means scarier.
    int pos;                    // 1-based position
    char oldAa;                 // Reference AA
    char newAa;                 // Alt AA
};

int aaMutInfoCmp(const void *a, const void *b)
/* Compare aaMutInfo priority for sorting. */
{
const struct aaMutInfo *amiA = *(struct aaMutInfo * const *)a;
const struct aaMutInfo *amiB = *(struct aaMutInfo * const *)b;
return amiA->priority - amiB->priority;
}

// For now, hardcode SARS-CoV-2 Spike RBD coords and antibody escape positions (1-based).
static int rbdStart = 319;
static int rbdEnd = 541;
static boolean *escapeMutPos = NULL;

static void initEscapeMutPos()
/* Allocate excapeMutPos and set positions implicated by at least a couple experiments from Bloom Lab
 * or that appear in Whelan, Rappuoli or McCoy tracks. */
{
AllocArray(escapeMutPos, rbdEnd);
escapeMutPos[332] = TRUE;
escapeMutPos[334] = TRUE;
escapeMutPos[335] = TRUE;
escapeMutPos[337] = TRUE;
escapeMutPos[339] = TRUE;
escapeMutPos[340] = TRUE;
escapeMutPos[345] = TRUE;
escapeMutPos[346] = TRUE;
escapeMutPos[348] = TRUE;
escapeMutPos[352] = TRUE;
escapeMutPos[357] = TRUE;
escapeMutPos[359] = TRUE;
escapeMutPos[361] = TRUE;
escapeMutPos[362] = TRUE;
escapeMutPos[363] = TRUE;
escapeMutPos[365] = TRUE;
escapeMutPos[366] = TRUE;
escapeMutPos[367] = TRUE;
escapeMutPos[369] = TRUE;
escapeMutPos[370] = TRUE;
escapeMutPos[371] = TRUE;
escapeMutPos[372] = TRUE;
escapeMutPos[373] = TRUE;
escapeMutPos[374] = TRUE;
escapeMutPos[376] = TRUE;
escapeMutPos[378] = TRUE;
escapeMutPos[383] = TRUE;
escapeMutPos[384] = TRUE;
escapeMutPos[385] = TRUE;
escapeMutPos[386] = TRUE;
escapeMutPos[408] = TRUE;
escapeMutPos[417] = TRUE;
escapeMutPos[441] = TRUE;
escapeMutPos[444] = TRUE;
escapeMutPos[445] = TRUE;
escapeMutPos[445] = TRUE;
escapeMutPos[447] = TRUE;
escapeMutPos[449] = TRUE;
escapeMutPos[450] = TRUE;
escapeMutPos[452] = TRUE;
escapeMutPos[455] = TRUE;
escapeMutPos[456] = TRUE;
escapeMutPos[458] = TRUE;
escapeMutPos[472] = TRUE;
escapeMutPos[473] = TRUE;
escapeMutPos[474] = TRUE;
escapeMutPos[476] = TRUE;
escapeMutPos[477] = TRUE;
escapeMutPos[478] = TRUE;
escapeMutPos[479] = TRUE;
escapeMutPos[483] = TRUE;
escapeMutPos[484] = TRUE;
escapeMutPos[485] = TRUE;
escapeMutPos[486] = TRUE;
escapeMutPos[487] = TRUE;
escapeMutPos[489] = TRUE;
escapeMutPos[490] = TRUE;
escapeMutPos[494] = TRUE;
escapeMutPos[499] = TRUE;
escapeMutPos[504] = TRUE;
escapeMutPos[514] = TRUE;
escapeMutPos[517] = TRUE;
escapeMutPos[522] = TRUE;
escapeMutPos[525] = TRUE;
escapeMutPos[526] = TRUE;
escapeMutPos[527] = TRUE;
escapeMutPos[528] = TRUE;
escapeMutPos[529] = TRUE;
escapeMutPos[530] = TRUE;
escapeMutPos[531] = TRUE;
}

static int spikePriority(int pos, char newAa)
/* Lower number for scarier spike mutation, per spike mutations track / RBD. */
{
if (escapeMutPos == NULL)
    initEscapeMutPos();
int priority = 0;
if (pos >= rbdStart && pos <= rbdEnd)
    {
    // Receptor binding domain
    priority = 100;
    // Antibody escape mutation in Variant of Concern/Interest
    if (pos == 484)
        priority = 10;
    else if (pos == 501)
        priority = 15;
    else if (pos == 452)
        priority = 20;
    // Other antibody escape mutations
    else if (pos == 439 || pos == 477)
        priority = 25;
    else if (escapeMutPos[pos])
        priority = 50;
    }
else if (pos >= 675 && pos <= 681)
    // Furin cleavage site; circumstantial evidence for Q677{H,P} spread in US.
    // Interesting threads on SPHERES 2021-02-26 about P681H tradeoff between infectivity vs
    // range of cell types that can be infected and other observations about that region.
    priority = 110;
else if (pos == 614 && newAa == 'G')
    // Old hat
    priority = 1000;
else
    // Somewhere else in Spike
    priority = 500;
return priority;
}

static void addSpikeChange(struct hash *spikeChanges, char *aaMutStr, char *sampleId)
/* Tally up an observance of a S gene change in a sample. */
{
// Parse oldAa, pos, newAA out of aaMutStr.  Need a more elegant way of doing this;
// this is a rush job to get something usable out there asap.
char oldAa = aaMutStr[0];
int pos = atoi(aaMutStr+1);
char newAa = aaMutStr[strlen(aaMutStr)-1];
struct hashEl *hel = hashLookup(spikeChanges, aaMutStr);
if (hel == NULL)
    {
    struct aaMutInfo *ami;
    AllocVar(ami);
    ami->name = cloneString(aaMutStr);
    slNameAddHead(&ami->sampleIds, sampleId);
    ami->priority = spikePriority(pos, newAa);
    ami->pos = pos;
    ami->oldAa = oldAa;
    ami->newAa = newAa;
    hashAdd(spikeChanges, aaMutStr, ami);
    }
else
    {
    struct aaMutInfo *ami = hel->val;
    slNameAddHead(&ami->sampleIds, sampleId);
    }
}

static void writeOneTsvRow(FILE *f, char *sampleId, struct usherResults *results,
                           struct hash *seqInfoHash, struct geneInfo *geneInfoList,
                           struct seqWindow *gSeqWin, struct hash *spikeChanges)
/* Write one row of tab-separate summary for sampleId.  Accumulate S gene AA change info. */
{
    struct placementInfo *info = hashFindVal(results->samplePlacements, sampleId);
    // sample name / ID
    fprintf(f, "%s\t", sampleId);
    // nucleotide mutations
    struct slName *mut;
    for (mut = info->sampleMuts;  mut != NULL;  mut = mut->next)
        {
        if (mut != info->sampleMuts)
            fputc(',', f);
        fputs(mut->name, f);
        }
    fputc('\t', f);
    // AA mutations
    struct singleNucChange *sncList = sncListFromSampleMutsAndImputed(info->sampleMuts,
                                                                      info->imputedBases);
    struct slPair *geneAaMutations = getAaMutations(sncList, geneInfoList, gSeqWin);
    struct slPair *geneAaMut;
    boolean first = TRUE;
    for (geneAaMut = geneAaMutations;  geneAaMut != NULL;  geneAaMut = geneAaMut->next)
        {
        struct slName *aaMut;
        for (aaMut = geneAaMut->val;  aaMut != NULL;  aaMut = aaMut->next)
            {
            if (first)
                first = FALSE;
            else
                fputc(',', f);
            fprintf(f, "%s:%s", geneAaMut->name, aaMut->name);
            if (sameString(geneAaMut->name, "S"))
                addSpikeChange(spikeChanges, aaMut->name, sampleId);
            }
        }
    fputc('\t', f);
    // imputed bases (if any)
    struct baseVal *bv;
    for (bv = info->imputedBases;  bv != NULL;  bv = bv->next)
        {
        if (bv != info->imputedBases)
            fputc(',', f);
        fprintf(f, "%d%s", bv->chromStart+1, bv->val);
        }
    fputc('\t', f);
    // path through tree to sample
    printVariantPathNoNodeNames(f, info->variantPath);
    // number of equally parsimonious placements
    fprintf(f, "\t%d", info->bestNodeCount);
    // parsimony score
    fprintf(f, "\t%d", info->parsimonyScore);
    struct seqInfo *si = hashFindVal(seqInfoHash, sampleId);
    if (si)
        {
        if (si->psl)
            {
            // length
            fprintf(f, "\t%d", si->seq->size);
            struct psl *psl = si->psl;
            // aligned bases, indel counts & ranges
            int aliCount = psl->match + psl->misMatch + psl->repMatch;
            fprintf(f, "\t%d\t%d\t%s\t%d\t%s",
                    aliCount, si->insBases, emptyForNull(si->insRanges),
                    si->delBases, emptyForNull(si->delRanges));
            }
        else
            fprintf(f, "\tn/a\tn/a\tn/a\tn/a\tn/a\tn/a");
        // SNVs that were masked (Problematic Sites track), not used in placement
        fputc('\t', f);
        struct singleNucChange *snc;
        for (snc = si->maskedSncList;  snc != NULL;  snc = snc->next)
            {
            if (snc != si->maskedSncList)
                fputc(',', f);
            fprintf(f, "%c%d%c", snc->refBase, snc->chromStart+1, snc->newBase);
            }
        }
    else
        {
        warn("writeOneTsvRow: no sequenceInfo for sample '%s'", sampleId);
        fprintf(f, "\tn/a\tn/a\tn/a\tn/a\tn/a\tn/a\tn/a");
        }
    fputc('\n', f);
}

static void rWriteTsvSummaryTreeOrder(struct phyloTree *node, FILE *f, struct usherResults *results,
                                      struct hash *seqInfoHash, struct geneInfo *geneInfoList,
                                      struct seqWindow *gSeqWin, struct hash *spikeChanges)
/* As we encounter leaves (user-uploaded samples) in depth-first search order, write out a line
 * of TSV summary for each one. */
{
if (node->numEdges)
    {
    int i;
    for (i = 0;  i < node->numEdges;  i++)
        rWriteTsvSummaryTreeOrder(node->edges[i], f, results, seqInfoHash, geneInfoList, gSeqWin,
                                  spikeChanges);
    }
else
    {
    writeOneTsvRow(f, node->ident->name, results, seqInfoHash, geneInfoList, gSeqWin, spikeChanges);
    }
}


static struct hash *hashFromSeqInfoList(struct seqInfo *seqInfoList)
/* Hash sequence name to seqInfo for quick lookup. */
{
struct hash *hash = hashNew(0);
struct seqInfo *si;
for (si = seqInfoList;  si != NULL;  si = si->next)
    hashAdd(hash, si->seq->name, si);
return hash;
}

static struct tempName *writeTsvSummary(struct usherResults *results, struct phyloTree *sampleTree,
                                        struct slName *sampleIds, struct seqInfo *seqInfoList,
                                        struct geneInfo *geneInfoList, struct seqWindow *gSeqWin,
                                        struct hash *spikeChanges, int *pStartTime)
/* Write a tab-separated summary file for download.  If the user uploaded enough samples to make
 * a tree, then write out samples in tree order; otherwise use sampleIds list.
 * Accumulate S gene changes. */
{
struct tempName *tsvTn = NULL;
AllocVar(tsvTn);
trashDirFile(tsvTn, "ct", "usher_samples", ".tsv");
FILE *f = mustOpen(tsvTn->forCgi, "w");
fprintf(f, "name\tnuc_mutations\taa_mutations\timputed_bases\tmutation_path"
        "\tplacement_count\tparsimony_score_increase\tlength\taligned_bases"
        "\tins_bases\tins_ranges\tdel_bases\tdel_ranges\tmasked_mutations"
        "\n");
struct hash *seqInfoHash = hashFromSeqInfoList(seqInfoList);
if (sampleTree)
    {
    rWriteTsvSummaryTreeOrder(sampleTree, f, results, seqInfoHash, geneInfoList, gSeqWin,
                              spikeChanges);
    }
else
    {
    struct slName *sample;
    for (sample = sampleIds;  sample != NULL;  sample = sample->next)
        writeOneTsvRow(f, sample->name, results, seqInfoHash, geneInfoList, gSeqWin, spikeChanges);
    }
carefulClose(&f);
hashFree(&seqInfoHash);
reportTiming(pStartTime, "write tsv summary");
return tsvTn;
}

static struct tempName *writeSpikeChangeSummary(struct hash *spikeChanges, int totalSampleCount)
/* Write a tab-separated summary of S (Spike) gene changes for download. */
{
struct tempName *tsvTn = NULL;
AllocVar(tsvTn);
trashDirFile(tsvTn, "ct", "usher_S_muts", ".tsv");
FILE *f = mustOpen(tsvTn->forCgi, "w");
fprintf(f, "aa_mutation\tsample_count\tsample_frequency\tsample_ids"
        "\n");
struct aaMutInfo *sChanges[spikeChanges->elCount];
struct hashCookie cookie = hashFirst(spikeChanges);
int ix = 0;
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    {
    if (ix >= spikeChanges->elCount)
        errAbort("writeSpikeChangeSummary: hash elCount is %d but got more elements",
                 spikeChanges->elCount);
    sChanges[ix++] = hel->val;
    }
if (ix != spikeChanges->elCount)
    errAbort("writeSpikeChangeSummary: hash elCount is %d but got fewer elements (%d)",
             spikeChanges->elCount, ix);
qsort(sChanges, ix, sizeof(sChanges[0]), aaMutInfoCmp);
for (ix = 0;  ix < spikeChanges->elCount;  ix++)
    {
    struct aaMutInfo *ami = sChanges[ix];
    int sampleCount = slCount(ami->sampleIds);
    fprintf(f, "S:%s\t%d\t%f",
            ami->name, sampleCount, (double)sampleCount / (double)totalSampleCount);
    slReverse(&ami->sampleIds);
    fprintf(f, "\t%s", ami->sampleIds->name);
    struct slName *sample;
    for (sample = ami->sampleIds->next;  sample != NULL;  sample = sample->next)
        fprintf(f, ",%s", sample->name);
    fputc('\n', f);
    }
carefulClose(&f);
return tsvTn;
}

static struct tempName *makeSubtreeZipFile(struct usherResults *results, struct tempName *jsonTns[],
                                           struct tempName *singleSubtreeJsonTn, int *pStartTime)
/* Make a zip archive file containing all of the little subtree Newick and JSON files so
 * user doesn't have to click on each one. */
{
struct tempName *zipTn;
AllocVar(zipTn);
trashDirFile(zipTn, "ct", "usher_subtrees", ".zip");
int subtreeCount = slCount(results->subtreeInfoList);
char *cmd[10 + 2*(subtreeCount+1)];
char **cmds[] = { cmd, NULL };
int cIx = 0, sIx = 0;
cmd[cIx++] = "zip";
cmd[cIx++] = "-j";
cmd[cIx++] = zipTn->forCgi;
cmd[cIx++] = singleSubtreeJsonTn->forCgi;
cmd[cIx++] = results->singleSubtreeInfo->subtreeTn->forCgi;
struct subtreeInfo *ti;
for (ti = results->subtreeInfoList;  ti != NULL;  ti = ti->next, sIx++)
    {
    cmd[cIx++] = jsonTns[sIx]->forCgi;
    cmd[cIx++] = ti->subtreeTn->forCgi;
    }
cmd[cIx++] = NULL;
struct pipeline *pl = pipelineOpen(cmds, pipelineRead, NULL, NULL);
pipelineClose(&pl);
reportTiming(pStartTime, "make subtree zipfile");
return zipTn;
}

static struct slName **getProblematicSites(char *db)
/* If config.ra specfies maskFile them return array of lists (usually NULL) of reasons that
 * masking is recommended, one per position in genome; otherwise return NULL. */
{
struct slName **pSites = NULL;
char *pSitesFile = phyloPlaceDbSettingPath(db, "maskFile");
if (isNotEmpty(pSitesFile) && fileExists(pSitesFile))
    {
    AllocArray(pSites, chromSize);
    struct bbiFile *bbi = bigBedFileOpen(pSitesFile);
    struct lm *lm = lmInit(0);
    struct bigBedInterval *bb, *bbList = bigBedIntervalQuery(bbi, chrom, 0, chromSize, 0, lm);
    for (bb = bbList;  bb != NULL;  bb = bb->next)
        {
        char *extra = bb->rest;
        char *reason = nextWord(&extra);
        int i;
        for (i = bb->start;  i < bb->end;  i++)
            slNameAddHead(&pSites[i], reason);
        }
    bigBedFileClose(&bbi);
    }
return pSites;
}

static void downloadsRow(char *treeFile, char *sampleSummaryFile, char *spikeSummaryFile,
                         char *subtreeZipFile)
/* Make a row of quick download file links, to appear between the button row & big summary table. */
{
printf("<p><b>Downloads:</b> | ");
printf("<a href='%s' download>Global phylogenetic tree with your sequences</a> | ", treeFile);
printf("<a href='%s' download>TSV summary of sequences and placements</a> | ", sampleSummaryFile);
printf("<a href='%s' download>TSV summary of Spike mutations</a> | ", spikeSummaryFile);
printf("<a href='%s' download>ZIP file of subtree JSON and Newick files</a> | ", subtreeZipFile);
puts("</p>");
}

static int subTreeInfoUserSampleCmp(const void *pa, const void *pb)
/* Compare subtreeInfo by number of user sample IDs (highest number first). */
{
struct subtreeInfo *tiA = *(struct subtreeInfo **)pa;
struct subtreeInfo *tiB = *(struct subtreeInfo **)pb;
return slCount(tiB->subtreeUserSampleIds) - slCount(tiA->subtreeUserSampleIds);
}

char *phyloPlaceSamples(struct lineFile *lf, char *db, char *defaultProtobuf,
                        boolean doMeasureTiming, int subtreeSize, int fontHeight)
/* Given a lineFile that contains either FASTA or VCF, prepare VCF for usher;
 * if that goes well then run usher, report results, make custom track files
 * and return the top-level custom track file; otherwise return NULL. */
{
char *ctFile = NULL;
measureTiming = doMeasureTiming;
int startTime = clock1000();
struct tempName *vcfTn = NULL;
struct slName *sampleIds = NULL;
char *usherPath = getUsherPath(TRUE);
char *usherAssignmentsPath = NULL;
char *source = NULL;
char *metadataFile = NULL;
struct treeChoices *treeChoices = loadTreeChoices(db);
if (treeChoices)
    {
    usherAssignmentsPath = defaultProtobuf;
    if (isEmpty(usherAssignmentsPath))
        usherAssignmentsPath = treeChoices->protobufFiles[0];
    int i;
    for (i = 0;  i < treeChoices->count;  i++)
        if (sameString(treeChoices->protobufFiles[i], usherAssignmentsPath))
            {
            metadataFile = treeChoices->metadataFiles[i];
            source = treeChoices->sources[i];
            break;
            }
    if (i == treeChoices->count)
        {
        usherAssignmentsPath = treeChoices->protobufFiles[0];
        metadataFile = treeChoices->metadataFiles[0];
        source = treeChoices->sources[0];
        }
    }
else
    {
    // Fall back on old settings
    usherAssignmentsPath = getUsherAssignmentsPath(db, TRUE);
    metadataFile = phyloPlaceDbSettingPath(db, "metadataFile");
    source = "GISAID";
    }
struct mutationAnnotatedTree *bigTree = parseParsimonyProtobuf(usherAssignmentsPath);
reportTiming(&startTime, "parse protobuf file");
if (! bigTree)
    {
    warn("Problem parsing %s; can't make subtree subtracks.", usherAssignmentsPath);
    }
lineFileCarefulNewlines(lf);
struct slName **maskSites = getProblematicSites(db);
struct dnaSeq *refGenome = hChromSeq(db, chrom, 0, chromSize);
boolean isFasta = FALSE;
struct seqInfo *seqInfoList = NULL;
if (lfLooksLikeFasta(lf))
    {
    boolean *informativeBases = informativeBasesFromTree(bigTree->tree, maskSites);
    struct slPair *failedSeqs;
    struct slPair *failedPsls;
    vcfTn = vcfFromFasta(lf, db, refGenome, informativeBases, maskSites,
                         &sampleIds, &seqInfoList, &failedSeqs, &failedPsls, &startTime);
    if (failedSeqs)
        {
        puts("<p>");
        struct slPair *fail;
        for (fail = failedSeqs;  fail != NULL;  fail = fail->next)
            printf("%s<br>\n", fail->name);
        puts("</p>");
        }
    if (failedPsls)
        {
        puts("<p>");
        struct slPair *fail;
        for (fail = failedPsls;  fail != NULL;  fail = fail->next)
            printf("%s<br>\n", fail->name);
        puts("</p>");
        }
    if (seqInfoList == NULL)
        printf("<p>Sorry, could not align any sequences to reference well enough to place in "
               "the phylogenetic tree.</p>\n");
    isFasta = TRUE;
    }
else if (lfLooksLikeVcf(lf))
    {
    vcfTn = checkAndSaveVcf(lf, refGenome, maskSites, &seqInfoList, &sampleIds);
    reportTiming(&startTime, "check uploaded VCF");
    }
else
    {
    if (isNotEmpty(lf->fileName))
        warn("Sorry, can't recognize your file %s as FASTA or VCF.\n", lf->fileName);
    else
        warn("Sorry, can't recognize your uploaded data as FASTA or VCF.\n");
    }
lineFileClose(&lf);
if (vcfTn)
    {
    fflush(stdout);
    int seqCount = slCount(seqInfoList);
    // Don't make smaller subtrees when a large number of sequences are uploaded.
    if (seqCount > MAX_SEQ_DETAILS)
        subtreeSize = 0;
    struct usherResults *results = runUsher(usherPath, usherAssignmentsPath, vcfTn->forCgi,
                                            subtreeSize, sampleIds, bigTree->condensedNodes,
                                            &startTime);
    if (results->singleSubtreeInfo)
        {
        readQcThresholds(db);
        int subtreeCount = slCount(results->subtreeInfoList);
        // Sort subtrees by number of user samples (largest first).
        slSort(&results->subtreeInfoList, subTreeInfoUserSampleCmp);
        // Make Nextstrain/auspice JSON file for each subtree.
        char *bigGenePredFile = phyloPlaceDbSettingPath(db, "bigGenePredFile");
        struct geneInfo *geneInfoList = getGeneInfoList(bigGenePredFile, refGenome);
        struct seqWindow *gSeqWin = chromSeqWindowNew(db, chrom, 0, chromSize);
        struct hash *sampleMetadata = getSampleMetadata(metadataFile);
        struct tempName *singleSubtreeJsonTn;
        AllocVar(singleSubtreeJsonTn);
        trashDirFile(singleSubtreeJsonTn, "ct", "singleSubtreeAuspice", ".json");
        treeToAuspiceJson(results->singleSubtreeInfo, db, geneInfoList, gSeqWin, sampleMetadata,
                          singleSubtreeJsonTn->forCgi, source);
        struct tempName *jsonTns[subtreeCount];
        struct subtreeInfo *ti;
        int ix;
        for (ix = 0, ti = results->subtreeInfoList;  ti != NULL;  ti = ti->next, ix++)
            {
            AllocVar(jsonTns[ix]);
            char subtreeName[512];
            safef(subtreeName, sizeof(subtreeName), "subtreeAuspice%d", ix+1);
            trashDirFile(jsonTns[ix], "ct", subtreeName, ".json");
            treeToAuspiceJson(ti, db, geneInfoList, gSeqWin, sampleMetadata, jsonTns[ix]->forCgi,
                              source);
            }
        puts("<p></p>");
        int subtreeButtonCount = subtreeCount;
        if (seqCount > MAX_SEQ_DETAILS || subtreeCount > MAX_SUBTREE_BUTTONS)
            subtreeButtonCount = 0;
        makeButtonRow(singleSubtreeJsonTn, jsonTns, subtreeButtonCount, isFasta);
        printf("<p>If you have metadata you wish to display, click a 'view subtree in "
               "Nextstrain' button, and then you can drag on a CSV file to "
               "<a href='"NEXTSTRAIN_DRAG_DROP_DOC"' target=_blank>add it to the tree view</a>."
               "</p>\n");

        // Make custom tracks for uploaded samples and subtree(s).
        struct phyloTree *sampleTree = NULL;
        struct tempName *ctTn = writeCustomTracks(vcfTn, results, sampleIds, bigTree->tree,
                                                  source, fontHeight, &sampleTree, &startTime);

        // Make a sample summary TSV file and accumulate S gene changes
        struct hash *spikeChanges = hashNew(0);
        struct tempName *tsvTn = writeTsvSummary(results, sampleTree, sampleIds, seqInfoList,
                                                 geneInfoList, gSeqWin, spikeChanges, &startTime);
        struct tempName *sTsvTn = writeSpikeChangeSummary(spikeChanges, slCount(sampleIds));
        struct tempName *zipTn = makeSubtreeZipFile(results, jsonTns, singleSubtreeJsonTn,
                                                    &startTime);
        downloadsRow(results->bigTreePlusTn->forHtml, tsvTn->forHtml, sTsvTn->forHtml,
                     zipTn->forHtml);

        if (seqCount <= MAX_SEQ_DETAILS)
            {
            summarizeSequences(seqInfoList, isFasta, results, jsonTns, sampleMetadata, bigTree,
                               refGenome);
            reportTiming(&startTime, "write summary table (including reading in lineages)");
            for (ix = 0, ti = results->subtreeInfoList;  ti != NULL;  ti = ti->next, ix++)
                {
                int subtreeUserSampleCount = slCount(ti->subtreeUserSampleIds);
                printf("<h3>Subtree %d: ", ix+1);
                if (subtreeUserSampleCount > 1)
                    printf("%d related samples", subtreeUserSampleCount);
                else if (subtreeCount > 1)
                    printf("Unrelated sample");
                printf("</h3>\n");
                makeNextstrainButtonN("viewNextstrainSub", ix, jsonTns);
                puts("<br>");
                // Make a sub-subtree with only user samples for display:
                struct phyloTree *subtree = phyloOpenTree(ti->subtreeTn->forCgi);
                subtree = phyloPruneToIds(subtree, ti->subtreeUserSampleIds);
                describeSamplePlacements(ti->subtreeUserSampleIds, results->samplePlacements,
                                         subtree, sampleMetadata, bigTree, source);
                }
            reportTiming(&startTime, "describe placements");
            }
        else
            printf("<p>(Skipping details and subtrees; "
                   "you uploaded %d sequences, and details/subtrees are shown only when "
                   "you upload at most %d sequences.)</p>\n",
                   seqCount, MAX_SEQ_DETAILS);

        // Offer big tree w/new samples for download
        puts("<h3>Downloads</h3>");
        puts("<ul>");
        printf("<li><a href='%s' download>SARS-CoV-2 phylogenetic tree "
               "with your samples (Newick file)</a>\n", results->bigTreePlusTn->forHtml);
        printf("<li><a href='%s' download>TSV summary of sequences and placements</a>\n",
               tsvTn->forHtml);
        printf("<li><a href='%s' download>TSV summary of S (Spike) gene changes</a>\n",
               sTsvTn->forHtml);
        printf("<li><a href='%s' download>ZIP archive of subtree Newick and JSON files</a>\n",
               zipTn->forHtml);
        // For now, leave in the individual links so I don't break anybody's pipeline that's
        // scraping this page...
        for (ix = 0, ti = results->subtreeInfoList;  ti != NULL;  ti = ti->next, ix++)
            {
            int subtreeUserSampleCount = slCount(ti->subtreeUserSampleIds);
            printf("<li><a href='%s' download>Subtree with %s", ti->subtreeTn->forHtml,
                   ti->subtreeUserSampleIds->name);
            if (subtreeUserSampleCount > 10)
                printf(" and %d other samples", subtreeUserSampleCount - 1);
            else
                {
                struct slName *sln;
                for (sln = ti->subtreeUserSampleIds->next;  sln != NULL;  sln = sln->next)
                    printf(", %s", sln->name);
                }
            puts(" (Newick file)</a>");
            printf("<li><a href='%s' download>Auspice JSON for subtree with %s",
                   jsonTns[ix]->forHtml, ti->subtreeUserSampleIds->name);
            if (subtreeUserSampleCount > 10)
                printf(" and %d other samples", subtreeUserSampleCount - 1);
            else
                {
                struct slName *sln;
                for (sln = ti->subtreeUserSampleIds->next;  sln != NULL;  sln = sln->next)
                    printf(", %s", sln->name);
                }
            puts(" (JSON file)</a>");
            }
        puts("</ul>");

        // Notify in opposite order of custom track creation.
        puts("<h3>Custom tracks for viewing in the Genome Browser</h3>");
        printf("<p>Added custom track of uploaded samples.</p>\n");
        if (subtreeCount > 0 && subtreeCount <= MAX_SUBTREE_CTS)
            printf("<p>Added %d subtree custom track%s.</p>\n",
                   subtreeCount, (subtreeCount > 1 ? "s" : ""));
        ctFile = urlFromTn(ctTn);
        }
    else
        {
        warn("No subtree output from usher.\n");
        }
    }
return ctFile;
}
