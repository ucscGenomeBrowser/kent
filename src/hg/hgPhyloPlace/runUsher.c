/* Invoke usher to place user's uploaded samples in the phylogenetic tree & parse output files. */

/* Copyright (C) 2020-2024 The Regents of the University of California */

#include "common.h"
#include "dnautil.h"
#include "hash.h"
#include "hgConfig.h"
#include "linefile.h"
#include "obscure.h"
#include "parsimonyProto.h"
#include "phyloPlace.h"
#include "regexHelper.h"
#include "pipeline.h"
#include "trackHub.h"
#include "trashDir.h"
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

// Keywords in stderr output of usher:
#define sampleIdPrefix "Sample name:"
#define pScorePrefix "Parsimony score:"
#define numPlacementsPrefix "Number of parsimony-optimal placements:"
#define imputedMutsPrefix "Imputed mutations:"

static void parseSampleIdAndParsimonyScore(char **words, char **retSampleId,
                                           struct hash *samplePlacements)
/* If words[] seems to contain columns of the line that gives sample ID and parsimony score,
 * then parse out those values. */
{
// Example line:
// words[0] = Current tree size (#nodes): 70775
// words[1] = Sample name: MyLabSequence2
// words[2] = Parsimony score: 1
// words[3] = Number of parsimony-optimal placements: 1
char *p = stringIn(sampleIdPrefix, words[1]);
if (p)
    {
    *retSampleId = cloneString(trimSpaces(p + strlen(sampleIdPrefix)));
    struct placementInfo *info;
    AllocVar(info);
    hashAdd(samplePlacements, *retSampleId, info);
    info->sampleId = *retSampleId;
    p = stringIn(pScorePrefix, words[2]);
    if (p)
        info->parsimonyScore = atoi(skipToSpaces(p + strlen(pScorePrefix)));
    else
        errAbort("Problem parsing stderr output of usher: "
                 "expected '" sampleIdPrefix "...' to be followed by '"
                 pScorePrefix "...' but could not find the latter." );
    p = stringIn(numPlacementsPrefix, words[3]);
    if (p)
        info->bestNodeCount = atoi(skipToSpaces(p + strlen(numPlacementsPrefix)));
    else
        errAbort("Problem parsing stderr output of usher: "
                 "expected '" sampleIdPrefix "... " pScorePrefix " ...' to be followed by '"
                 numPlacementsPrefix "...' but could not find the latter." );
    }
else
    errAbort("Unexpected format of sample ID line:\n%s\t%s\t%s\t%s",
             words[0], words[1], words[2], words[3]);
}

static struct singleNucChange *parseSnc(char *sncStr)
/* If sncStr looks like a <old><pos><new>-style single nucleotide change then parse out those
 * values & return singleNucChange (with parBase and newBase; no refBase), otherwise return NULL.  */
{
struct singleNucChange *snc = NULL;
regmatch_t substrs[4];
if (regexMatchSubstr(sncStr, "^([ACGT])([0-9]+)([ACGT])$", substrs, ArraySize(substrs)))
    {
    int chromStart = regexSubstringInt(sncStr, substrs[2]) - 1;
    snc = sncNew(chromStart, '\0', sncStr[0], sncStr[substrs[3].rm_so]);
    }
return snc;
}

static struct baseVal *bvListFromSemiColonSep(char *mutStr)
/* Parse a string of ;-sep'd position:allele & return a list of struct baseVal. */
{
char *muts[strlen(mutStr) / 4];
int mutCount = chopString(mutStr, ";", muts, ArraySize(muts));
struct baseVal *bvList = NULL;
int i;
for (i = 0;  i < mutCount;  i++)
    {
    boolean problem = FALSE;
    char *colon = strchr(muts[i], ':');
    if (colon)
        {
        int pos = atoi(muts[i]);
        char *val = cloneString(colon+1);
        if (pos < 1)
            problem = TRUE;
        else if (!isAllNt(val, strlen(val)))
            problem = TRUE;
        else
            {
            struct baseVal *bv;
            AllocVar(bv);
            bv->chromStart = pos - 1;
            bv->val = val;
            slAddHead(&bvList, bv);
            }
        }
    if (problem)
        errAbort("Problem parsing stderr output of usher: "
                 "expected imputed mutation to be number:base, but got '%s'", muts[i]);
    }
slReverse(&bvList);
return bvList;
}

static boolean parseImputedMutations(char **words, struct placementInfo *info)
/* If words[] looks like it defines imputed mutations of the most recently named sample,
 * then parse out the list and add to info->imputedBases and return TRUE. */
{
// Example line:
// words[0] = Imputed mutations: 
// words[1] = 6709:A;23403:G
boolean matches = FALSE;
if (stringIn(imputedMutsPrefix, words[0]))
    {
    matches = TRUE;
    info->imputedBases = bvListFromSemiColonSep(words[1]);
    }
return matches;
}

static void parseStderr(char *amsStderrFile, struct hash *samplePlacements)
/* The stderr output of usher is where we find important info for each sample:
 * the path of variants on nodes from root to sample leaf, imputed values of ambiguous bases
 * (if any), and parsimony score. */
{
struct lineFile *lf = lineFileOpen(amsStderrFile, TRUE);
char *sampleId = NULL;
int size;
char *line;
while (lineFileNext(lf, &line, &size))
    {
    char lineCpy[size+1];
    safencpy(lineCpy, sizeof lineCpy, line, size);
    char *words[16];
    int wordCount = chopTabs(lineCpy, words);
    if (wordCount == 4)
        parseSampleIdAndParsimonyScore(words, &sampleId, samplePlacements);
    else if (wordCount == 2)
        {
        if (! sampleId)
            errAbort("Problem parsing stderr output of usher: "
                     "Got line starting with '%s' that was not preceded by a line that "
                     "defines sample ID.:\n%s", words[0], line);
        struct placementInfo *info = hashFindVal(samplePlacements, sampleId);
        if (!info)
            errAbort("Problem parsing stderr output of usher: "
                     "Can't find placement info for sample '%s'", sampleId);
        parseImputedMutations(words, info);
        }
    }
}

static void parsePlacements(char *outDirName, char *stderrName, struct hash *samplePlacements,
                            struct slName **pSampleIds)
