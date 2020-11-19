/* Invoke usher to place user's uploaded samples in the phylogenetic tree & parse output files. */

/* Copyright (C) 2020 The Regents of the University of California */

#include "common.h"
#include "dnautil.h"
#include "hash.h"
#include "linefile.h"
#include "obscure.h"
#include "parsimonyProto.h"
#include "phyloPlace.h"
#include "regexHelper.h"
#include "pipeline.h"
#include "trashDir.h"

// Keywords in stderr output of usher:
#define sampleIdPrefix "Sample name:"
#define pScorePrefix "Parsimony score:"
#define numPlacementsPrefix "Number of parsimony-optimal placements:"
#define bestNodePrefix "Best node ("
#define mutationsPrefix "Mutations: "
#define sampleMutsPrefix "Sample mutations:"
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

static struct variantPathNode *parsePipeyPath(char *pipeyPath)
/* Parse something like "|C8782T|T28144C| > |C29095T|  > |G2494A|T11083G|C18501T|"
 * into a variant path with unknown node names. */
{
if (isEmpty(pipeyPath))
    return NULL;
struct variantPathNode *variantPath = NULL;
char *words[strlen(pipeyPath) / 5];
int wordCount = chopString(pipeyPath, " > ", words, ArraySize(words));
int i;
for (i = 0;  i < wordCount;  i++)
    {
    struct variantPathNode *vpn;
    AllocVar(vpn);
    vpn->nodeName = cloneString("?");
    char *mutStr = words[i];
    // Trim first and last pipes
    if (mutStr[0] == '|')
        mutStr++;
    if (mutStr[strlen(mutStr)-1] == '|')
        mutStr[strlen(mutStr)-1] = '\0';
    // Split by pipe
    char *muts[strlen(mutStr) / 4];
    int sncCount = chopString(mutStr, "|", muts, ArraySize(muts));
    int j;
    for (j = 0;  j < sncCount;  j++)
        {
        struct singleNucChange *snc = parseSnc(muts[j]);
        if (!snc)
            errAbort("Expected single-nucleotide change but got '%s' when parsing pipe-separated "
                     "best node path", muts[j]);
        slAddHead(&vpn->sncList, snc);
        }
    slReverse(&vpn->sncList);
    slAddHead(&variantPath, vpn);
    }
slReverse(&variantPath);
return variantPath;
}

static boolean parseBestNode(char **words, struct placementInfo *info)
/* If the line starts with "Best node", parse out the name and path of the node
 * (or one of the nodes) with the lowest parsimony distance from the sample being placed;
 * add to info->bestNodes and return TRUE. */
{
// Example line (* means this node was chosen for placement):
// words[0] = Best node (child)*: 1239
// words[1] = Mutations: |C8782T|T28144C| > |C29095T| > |T8782C| > |T29095C| > |C28144T|
// or
// words[0] = Best node (sibling): SomeLeafName
// words[1] = Mutations: |C8782T|T28144C| > |C29095T| > |T8782C| > |T29095C| > |C28144T| > |G11083T| > |G2494A|T11083G|C18501T|
boolean matches = FALSE;
if (stringIn(bestNodePrefix, words[0]))
    {
    matches = TRUE;
    struct bestNodeInfo *bn;
    AllocVar(bn);
    char *p = words[0] + strlen(bestNodePrefix);
    if (startsWith("sibling", p))
        bn->isSibling = TRUE;
    boolean isChosen = (stringIn(")*:", words[0]) != NULL);
    p = stringIn(": ", words[0]);
    if (p)
        bn->name = cloneString(p + strlen(": "));
    else
        errAbort("parseBestNode: expected first column to have ': ' followed by name, but got '%s'",
                 words[0]);
    if (startsWith(mutationsPrefix, words[1]))
        bn->variantPath = parsePipeyPath(words[1] + strlen(mutationsPrefix));
    else
        errAbort("parseBestNode: expected second column to have '" mutationsPrefix"' followed by "
                 "path, but got '%s'", words[1]);
    if (isChosen)
        slAddHead(&info->bestNodes, bn);
    else
        slAddTail(&info->bestNodes, bn);
    }
return matches;
}