/* If usher created the file outdir/placement_stats.tsv then parse its contents into
 * samplePlacements; otherwise parse the same info out of the stderr output.
 * Check placements vs. *pSampleIds; if usher renamed a sample with prefix to avoid clash with
 * sample already in the tree then rename the sample in *pSampleIds. */
{
char placementsFileName[PATH_LEN];
safef(placementsFileName, sizeof placementsFileName, "%s/placement_stats.tsv", outDirName);
int placementCount = 0;
if (fileExists(placementsFileName))
    {
    // Frustratingly, the file from server can exist but appear to be empty for a couple seconds!
    // Worse, it might have a multiple-of-4096 size yet still be incomplete!
    // If we don't get this file's data, then we'll run into errors when parsing subsequent files.
    // So wait a while if the size seems suspicious.
    int maxTries = 30;
    int nx = 0;
    long long size = 0;
    while ((size = fileSize(placementsFileName)) % 4096 == 0 && nx < maxTries)
        {
        fprintf(stderr, "parsePlacements: after %ds, %s has multiple-of-4096 size %lld.\n",
                nx, placementsFileName, size);
        sleep(1);
        nx++;
        }
    if ((size % 4096) == 0)
        {
        fprintf(stderr, "parsePlacements: %s still has multiple-of-4096 size %lld but we've waited "
                "a long time (%ds), proceed.\n",
                placementsFileName, size, nx);
        }
    else if (nx > 0)
        fprintf(stderr, "parsePlacements: after %ds, %s size is %lld, getting started.\n",
                nx, placementsFileName, size);
    struct lineFile *lf = lineFileOpen(placementsFileName, TRUE);
    char *line;
    while (lineFileNext(lf, &line, NULL))
        {
        char *words[5];
        int wordCount = chopTabs(line, words);
        lineFileExpectAtLeast(lf, 4, wordCount);
        char *sampleId = words[0];
        struct placementInfo *info;
        AllocVar(info);
        hashAdd(samplePlacements, sampleId, info);
        info->sampleId = cloneString(sampleId);
        info->parsimonyScore = atoi(words[1]);
        info->bestNodeCount = atoi(words[2]);
        info->imputedBases = bvListFromSemiColonSep(words[3]);
        placementCount++;
        }
    lineFileClose(&lf);
    }
else
    {
    fprintf(stderr, "parsePlacements: No %s, reading stderr %s\n", placementsFileName, stderrName);
    parseStderr(stderrName, samplePlacements);
    }
// Check whether we placed all samples, and whether any sample names have the USHER_DEDUP_PREFIX
// in usher results files.
struct slName *newSampleIds = NULL;
struct slName *sample, *next = NULL;
for (sample = *pSampleIds;  sample != NULL;  sample = next)
    {
    next = sample->next;
    if (!hashLookup(samplePlacements, sample->name))
        {
        // Usher might have added a prefix to distinguish from a sequence with the same name
        // already in the tree.
        char nameWithPrefix[strlen(USHER_DEDUP_PREFIX) + strlen(sample->name) + 1];
        safef(nameWithPrefix, sizeof nameWithPrefix, "%s%s", USHER_DEDUP_PREFIX, sample->name);
        if (!hashLookup(samplePlacements, nameWithPrefix))
            {
            warn("parsePlacements: did not find placement for sample '%s'", sample->name);
            slAddHead(&newSampleIds, sample);
            }
        else
            {
            warn("Renamed %s to %s to distinguish it from sequence already in the tree",
                 sample->name, nameWithPrefix);
            slNameAddHead(&newSampleIds, nameWithPrefix);
            }
        }
    else
        slAddHead(&newSampleIds, sample);
    }
slReverse(&newSampleIds);
*pSampleIds = newSampleIds;
}

static void parseVariantPaths(char *filename, struct hash *samplePlacements)
/* Parse out space-sep list of {node ID, ':', node-associated ,-sep variant list} into
 * variantPathNode list and associate with sample ID. */
{
// Example line (note the back-mutation at 28144T... may want to highlight those):
// words[0] = MySeq
// words[1] = 1:C8782T,T28144C 2309:C29095T 2340:T8782C 2342:T29095C 2588:C28144T MySeq:C29867T 
struct lineFile *lf = lineFileOpen(filename, TRUE);
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    char *words[3];
    int wordCount = chopTabs(line, words);
    lineFileExpectWords(lf, 2, wordCount);
    char *sampleId = words[0];
    char *nodePath = words[1];
    char *nodes[strlen(nodePath) / 4];
    int nodeCount = chopString(nodePath, " ", nodes, ArraySize(nodes));
    struct variantPathNode *vpNodeList = NULL;
    int i;
    for (i = 0;  i < nodeCount;  i++)
        {
        struct variantPathNode *vpn;
        AllocVar(vpn);
        char *nodeVariants = nodes[i];
        // First there is a node ID followed by ':'.  If node ID is numeric, ignore;
        // otherwise it is a sample ID, indicating a mutation specific to the sample ID,
        // so include it.
        char *colon = strrchr(nodeVariants, ':');
        if (colon)
            {
            *colon = '\0';
            char *nodeName = nodeVariants;
            nodeVariants = colon+1;
            vpn->nodeName = cloneString(nodeName);
            }
        // Next there should be a comma-sep list of mutations/variants.
        char *variants[strlen(nodeVariants) / 4];
        int varCount = chopCommas(nodeVariants, variants);
        int j;
        for (j = 0;  j < varCount;  j++)
            {
            variants[j] = trimSpaces(variants[j]);
            struct singleNucChange *snc = parseSnc(variants[j]);
            if (snc)
                slAddHead(&vpn->sncList, snc);
            else
                errAbort("parseVariantPath: Expected variant path for %s to specify "
                         "single-nucleotide changes but got '%s'", sampleId, variants[j]);
            }
        slReverse(&vpn->sncList);
        slAddHead(&vpNodeList, vpn);
        }
    slReverse(&vpNodeList);
    struct placementInfo *info = hashFindVal(samplePlacements, sampleId);
    if (!info)
        errAbort("parseVariantPath: can't find placementInfo for sample '%s'", sampleId);
    info->variantPath = vpNodeList;
    }
lineFileClose(&lf);
}

static struct variantPathNode *parseSubtreeMut(char *line)
/* Parse a line with a node name and list of mutations.  Examples:
 * ROOT->1: C241T,C14408T,A23403G,C3037T,A20268G,C28854T,T24076C
 * 1: C20759T
 * USA/CA-CZB-4019/2020|EPI_ISL_548621|20-08-01: G17608T,G22199T
 * node_6849_condensed_4_leaves: 
 */
{
struct variantPathNode *vpn = NULL;
char *colon = strrchr(line, ':');
if (colon)
    {
    AllocVar(vpn);
    char *nodeName = line;
    *colon = '\0';
    vpn->nodeName = cloneString(nodeName);
    char *mutString = trimSpaces(colon+1);
    int mutCount = chopCommasLen(mutString);
    char *mutWords[mutCount];
    chopCommas(mutString, mutWords);
    int i;
    for (i = 0;  i < mutCount;  i++)
        {
        struct singleNucChange *snc = parseSnc(mutWords[i]);
        if (snc)
            slAddHead(&vpn->sncList, snc);
        else
            errAbort("parseSubtreeMut: Expected subtree mutation list to specify single-nucleotide "
                     "changes but got '%s'", mutWords[i]);
        }
    slReverse(&vpn->sncList);
    }
else
    errAbort("parseSubtreeMut: Expected line to contain colon but got '%s'", line);
return vpn;
}

static struct variantPathNode *parseSubtreeMutations(char *filename)
/* Parse subtree node mutation lists out of usher subtree-N-mutations.txt file. */
{
struct variantPathNode *subtreeMutList = NULL;
struct lineFile *lf = lineFileOpen(filename, TRUE);
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    struct variantPathNode *vpn = parseSubtreeMut(line);
    if (vpn)
        {
        // The ROOT->1 (subtree ancestor) sncList needs to be prepended to the subtree
        // root's sncList because Auspice doesn't seem to display mutations on nodes
        // with only one child.  Discard the subtree ancestor element.
        if (subtreeMutList && subtreeMutList->next == NULL &&
            startsWith("ROOT->", subtreeMutList->nodeName))
            {
            vpn->sncList = slCat(subtreeMutList->sncList, vpn->sncList);
            subtreeMutList = NULL;
            }
        slAddHead(&subtreeMutList, vpn);
        }
    }