static boolean parseSampleMutations(char **words, struct placementInfo *info)
/* If words[] looks like it defines the sample mutations relative to the reference genome,
 * then parse out the list and add to info->sampleMuts and return TRUE. */
{
// Example line:
// words[0] = Sample mutations:
// words[1] = |C241T| |C3037T| |C14408T| |A23403G|
boolean matches = FALSE;
if (stringIn(sampleMutsPrefix, words[0]))
    {
    matches = TRUE;
    char *mutStr = words[1];
    stripChar(mutStr, '|');
    info->sampleMuts = slNameListFromString(mutStr, ' ');
    }
return matches;
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
    char *muts[strlen(words[1]) / 4];
    int mutCount = chopString(words[1], ";", muts, ArraySize(muts));
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
    info->imputedBases = bvList;
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
        if (! parseBestNode(words, info) &&
            ! parseSampleMutations(words, info))
            parseImputedMutations(words, info);
        }
    }
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
    char *mutWords[strlen(mutString)/4];
    int mutCount = chopCommas(mutString, mutWords);
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

static int hashElIntCmpDesc(const void *va, const void *vb)
/* Compare two hashEl by integer value, descending. */
{
const struct hashEl *a = *((struct hashEl **)va);
const struct hashEl *b = *((struct hashEl **)vb);
return b->val - a->val;
}

static char *summarizeCountries(struct slName *idList)
/* If enough sample names in idList have identifiable countries, then return a summary string,
 * else NULL. */
{
char *summary = NULL;
int idCount = slCount(idList);
int idWithCountryCount = 0;
struct hash *countryCounts = hashNew(0);
struct slName *id;
for (id = idList;  id != NULL;  id = id->next)
    {
    regmatch_t substrs[2];
    if (regexMatchSubstr(id->name, "([A-Za-z_]+)/[^/]+/20", substrs, ArraySize(substrs)))
        {
        char country[256];
        regexSubstringCopy(id->name, substrs[1], country, sizeof country);
        idWithCountryCount++;
        hashIncInt(countryCounts, country);
        }
    }
if ((double)idWithCountryCount / idCount >= 0.5)
    {
    struct dyString *dy = dyStringCreate("%d from ", idCount);
    struct hashEl *helList = hashElListHash(countryCounts);
    if (helList->next == NULL)
        dyStringAppend(dy, helList->name);
    else
        {
        slSort(&helList, hashElIntCmpDesc);
        struct hashEl *hel;
        for (hel = helList;  hel != NULL;  hel = hel->next)
            {
            if (hel != helList)
                dyStringAppend(dy, "+");
            dyStringAppend(dy, hel->name);
            int count = ptToInt(hel->val);
            if (count > 1)
                dyStringPrintf(dy, "[%d]", count);
            if (dyStringLen(dy) > 100)
                {
                dyStringAppend(dy, "+...");
                break;
                }
            }
        }
    // Labels have to be unique -- tack on an example ID
    dyStringPrintf(dy, " eg %s", idList->name);
    summary = dyStringCannibalize(&dy);
    subChar(summary, ' ', '_');
    }
return summary;
}

static char *expandCondensedNodeName(struct hash *condensedNodes, char *name)
/* If name is found in condensedNodes, then return an expanded name that indicates countries of
 * origin (if we can find them) and an example ID.  Otherwise return NULL. */
{
char *expandedName = NULL;
struct slName *nodeList = hashFindVal(condensedNodes, name);
if (nodeList)
    {
    char *summary = summarizeCountries(nodeList);
    if (summary)
        expandedName = summary;
    else
        {
        int nodeCount = slCount(nodeList);
        struct dyString *dy = dyStringCreate("%s_and_%d_others", nodeList->name, nodeCount-1);
        expandedName=  dyStringCannibalize(&dy);
        }
    }
return expandedName;
}