slReverse(&subtreeMutList);
lineFileClose(&lf);
return subtreeMutList;
}

static struct usherResults *usherResultsNew()
/* Allocate & return usherResults (just bigTreePlusTn and samplePlacements, subtrees come later). */
{
struct usherResults *results;
AllocVar(results);
AllocVar(results->bigTreePlusTn);
trashDirFile(results->bigTreePlusTn, "ct", "phyloPlusSamples", ".nwk");
results->samplePlacements = hashNew(0);
return results;
}

static void rPhyloLeafNames(struct phyloTree *node, struct slName **pList)
/* Build up a list of leaf names under node in reverse depth-first-search order. */
{
if (node->numEdges > 0)
    {
    int i;
    for (i = 0;  i < node->numEdges;  i++)
        rPhyloLeafNames(node->edges[i], pList);
    }
else
    slAddHead(pList, slNameNew(node->ident->name));
}

static struct slName *phyloLeafNames(struct phyloTree *tree)
/* Return a list of leaf names in tree in depth-first-search order. */
{
struct slName *list = NULL;
rPhyloLeafNames(tree, &list);
slReverse(&list);
return list;
}

static struct hash *slNameListToIxHash(struct slName *list)
/* Given a list of names, add each name to a hash of ints with the index of the name in the list. */
{
struct hash *hash = hashNew(0);
struct slName *sln;
int ix;
for (ix = 0, sln = list;  sln != NULL;  ix++, sln = sln->next)
    hashAddInt(hash, sln->name, ix);
return hash;
}

static struct slName *getSubtreeSampleIds(struct slName *sampleIds, struct hash *subtreeIds)
/* Return a list of sampleIds that are found in subtreeIds. */
{
struct slName *subtreeSampleIds = NULL;
struct slName *id;
for (id = sampleIds;  id != NULL;  id = id->next)
    if (hashLookup(subtreeIds, id->name))
        slNameAddHead(&subtreeSampleIds, id->name);
slReverse(&subtreeSampleIds);
return subtreeSampleIds;
}

static struct hash *hashVpnList(struct variantPathNode *vpnList)
/* Return a hash of nodeName to sncList for each variantPathNode in vpnList. */
{
struct hash *vpnHash = hashNew(13);
struct variantPathNode *vpn;
for (vpn = vpnList;  vpn != NULL;  vpn = vpn->next)
    hashAdd(vpnHash, vpn->nodeName, vpn);
return vpnHash;
}

static void addMutationsToTree(struct phyloTree *node, struct hash *subtreeMutHash)
/* Store the list of mutations associated with each node in node->priv, removing node from
 * subtreeMutHash so we can check that it's empty when done with tree. */
{
if (! node->ident->name)
    errAbort("addMutationsToTree: node name is NULL but I need a node name to find mutations");
if (node->priv != NULL)
    errAbort("addMutationsToTree: node '%s' already has mutations assigned (duplicated in tree?)",
             node->ident->name);
struct variantPathNode *nodeMuts = hashFindVal(subtreeMutHash, node->ident->name);
if (! nodeMuts)
    errAbort("addMutationsToTree: can't find node '%s' in subtree mutations file",
             node->ident->name);
hashRemove(subtreeMutHash, node->ident->name);
node->priv = nodeMuts->sncList;
int i;
for (i = 0;  i < node->numEdges;  i++)
    addMutationsToTree(node->edges[i], subtreeMutHash);
}

static struct subtreeInfo *parseOneSubtree(struct tempName *subtreeTn, char *subtreeName,
                                           struct variantPathNode *subtreeMuts,
                                           struct slName *userSampleIds)
/* Parse usher's subtree output and figure out which user samples are in subtree. */
{
struct subtreeInfo *ti;
AllocVar(ti);
ti->subtreeTn = subtreeTn;
ti->subtree = phyloOpenTree(ti->subtreeTn->forCgi);
struct hash *subtreeMutHash = hashVpnList(subtreeMuts);
addMutationsToTree(ti->subtree, subtreeMutHash);
if (hashNumEntries(subtreeMutHash) != 0)
    errAbort("addMutationsToTree: subtree has fewer nodes than defined in subtree mutations file");
ti->subtreeNameList = phyloLeafNames(ti->subtree);
ti->subtreeIdToIx = slNameListToIxHash(ti->subtreeNameList);
ti->subtreeUserSampleIds = getSubtreeSampleIds(userSampleIds, ti->subtreeIdToIx);
if (slCount(ti->subtreeUserSampleIds) == 0)
    errAbort("No user sample IDs (out of %d) found in subtree file %s", slCount(userSampleIds), ti->subtreeTn->forCgi);
hashFree(&subtreeMutHash);
return ti;
}

static struct subtreeInfo *parseSubtrees(int subtreeCount, struct tempName *singleSubtreeTn,
                                         struct variantPathNode *singleSubtreeMuts,
                                         struct tempName *subtreeTns[],
                                         struct variantPathNode *subtreeMuts[],
                                         struct slName *userSampleIds)
/* Parse usher's subtree output and figure out which user samples are in each subtree.
 * Add parsed singleSubtree at head of list, followed by numbered subtrees. */
{
struct subtreeInfo *subtreeInfoList = NULL;
int sIx;
for (sIx = 0;  sIx < subtreeCount;  sIx++)
    {
    char subtreeName[512];
    safef(subtreeName, sizeof(subtreeName), "subtree%d", sIx+1);
    struct subtreeInfo *ti = parseOneSubtree(subtreeTns[sIx], subtreeName, subtreeMuts[sIx],
                                             userSampleIds);
    slAddHead(&subtreeInfoList, ti);
    }
slReverse(&subtreeInfoList);
struct subtreeInfo *ti = parseOneSubtree(singleSubtreeTn, "singleSubtree", singleSubtreeMuts,
                                         userSampleIds);
slAddHead(&subtreeInfoList, ti);
return subtreeInfoList;
}

static void parseClades(char *filename, struct hash *samplePlacements)
/* Parse usher's clades.txt, which might have {sample, clade} or {sample, clade, lineage}. */
{
struct hash *wordStore = hashNew(0);
struct lineFile *lf = lineFileOpen(filename, TRUE);
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    char *words[3];
    int wordCount = chopTabs(line, words);
    char *sampleId = words[0];
    struct placementInfo *info = hashFindVal(samplePlacements, sampleId);
    if (!info)
        errAbort("parseClades: can't find placementInfo for sample '%s'", sampleId);
    if (wordCount > 1)
        {
        // Nextstrain's clade "20E (EU1)" has to be tweaked to "20E.EU1" for matUtils to avoid
        // whitespace trouble; tweak it back.
        if (sameString(words[1], "20E.EU1"))
            words[1] = "20E (EU1)";
        // Chop extra chars in usher-sampled clades.txt output
        char *ptr = strstr(words[1], "*|");
        if (ptr)
            *ptr = '\0';
        info->nextClade = hashStoreName(wordStore, words[1]);
        }
    if (wordCount > 2)
        {
        // Chop extra chars in usher-sampled clades.txt output
        char *ptr = strstr(words[2], "*|");
        if (ptr)
            *ptr = '\0';
        info->pangoLineage = hashStoreName(wordStore, words[2]);
        }
    }
lineFileClose(&lf);
}

static char *dirPlusFile(struct dyString *dy, char *dir, char *file)
/* Write dir/file into dy and return pointer to dy->string.  Do not free result! */
{
dyStringClear(dy);
dyStringPrintf(dy, "%s/%s", dir, file);
return dy->string;
}

#define USHER_SAMPLED_TREE_PREFIX "tree-0-"

static int processOutDirFiles(struct usherResults *results, char *outDir,
                              struct tempName **retSingleSubtreeTn,
                              struct variantPathNode **retSingleSubtreeMuts,
                              struct tempName *subtreeTns[], struct variantPathNode *subtreeMuts[],
                              int maxSubtrees)
/* Get paths to files in outDir; parse them and move files that we'll keep up to trash/ct/,
 * leaving behind files that we will remove when done. */
{
int subtreeCount = 0;
memset(subtreeTns, 0, maxSubtrees * sizeof(*subtreeTns));
memset(subtreeMuts, 0, maxSubtrees * sizeof(*subtreeMuts));
struct dyString *dyScratch = dyStringNew(0);
struct slName *outDirFiles = listDir(outDir, "*"), *file;
for (file = outDirFiles;  file != NULL;  file = file->next)
    {
    char *path = dirPlusFile(dyScratch, outDir, file->name);
    if (sameString(file->name, "uncondensed-final-tree.nh"))
        {
        mustRename(path, results->bigTreePlusTn->forCgi);
        }
    else if (sameString(file->name, "mutation-paths.txt") ||  // original usher
             sameString(file->name, "mutation-paths-1.txt"))  // usher-sampled(-server)
        {
        parseVariantPaths(path, results->samplePlacements);
        }
    else if (startsWith("subtree-", file->name) ||
             startsWith(USHER_SAMPLED_TREE_PREFIX "subtree-", file->name))
        {
        char fnameCpy[strlen(file->name)+1];
        if (startsWith(USHER_SAMPLED_TREE_PREFIX, file->name))
            safecpy(fnameCpy, sizeof fnameCpy, file->name + strlen(USHER_SAMPLED_TREE_PREFIX));
        else
            safecpy(fnameCpy, sizeof fnameCpy, file->name);
        char *parts[4];
        int partCount = chopString(fnameCpy, "-", parts, ArraySize(parts));
        if (partCount == 2)
            {
            // Expect "subtree-N.nh"
            char *dot = strstr(parts[1], ".nh");
            if (dot)
                {
                *dot = '\0';
                int subtreeIx = atol(parts[1]) - 1;
                if (subtreeIx < 0)
                    errAbort("processOutDirFiles: expected subtree file names to have numbers "
                             "starting with 1, but found one with 0 (%s)", file->name);
                if (subtreeIx >= maxSubtrees)
                    errAbort("Too many subtrees in usher output (max %d)", maxSubtrees);
                if (subtreeIx >= subtreeCount)
                    subtreeCount = subtreeIx + 1;
                char sname[32];
                safef(sname, sizeof sname, "subtree%d", subtreeIx+1);
                AllocVar(subtreeTns[subtreeIx]);
                trashDirFile(subtreeTns[subtreeIx], "ct", sname, ".nwk");
                mustRename(path, subtreeTns[subtreeIx]->forCgi);
                }
            else
                warn("Unexpected filename '%s' from usher, ignoring", file->name);
            }
        else if (partCount == 3)
            {
            // Expect "subtree-N-mutations.txt" or "subtree-N-expanded.txt"
            int subtreeIx = atol(parts[1]) - 1;
            if (subtreeIx < 0)
                errAbort("processOutDirFiles: expected subtree file names to have numbers "
                         "starting with 1, but found one with 0 (%s)", file->name);
            if (subtreeIx >= maxSubtrees)
                errAbort("Too many subtrees in usher output (max %d)", maxSubtrees);
            if (subtreeIx >= subtreeCount)
                subtreeCount = subtreeIx + 1;
            if (sameString(parts[2], "mutations.txt"))
                {
                subtreeMuts[subtreeIx] = parseSubtreeMutations(path);
                }
            else if (sameString(parts[2], "expanded.txt"))
                {
                // Don't need this, just remove it
                }
            else
                warn("Unexpected filename '%s' from usher, ignoring", file->name);
            }
        else
            warn("Unexpected filename '%s' from usher, ignoring", file->name);
        }
    else if (startsWith("single-subtree", file->name) ||
             startsWith(USHER_SAMPLED_TREE_PREFIX "single-subtree", file->name))
        {
        // One subtree to bind them all
        char fnameCpy[strlen(file->name)+1];
        if (startsWith(USHER_SAMPLED_TREE_PREFIX, file->name))
            safecpy(fnameCpy, sizeof fnameCpy, file->name + strlen(USHER_SAMPLED_TREE_PREFIX));
        else
            safecpy(fnameCpy, sizeof fnameCpy, file->name);
        char *parts[4];
        int partCount = chopString(fnameCpy, "-", parts, ArraySize(parts));
        if (partCount == 2)
            {
            // Expect single-subtree.nh
            if (!endsWith(parts[1], ".nh"))
                warn("Unexpected filename '%s' from usher, ignoring", file->name);
            else if (retSingleSubtreeTn)
                {
                struct tempName *tn;
                AllocVar(tn);
                trashDirFile(tn, "ct", "subtree", ".nwk");
                mustRename(path, tn->forCgi);
                *retSingleSubtreeTn = tn;
                }
            }
        else if (partCount == 3)
            {
            // Expect single-subtree-mutations.txt or single-subtree-expanded.txt
            if (sameString(parts[2], "mutations.txt"))
                {
                if (retSingleSubtreeMuts)
                    *retSingleSubtreeMuts = parseSubtreeMutations(path);
                }
            else if (sameString(parts[2], "expanded.txt"))
                {
                // Don't need this, just remove it
                }
            else
                warn("Unexpected filename '%s' from usher, ignoring", file->name);
            }
        else
            warn("Unexpected filename '%s' from usher, ignoring", file->name);
        }
    else if (sameString(file->name, "clades.txt"))
        {
        parseClades(path, results->samplePlacements);
        }
    else if (sameString(file->name, "final-tree.nh") ||
             sameString(file->name, "current-tree.nh") ||
             sameString(file->name, "placement_stats.tsv"))
        {
        // Don't need this (or already parsed it elsewhere not here), just remove it.
        }
    else
        warn("Unexpected filename '%s' from usher, ignoring", file->name);
    }
dyStringFree(&dyScratch);
// Make sure we got a complete range of subtrees [0..subtreeCount-1]
int i;
for (i = 0;  i < subtreeCount;  i++)
    {
    if (subtreeTns[i] == NULL)
        errAbort("Missing file subtree-%d.nh in usher results", i+1);
    if (subtreeMuts[i] == NULL)
        errAbort("Missing file subtree-%d-mutations.txt in usher results", i+1);
    }
return subtreeCount;
}

static void removeOutDir(char *outDir)
/* Remove outDir and its files. */
{
struct slName *outDirFiles = listDir(outDir, "*"), *file;
struct dyString *dyScratch = dyStringNew(0);
for (file = outDirFiles;  file != NULL;  file = file->next)
    {
    char *path = dirPlusFile(dyScratch, outDir, file->name);
    unlink(path);
    }
dyStringFree(&dyScratch);
rmdir(outDir);
}