static struct hash *expandCondensedNodeNames(struct hash *condensedNodes, struct slName *idList)
/* If idList contains names found in condensedNodes, then hash those names to new names that are
 * _-sep strings from the lists in condensedNodes.  Return hash (which may have no elements
 * if nothing in idList is found in condensedNodes). */
{
struct hash *hash = hashNew(0);
struct slName *id;
for (id = idList;  id != NULL;  id = id->next)
    {
    char *expandedName = expandCondensedNodeName(condensedNodes, id->name);
    if (expandedName)
        hashAdd(hash, id->name, expandedName);
    }
return hash;
}

static void rSubstTreeNames(struct phyloTree *node, struct hash *nameSubstitutions)
/* If node or descendants have names in nameSubstitutions, then substitute those names. */
{
if (node->ident->name)
    {
    char *subst = hashFindVal(nameSubstitutions, node->ident->name);
    if (subst)
        node->ident->name = subst;
    }
int i;
for (i = 0;  i < node->numEdges;  i++)
    rSubstTreeNames(node->edges[i], nameSubstitutions);
}

static struct tempName *substituteTreeNames(struct phyloTree *tree, struct hash *nameSubstitutions)
/* If tree has any nodes whose names are in nameSubstitutions, then substitute those names.
 * Write tree out to a trash file and return its location. */
{
rSubstTreeNames(tree, nameSubstitutions);
struct tempName *newTn;
AllocVar(newTn);
trashDirFile(newTn, "ct", "treeNameSubst", ".nwk");
FILE *f = mustOpen(newTn->forCgi, "w");
phyloPrintTree(tree, f);
carefulClose(&f);
return newTn;
}

static struct slName *substituteNameList(struct slName *idList, struct hash *nameSubstitutions)
/* Return a new list that is just like idList, except if any item in idList has a value in
 * nameSubstitutions, then the item is replaced by the substitution. */
{
struct slName *newList = NULL;
struct slName *id;
for (id = idList;  id != NULL;  id = id->next)
    {
    char *subst = hashFindVal(nameSubstitutions, id->name);
    slNameAddHead(&newList, subst ? subst : id->name);
    }
slReverse(&newList);
return newList;
}

static void addMutationsToTree(struct phyloTree *node, struct variantPathNode **pNodeMutList)
/* Store the list of mutations associated with each node in node->priv. */
{
struct variantPathNode *nodeMuts = slPopHead(pNodeMutList);
if (! nodeMuts)
    errAbort("addMutationsToTree: subtree mutation list has fewer nodes than subtree");
if (! sameString(nodeMuts->nodeName, node->ident->name))
    errAbort("addMutationsToTree: subtree node name is '%s' but subtree mutation list item is '%s'",
             node->ident->name, nodeMuts->nodeName);
if (node->priv != NULL)
    errAbort("addMutationsToTree: node already has mutations assigned");
node->priv = nodeMuts->sncList;
int i;
for (i = 0;  i < node->numEdges;  i++)
    addMutationsToTree(node->edges[i], pNodeMutList);
}

static struct subtreeInfo *parseSubtrees(int subtreeCount, struct tempName *subtreeTns[],
                                         struct variantPathNode *subtreeMuts[],
                                         struct slName *userSampleIds, struct hash *condensedNodes)