static void runUsherCommand(char *cmd[], char *stderrFile, char *anchorFile, int *pStartTime)
/* Run the standalone usher command with its stderr output redirected to stderrFile. */
{
char **cmds[] = { cmd, NULL };
struct pipeline *pl = pipelineOpen(cmds, pipelineRead, NULL, stderrFile, 0);
pipelineClose(&pl);
reportTiming(pStartTime, "run usher command");
}

boolean serverIsConfigured(char *org)
/* Return TRUE if all necessary configuration settings are in place to run usher-sampled-server. */
{
char *serverDir = cfgOption("hgPhyloPlaceServerDir");
if (isNotEmpty(serverDir))
    {
    char *usherServerEnabled = phyloPlaceOrgSetting(org, "usherServerEnabled");
    if (isNotEmpty(usherServerEnabled) && SETTING_IS_ON(usherServerEnabled) &&
        fileExists(PHYLOPLACE_DATA_DIR "/usher-sampled-server"))
        {
        return TRUE;
        }
    }
return FALSE;
}

static char *getUsherServerFilePath(char *org, char *file)
/* Alloc & return the path to special server file in $hgPhyloPlaceServerDir/$org/ . */
{
char *serverDir = cfgOption("hgPhyloPlaceServerDir");
// We need host name in order to prevent clashes between hgw1 and hgw2 sharing the same hg.conf
// contents and NFS filesystem.  And not the vhost name from getHost(), the real base host name.
char host[PATH_LEN];
int ret = gethostname(host, sizeof host);
if (ret != 0)
    safecpy(host, sizeof host, "hostunknown");
char *p = strchr(host, '.');
if (p)
    *p = '\0';
struct dyString *dyPath = dyStringCreate("%s/%s/%s.%s",
                                         serverDir, trackHubSkipHubName(org), host, file);
return dyStringCannibalize(&dyPath);
}

static char *getUsherServerMfifoPath(char *org)
/* Alloc & return path to server manager fifo file for org. */
{
return getUsherServerFilePath(org, "server.fifo");
}

static char *getUsherServerSocketPath(char *org)
/* Alloc & return path to server socket file for org. */
{
return getUsherServerFilePath(org, "server.socket");
}

static char *getUsherServerLogPath(char *org)
/* Alloc & return path to server log file (very important because it tells us the pid) for org. */
{
return getUsherServerFilePath(org, "server.log");
}

boolean serverIsRunning(char *org, FILE *errFile)
/* Return TRUE if logFile exists and its first line specifies a pid as expected from daemonishSpawn,
 * and that pid looks alive according to /proc. */
{
boolean pidIsLive = FALSE;
char *logFile = getUsherServerLogPath(org);
struct lineFile *lf = lineFileMayOpen(logFile, TRUE);
if (lf)
    {
    char *line;
    if (lineFileNext(lf, &line, NULL))
        {
        if (isAllDigits(line))
            {
            int pid = atol(line);
            if (pid > 1)
                {
                char procStatus[PATH_LEN];
                safef(procStatus, sizeof procStatus, "/proc/%d/status", pid);
                struct lineFile *psLf = lineFileMayOpen(procStatus, TRUE);
                if (psLf)
                    {
                    const static char *statePrefix ="State:\t";
                    boolean foundState = FALSE;
                    while (lineFileNext(psLf, &line, NULL))
                        {
                        if (startsWith(statePrefix, line))
                            {
                            foundState = TRUE;
                            char state = *(line + strlen(statePrefix));
                            // State Running, Sleep or Deep (uninterruptible) sleep is OK
                            if (state == 'R' || state == 'S' || state == 'D')
                                {
                                pidIsLive = TRUE;
                                fprintf(errFile, "serverIsRunning: Found pid %d with state %c.\n",
                                        pid, state);
                                }
                            else
                                {
                                // State sTopped or Zombie not OK
                                fprintf(errFile, "serverIsRunning: WARNING: pid %d has state %c.\n",
                                        pid, state);
                                }
                            }
                        }
                    lineFileClose(&psLf);
                    if (!foundState)
                        fprintf(errFile,
                                "serverIsRunning: No line of %s begins with '%s', "
                                "can't find process state.\n",
                                procStatus, statePrefix);
                    }
                else
                    fprintf(errFile, "serverIsRunning: Can't open %s, looks like server is dead.\n",
                            procStatus);
                }
            else
                fprintf(errFile, "serverIsRunning: expected number greater than 1 (pid) in "
                        "first line of log file %s, but got '%s'\n", logFile, line);
            }
        else
            {
            fprintf(errFile, "serverIsRunning: First line of log file %s has unexpected format '%s' "
                    "(expected to begin with pid)\n",
                    logFile, line);
            }
        }
    else
        fprintf(errFile, "serverIsRunning: log file '%s' appears empty\n", logFile);
    lineFileClose(&lf);
    }
else
    fprintf(errFile, "serverIsRunning: Can't open log file '%s'\n", logFile);
return pidIsLive;
}


//******************* BEGIN copied from lib/pipeline.c, consider libifying these ***************
static void closeNonStdDescriptors(void)
/* close non-standard file descriptors */
{
long maxFd = sysconf(_SC_OPEN_MAX);
if (maxFd < 0)
    maxFd = 4096;  // shouldn't really happen
int fd;
for (fd = STDERR_FILENO+1; fd < maxFd; fd++)
    close(fd);
}

static int openRead(char *fname)
/* open a file for reading */
{
int fd = open(fname, O_RDONLY);
if (fd < 0)
    errnoAbort("can't open for read access: %s", fname);
return fd;
}

static int openWrite(char *fname, boolean append)
/* open a file for write access */
{
int flags = O_WRONLY|O_CREAT;
if (append)
    flags |= O_APPEND;
else
    flags |= O_TRUNC;
int fd = open(fname, flags, 0666);
if (fd < 0)
    errnoAbort("can't open for write access: %s", fname);
return fd;
}
//******************* END copied from lib/pipeline.c, consider libifying these ***************


static void daemonishInit(int newStderr)
/* Isolate a double-fork-spawned process from its parent processes by closing file descriptors,
 * setting stderr to errlogFile, resetting umask etc.
 * See "man 7 daemon".  This function is "daemonish" because I'm not doing 100% of the recommended
 * initialization things from "man 7 daemon", because I'm not running a Linux system service,
 * just a program that needs to remain running in the background after this CGI exits.
 * This errAborts if setting stdin, stdout or stderr fails, and ignores any other failures. */
{
// Set stdin and stdout to /dev/null, stderr to errlogFile, close all other possibly open fds.
int newStdin = openRead("/dev/null");
if (dup2(newStdin, STDIN_FILENO) < 0)
    errnoAbort("daemonishInit: can't dup2 to stdin");
int newStdout = openWrite("/dev/null", TRUE);
if (dup2(newStdout, STDOUT_FILENO) < 0)
    errnoAbort("daemonishInit: can't dup2 to stdout");
if (dup2(newStderr, STDERR_FILENO) < 0)
    errnoAbort("daemonishInit: can't dup2 to stderr");
closeNonStdDescriptors();

// Reset all signal handlers to their default.
#ifndef SIGRTMAX   // may not have real-time extensions
#define SIGRTMAX SIGUSR2
#endif

int ix;
for (ix = 0;  ix <= SIGRTMAX;  ix++)
    signal(ix, SIG_DFL);

// Reset the signal mask
sigset_t sigset;
sigemptyset(&sigset);
sigprocmask(SIG_SETMASK, &sigset, NULL);

// Reset the umask
umask(0);
}

int daemonishSpawn(char *cmd[], char *errlogFile)
/* Use a double fork & setgid to create a process with parent process group = initd so it has
 * no controlling terminal and so the CGI won't become a zombie process when done.
 * See "man 7 daemon".  This function is "daemonish" because I'm not doing 100% of the recommended
 * initialization things from "man 7 daemon", because I'm not running a Linux system service,
 * just a program that needs to remain running in the background after this CGI exits.
 * Double-fork grandchild execs cmd, uses /dev/null for stdin and stdout, and sends stderr
 * to errlogFile -- after first writing its pid to errlogFile so that later processes can use
 * errlogFile to check whether the spawned process is still running.
 * Calling process/first fork parent returns result of first fork (pid for success, -1 for error) */
{
int fork1 = fork();
if (fork1 < 0)
    errnoWarn("first fork failed (%d)", fork1);
else if (fork1 == 0)
    {
    // First fork child will setsid and fork again
    if (setsid() < 0)
        errnoAbort("setsid failed; first fork child errAborting");
    else
        {
        int fork2 = fork();
        if (fork2 < 0)
            errnoAbort("second fork failed (%d); first fork child errAborting", fork2);
        else if (fork2 > 0)
            {
            // First fork child / second fork parent is all done. Use _exit not exit so we
            // don't close mysql connections etc.
            _exit(EXIT_SUCCESS);
            }
        else
            {
            // Second fork child sets stdin and stdout to /dev/null, stderr to errLogFile (after
            // writing pid), then becomes cmd using execvp.
            int newStderr = openWrite(errlogFile, FALSE);
            char pidBuf[16];
            safef(pidBuf, sizeof pidBuf, "%llu\n", (long long unsigned)getpid());
            write(newStderr, pidBuf, strlen(pidBuf));
            daemonishInit(newStderr);
            execvp(cmd[0], cmd);
            }
        }
    }
return fork1;
}

boolean startServer(char *org, struct treeChoices *treeChoices, FILE *errFile)
/* Start up an usher-sampled-server process to run in the background. */
{
boolean success = FALSE;
if (serverIsConfigured(org))
    {
    char *serverDir = cfgOption("hgPhyloPlaceServerDir");
    if (! fileExists(serverDir))
        makeDir(serverDir);
    char path[PATH_LEN];
    safef(path, sizeof path, "%s/%s", serverDir, org);
    if (! fileExists(path))
        makeDir(path);

    char *usherServerPath = PHYLOPLACE_DATA_DIR "/usher-sampled-server";
    // Each protobuf file from treeChoices must appear as a separate arg after -l,
    // so we have to dynamically build up serverCmd, but can use a static for the first args.
    char *serverCmdBase[] = { usherServerPath,
                              "-m", getUsherServerMfifoPath(org),
                              "-s", getUsherServerSocketPath(org),
                              "-T", USHER_NUM_THREADS,
                              "-t", USHER_SERVER_CHILD_TIMEOUT,
                              "-l" };
    size_t serverCmdBaseSize = ArraySize(serverCmdBase);
    size_t serverCmdSize = serverCmdBaseSize + treeChoices->count + 1;
    char *serverCmd[serverCmdSize];
    int ix;
    for (ix = 0;  ix < serverCmdBaseSize;   ix++)
        serverCmd[ix] = serverCmdBase[ix];
    for (ix = 0;  ix < treeChoices->count;  ix++)
        serverCmd[serverCmdBaseSize + ix] = treeChoices->protobufFiles[ix];
    serverCmd[serverCmdBaseSize + ix] = NULL;

    fputs("Spawning server with command: ", errFile);
    for (ix = 0;  serverCmd[ix] != NULL;  ix++)
        {
        fputc(' ', errFile);
        fputs(serverCmd[ix], errFile);
        }
    fputc('\n', errFile);
    success = (daemonishSpawn(serverCmd, getUsherServerLogPath(org)) > 0);
    if (success)
        fputs("Server spawned; first fork returned successfully.\n", errFile);
    else
        fputs("Server spawn failed at first fork!\n", errFile);
    }
else
    errAbort("Usher server is not configured for %s, not starting", org);
return success;
}

void serverReloadProtobufs(char *org, struct treeChoices *treeChoices)
/* Send a reload command and list of protobufs for org to usher server. */
{
char *usherServerMfifoPath = getUsherServerMfifoPath(org);
FILE *mf = fopen(usherServerMfifoPath, "a");
if (mf)
    {
    fprintf(mf, "reload\n");
    int ix;
    for (ix = 0;  ix < treeChoices->count;  ix++)
        fprintf(mf, "%s\n", treeChoices->protobufFiles[ix]);
    fputc('\n', mf);
    carefulClose(&mf);
    }
else
    warn("serverReload: unable to open '%s', command not sent", usherServerMfifoPath);
}

void serverStop(char *org)
/* Send stop command to usher server. */
{
char *usherServerMfifoPath = getUsherServerMfifoPath(org);
FILE *mf = fopen(usherServerMfifoPath, "a");
if (mf)
    {
    fprintf(mf, "stop\n");
    carefulClose(&mf);
    }
else
    warn("serverStop: unable to open '%s', command not sent", usherServerMfifoPath);
}

void serverSetThreadCount(char *org, int val)
/* Send thread command and value to usher server. */
{
char *usherServerMfifoPath = getUsherServerMfifoPath(org);
FILE *mf = fopen(usherServerMfifoPath, "a");
if (mf)
    {
    if (val > 0)
        fprintf(mf, "thread %d\n", val);
    else
        errAbort("Bad value %d passed to serverSetThreadCount, not sending", val);
    carefulClose(&mf);
    }
else
    warn("serverSetTimeout: unable to open '%s', command not sent", usherServerMfifoPath);
}

void serverSetTimeout(char *org, int val)
/* Send timeout command and value (in seconds) to usher server. */
{
char *usherServerMfifoPath = getUsherServerMfifoPath(org);
FILE *mf = fopen(usherServerMfifoPath, "a");
if (mf)
    {
    if (val > 0)
        fprintf(mf, "timeout %d\n", val);
    else
        errAbort("Bad value %d passed to serverSetTimeout, not sending", val);
    carefulClose(&mf);
    }
else
    warn("serverSetTimeout: unable to open '%s', command not sent", usherServerMfifoPath);
}