/* Parse usher's subtree output, figure out which user samples are in each subtree, expand names
 * of condensed nodes, and write auspice json. */
{
struct subtreeInfo *subtreeInfoList = NULL;
int sIx;
for (sIx = 0;  sIx < subtreeCount;  sIx++)
    {
    struct subtreeInfo *ti;
    AllocVar(ti);
    ti->subtreeTn = subtreeTns[sIx];
    ti->subtree = phyloOpenTree(ti->subtreeTn->forCgi);
    addMutationsToTree(ti->subtree, &subtreeMuts[sIx]);
    if (subtreeMuts[sIx] != NULL)
        errAbort("addMutationsToTree: subtreeMutationList has more nodes than subtree");
    struct slName *subtreeIdList = phyloLeafNames(ti->subtree);
    // Don't do name substitutions on condensed node names in subtreeIdToIx since the IDs have to
    // match those in the original tree from protobuf.
    ti->subtreeIdToIx = slNameListToIxHash(subtreeIdList);
    ti->subtreeUserSampleIds = getSubtreeSampleIds(userSampleIds, ti->subtreeIdToIx);
    if (slCount(ti->subtreeUserSampleIds) == 0)
        errAbort("No user sample IDs found in subtree file %s", ti->subtreeTn->forCgi);
    // Substitute in nicer node names for condensed nodes for displaying to the user in
    // custom tracks and Nextstrain/auspice JSON.
    struct hash *nameSubstitutions = expandCondensedNodeNames(condensedNodes, subtreeIdList);
    if (nameSubstitutions->elCount > 0)
        ti->subtreeTn = substituteTreeNames(ti->subtree, nameSubstitutions);
    ti->subtreeNameList = substituteNameList(subtreeIdList, nameSubstitutions);
    slAddHead(&subtreeInfoList, ti);
    hashFree(&nameSubstitutions);
    slFreeList(&subtreeIdList);
    }
slReverse(&subtreeInfoList);
return subtreeInfoList;
}

static char *dirPlusFile(struct dyString *dy, char *dir, char *file)
/* Write dir/file into dy and return pointer to dy->string. */
{
dyStringClear(dy);
dyStringPrintf(dy, "%s/%s", dir, file);
return dy->string;
}

static int processOutDirFiles(struct usherResults *results, char *outDir,
                              struct tempName *subtreeTns[], struct variantPathNode *subtreeMuts[],
                              int maxSubtrees)
/* Get paths to files in outDir; parse them, move files that we'll keep up to trash/ct/,
 * remove outDir. */
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
    else if (sameString(file->name, "mutation-paths.txt"))
        {
        parseVariantPaths(path, results->samplePlacements);
        }
    else if (startsWith("subtree-", file->name))
        {
        char fnameCpy[strlen(file->name)+1];
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
    else if (sameString(file->name, "final-tree.nh"))
        {
        // Don't need this, just remove it.
        }
    else
        warn("Unexpected filename '%s' from usher, ignoring", file->name);
    unlink(path);
    }
rmdir(outDir);
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

#define MAX_SUBTREES 1000

struct usherResults *runUsher(char *usherPath, char *usherAssignmentsPath, char *vcfFile,
                              int subtreeSize, struct slName *userSampleIds,
                              struct hash *condensedNodes, int *pStartTime)
/* Open a pipe from Yatish Turakhia's usher program, save resulting big trees and
 * subtrees to trash files, return list of slRef to struct tempName for the trash files
 * and parse other results out of stderr output. */
{
struct usherResults *results = usherResultsNew();
char subtreeSizeStr[16];
safef(subtreeSizeStr, sizeof subtreeSizeStr, "%d", subtreeSize);
char *numThreadsStr = "16";
struct tempName tnOutDir;
trashDirFile(&tnOutDir, "ct", "usher_outdir", ".dir");
char *cmd[] = { usherPath, "-v", vcfFile, "-i", usherAssignmentsPath, "-d", tnOutDir.forCgi,
                "-k", subtreeSizeStr, "-T", numThreadsStr, "-u", "-l", NULL };
char **cmds[] = { cmd, NULL };
struct tempName tnStderr;
trashDirFile(&tnStderr, "ct", "usher_stderr", ".txt");
struct pipeline *pl = pipelineOpen(cmds, pipelineRead, NULL, tnStderr.forCgi);
pipelineClose(&pl);
reportTiming(pStartTime, "run usher");
parseStderr(tnStderr.forCgi, results->samplePlacements);

struct tempName *subtreeTns[MAX_SUBTREES];
struct variantPathNode *subtreeMuts[MAX_SUBTREES];
int subtreeCount = processOutDirFiles(results, tnOutDir.forCgi, subtreeTns, subtreeMuts,
                                      MAX_SUBTREES);
results->subtreeInfoList = parseSubtrees(subtreeCount, subtreeTns, subtreeMuts, userSampleIds,
                                         condensedNodes);
return results;
}