static int getServerSocket(char *org, struct treeChoices *treeChoices, FILE *errFile)
/* Try to connect to server; attempt to restart server and then connect if it seems like the server
 * is down.  Return -1 if unable to connect. */
{
int socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
if (socketFd < 0)
    {
    // OS-level failure, not anything we can do about it
    fprintf(errFile, "Failed to create a UNIX socket: %s\n", strerror(errno));
    }
else
    {
    // From "man 7 unix":
    // struct sockaddr_un {
    //     sa_family_t sun_family;               /* AF_UNIX */
    //     char        sun_path[UNIX_PATH_MAX];  /* pathname */
    // };
    struct sockaddr_un addr;
    addr.sun_family=AF_UNIX;
    char *usherServerSocketPath = getUsherServerSocketPath(org);
    safecpy(addr.sun_path, sizeof(addr.sun_path), usherServerSocketPath);
    int ret = connect(socketFd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0)
        {
        fprintf(errFile, "Cannot connect socket %d to path '%s': %s\n",
                socketFd, usherServerSocketPath, strerror(errno));
        // Does the server need to be restarted, or (in case another web request prompted some
        // other hgPhyloPlace process to restart it but it isn't quite up yet) should we wait a
        // few seconds and try again?
        if (serverIsRunning(org, errFile) ||
            startServer(org, treeChoices, errFile))
            {
            sleep(5);
            ret = connect(socketFd, (struct sockaddr *)&addr, sizeof(addr));
            if (ret < 0)
                {
                // Give up - fall back on regular usher command
                fprintf(errFile, "Second attempt to connect socket %d to path '%s' failed: %s\n",
                        socketFd, usherServerSocketPath, strerror(errno));
                socketFd = -1;
                }
            }
        }
    }
return socketFd;
}

// Server sends ASCII EOT character (4) when done. Sadly I can't find a header file that defines EOT.
#define EOT 4

static boolean sendQuery(int socketFd, char *cmd[], char *org, struct treeChoices *treeChoices,
                         FILE *errFile, boolean addNoIgnorePrefix, char *anchorFile)
/* Send command to socket, read response on socket, return TRUE if we get a successful response. */
{
boolean success = FALSE;
struct dyString *dyMessage = dyStringNew(0);
int ix;
for (ix = 0;  cmd[ix] != NULL;  ix++)
    {
    // Don't include args from -T onward; server rejects requests with -T or --optimization_radius
    if (sameString("-T", cmd[ix]))
        break;
    dyStringPrintf(dyMessage, "%s\n", cmd[ix]);
    }
if (addNoIgnorePrefix)
    // Needed when placing uploaded sequences, but not when finding uploaded names
    dyStringPrintf(dyMessage, "--no-ignore-prefix\n"USHER_DEDUP_PREFIX"\n");
if (isNotEmpty(anchorFile))
    dyStringPrintf(dyMessage, "--anchor-samples\n%s\n", anchorFile);
dyStringAppendC(dyMessage, '\n');
boolean serverError = FALSE;
int bytesWritten = write(socketFd, dyMessage->string, dyMessage->stringSize);
if (bytesWritten == dyMessage->stringSize)
    {
    struct lineFile *lf = lineFileAttach("server socket", TRUE, socketFd);
    if (lf)
        {
        char *line;
        while (lineFileNext(lf, &line, NULL))
            {
            if (startsWith("Tree", line) && endsWith(line, "not found"))
                {
                // Tell the server to reload the latest protobufs
                serverReloadProtobufs(org, treeChoices);
                // Reloading multiple trees takes a while, so fall back on standalone usher(-sampled)
                serverError = TRUE;
                // Continue reading output from server.
                }
            else if (line[0] == EOT)
                {
                success = ! serverError;
                break;
                }
            else if (isNotEmpty(line))
                fprintf(errFile, "%s\n", line);
            }
        }
    else
        fprintf(errFile, "Failed to attach linefile to socket %d.\n", socketFd);
    }
else
    fprintf(errFile, "Failed to send query to socket %d: attempted to write %ld bytes, ret=%d\n",
            socketFd, dyMessage->stringSize, bytesWritten);
dyStringFree(&dyMessage);
return success;
}

static boolean runUsherServer(char *org, char *cmd[], char *stderrFile,
                              struct treeChoices *treeChoices, char *anchorFile, int *pStartTime)
/* Start the server if necessary, connect to it, send a query, get response and return TRUE if.
 * all goes well. If unsuccessful, write reasons to errFile and return FALSE. */
{
boolean success = FALSE;
if (serverIsConfigured(org))
    {
    FILE *errFile = mustOpen(stderrFile, "w");
    int serverSocket = getServerSocket(org, treeChoices, errFile);

    reportTiming(pStartTime, "get socket");
    if (serverSocket > 0)
        {
        success = sendQuery(serverSocket, cmd, org, treeChoices, errFile, TRUE, anchorFile);
        close(serverSocket);
        if (success)
            reportTiming(pStartTime, "send query and get response (successful)");
        else
            reportTiming(pStartTime, "send query and get response (failed)");
        }
    carefulClose(&errFile);
    }
return success;
}

static int indexOfNull(char *stringArray[])
/* Return the index of the first NULL element of stringArray.
 * Do not call this unless stringArray has at least one NULL! */
{
int ix = 0;
while (stringArray[ix] != NULL)
    ix++;
return ix;
}

#define MAX_SUBTREES 1000

struct usherResults *runUsher(char *org, char *usherPath, char *usherAssignmentsPath, char *vcfFile,
                              int subtreeSize, struct slName **pUserSampleIds,
                              struct treeChoices *treeChoices, char *anchorFile, int *pStartTime)
/* Open a pipe from Yatish Turakhia's usher program, save resulting big trees and
 * subtrees to trash files, return list of slRef to struct tempName for the trash files
 * and parse other results out of stderr output.  The usher-sampled version of usher might
 * modify userSampleIds, adding a prefix if a sample with the same name is already in the tree. */
{
struct usherResults *results = usherResultsNew();
char subtreeSizeStr[16];
safef(subtreeSizeStr, sizeof subtreeSizeStr, "%d", subtreeSize);
struct tempName tnOutDir;
trashDirFile(&tnOutDir, "ct", "usher_outdir", ".dir");
char *cmd[] = { usherPath, "-v", vcfFile, "-i", usherAssignmentsPath, "-d", tnOutDir.forCgi,
                "-k", subtreeSizeStr, "-K", SINGLE_SUBTREE_SIZE, "-u",
                "-T", USHER_NUM_THREADS,       // Don't pass args from -T onward to server
                // Room for extra arguments if using usher-sampled
                NULL, NULL, NULL, NULL, NULL, NULL,
                NULL };
struct tempName tnStderr;
trashDirFile(&tnStderr, "ct", "usher_stderr", ".txt");
struct tempName tnServerStderr;
trashDirFile(&tnServerStderr, "ct", "usher_server_stderr", ".txt");
char *stderrFile = tnServerStderr.forCgi;
if (! runUsherServer(org, cmd, tnServerStderr.forCgi, treeChoices, anchorFile, pStartTime))
    {
    if (endsWith(usherPath, "-sampled"))
        {
        // Add --no-ignore-prefix
        int ix = indexOfNull(cmd);
        cmd[ix++] = "--no-ignore-prefix";
        cmd[ix++] = USHER_DEDUP_PREFIX;
        // Add --anchor-samples if configured
        if (isNotEmpty(anchorFile))
            {
            cmd[ix++] = "--anchor-samples";
            cmd[ix++] = anchorFile;
            }
        // Add --optimization-radius 0 unless optimization is explicitly enabled
        char *enableOptimization = phyloPlaceOrgSetting(org, "enableOptimization");
        if (SETTING_NOT_ON(enableOptimization))
            {
            cmd[ix++] = "--optimization_radius";
            cmd[ix++] = "0";
            }
        }
    runUsherCommand(cmd, tnStderr.forCgi, anchorFile, pStartTime);
    stderrFile = tnStderr.forCgi;
    }

struct tempName *singleSubtreeTn = NULL, *subtreeTns[MAX_SUBTREES];
struct variantPathNode *singleSubtreeMuts = NULL, *subtreeMuts[MAX_SUBTREES];
parsePlacements(tnOutDir.forCgi, stderrFile, results->samplePlacements, pUserSampleIds);
int subtreeCount = processOutDirFiles(results, tnOutDir.forCgi, &singleSubtreeTn,
                                      &singleSubtreeMuts, subtreeTns, subtreeMuts, MAX_SUBTREES);
if (singleSubtreeTn)
    {
    results->subtreeInfoList = parseSubtrees(subtreeCount, singleSubtreeTn, singleSubtreeMuts,
                                             subtreeTns, subtreeMuts, *pUserSampleIds);
    results->singleSubtreeInfo = results->subtreeInfoList;
    results->subtreeInfoList = results->subtreeInfoList->next;
    removeOutDir(tnOutDir.forCgi);
    }
else
    {
    results = NULL;
    warn("Sorry, there was a problem running usher.  "
         "Please ask genome-www@soe.ucsc.edu to take a look at %s.", stderrFile);
    }
reportTiming(pStartTime, "parse results from usher");
return results;
}

static void addEmptyPlacements(struct slName *sampleIds, struct hash *samplePlacements)
/* Parsing an usher-style clades.txt file from matUtils extract requires samplePlacements to
 * have placementInfo for each sample.  When running usher, those are added when we parse
 * placement_stats.tsv; when running matUtils, just allocate one for each sample. */
{
struct slName *sample;
for (sample = sampleIds;  sample != NULL;  sample = sample->next)
    {
    struct placementInfo *info;
    AllocVar(info);
    hashAdd(samplePlacements, sample->name, info);
    info->sampleId = cloneString(sample->name);
    }
}

static void runMatUtilsExtractCommand(char *matUtilsPath, char *protobufPath,
                                      char *subtreeSizeStr, struct tempName *tnSamples,
                                      struct tempName *tnOutDir, char *anchorFile, int *pStartTime)
/* Open a pipe from Yatish Turakhia and Jakob McBroome's matUtils extract to extract subtrees
 * containing sampleIds, save resulting subtrees to trash files, return subtree results.
 * Caller must ensure that sampleIds are names of leaves in the protobuf tree. */
{
char *cmd[] = { matUtilsPath, "extract", "-i", protobufPath, "-d", tnOutDir->forCgi,
                "-s", tnSamples->forCgi,
                "-x", subtreeSizeStr, "-X", SINGLE_SUBTREE_SIZE, "-T", USHER_NUM_THREADS,
                "--usher-clades-txt", "--usher-anchor-samples", anchorFile, NULL };
char **cmds[] = { cmd, NULL };
// Don't pass --usher-anchor-samples option unless it's configured
if (isEmpty(anchorFile))
    {
    int ix = stringArrayIx("--usher-anchor-samples", cmd, ArraySize(cmd)-1);
    if (ix > 0)
        cmd[ix] = NULL;
    }
struct tempName tnStderr;
trashDirFile(&tnStderr, "ct", "matUtils_stderr", ".txt");
struct pipeline *pl = pipelineOpen(cmds, pipelineRead, NULL, tnStderr.forCgi, 0);
pipelineClose(&pl);
reportTiming(pStartTime, "run matUtils command");
}

static boolean runMatUtilsServer(char *org, char *protobufPath, char *subtreeSizeStr,
                                 struct tempName *tnSamples, struct tempName *tnOutDir,
                                 struct treeChoices *treeChoices, char *anchorFile, int *pStartTime)
/* Cheng Ye added a 'matUtils mode' to usher-sampled-server so we can get subtrees super-fast
 * for uploaded sample names too. */
{
boolean success = FALSE;
char *cmd[] = { "usher-sampled-server", "-i", protobufPath, "-d", tnOutDir->forCgi,
                "-k", subtreeSizeStr, "-K", SINGLE_SUBTREE_SIZE,
                "--existing_samples", tnSamples->forCgi, "-D",
                NULL };
struct tempName tnErrFile;
trashDirFile(&tnErrFile, "ct", "matUtils_server_stderr", ".txt");
if (serverIsConfigured(org))
    {
    FILE *errFile = mustOpen(tnErrFile.forCgi, "w");
    int serverSocket = getServerSocket(org, treeChoices, errFile);

    reportTiming(pStartTime, "get socket");
    if (serverSocket > 0)
        {
        success = sendQuery(serverSocket, cmd, org, treeChoices, errFile, FALSE, anchorFile);
        close(serverSocket);
        if (success)
            reportTiming(pStartTime, "send query and get response (successful)");
        else
            reportTiming(pStartTime, "send query and get response (failed)");
        }
    carefulClose(&errFile);
    }
return success;
}

struct usherResults *runMatUtilsExtractSubtrees(char *org, char *matUtilsPath, char *protobufPath,
                                                int subtreeSize, struct slName *sampleIds,
                                                struct treeChoices *treeChoices, char *anchorFile,
                                                int *pStartTime)
/* Open a pipe from Yatish Turakhia and Jakob McBroome's matUtils extract to extract subtrees
 * containing sampleIds, save resulting subtrees to trash files, return subtree results.
 * Caller must ensure that sampleIds are names of leaves in the protobuf tree. */
{
struct usherResults *results = usherResultsNew();
char subtreeSizeStr[16];
safef(subtreeSizeStr, sizeof subtreeSizeStr, "%d", subtreeSize);
struct tempName tnSamples;
trashDirFile(&tnSamples, "ct", "matUtilsExtractSamples", ".txt");
FILE *f = mustOpen(tnSamples.forCgi, "w");
struct slName *sample;
for (sample = sampleIds;  sample != NULL;  sample = sample->next)
    fprintf(f, "%s\n", sample->name);
carefulClose(&f);
struct tempName tnOutDir;
trashDirFile(&tnOutDir, "ct", "matUtils_outdir", ".dir");
if (! runMatUtilsServer(org, protobufPath, subtreeSizeStr, &tnSamples, &tnOutDir, treeChoices,
                        anchorFile, pStartTime))
    runMatUtilsExtractCommand(matUtilsPath, protobufPath, subtreeSizeStr, &tnSamples, &tnOutDir,
                              anchorFile, pStartTime);
addEmptyPlacements(sampleIds, results->samplePlacements);
struct tempName *singleSubtreeTn = NULL, *subtreeTns[MAX_SUBTREES];
struct variantPathNode *singleSubtreeMuts = NULL, *subtreeMuts[MAX_SUBTREES];
int subtreeCount = processOutDirFiles(results, tnOutDir.forCgi, &singleSubtreeTn, &singleSubtreeMuts,
                                      subtreeTns, subtreeMuts, MAX_SUBTREES);
results->subtreeInfoList = parseSubtrees(subtreeCount, singleSubtreeTn, singleSubtreeMuts,
                                         subtreeTns, subtreeMuts, sampleIds);
results->singleSubtreeInfo = results->subtreeInfoList;
results->subtreeInfoList = results->subtreeInfoList->next;
reportTiming(pStartTime, "process results from matUtils");
return results;
}
