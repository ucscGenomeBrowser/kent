/* Parse a protobuf-serialized file created by Yatish Turakhia's usher program
 * with the --save-mutation-annotated-tree into a phylogenetic tree annotated with SNVs.
 * https://github.com/yatisht/usher/ file parsimony.proto defines the file's data structure. */

/* Copyright (C) 2020 The Regents of the University of California */

#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "pipeline.h"
#include "parsimonyProto.h"
#include "protobuf.h"

/* Data structures corresponding to Yatish's parsimony.proto spec */
// struct singleNucChange defined in parsimonyProto.h corresponds to mutation message

struct mutationList
/* Data for one node in a phylogenetic tree: a list of single-nucleotide mutations
 * associated with this node by parsimonious assignment. */
    {
    struct mutationList *next;
    struct singleNucChange *muts;
    };

struct parsimonyData
/* A Newick format string encoding a tree, and an array of struct mutationList, one per node
 * in the tree (in order of left paren in Newick string). */
    {
    char *newick;                        // Tree in Newick/"New Hampshire" (.nh) format.
    struct hash *condensedNodes;         // Condensed node names --> list of sample names
    struct mutationList *nodeMutations;  // List (in DFS order) of per-node mutation lists.
    int nodeCount;                       // Number of nodes in tree
    };

static char intToNt(int val)
/* [0123] --> [ACGT] */
{
static char map[5] = "ACGT";
if (val < 0 || val > 3)
    errAbort("intToNt: expected input to be between 0 and 3 but got %d", val);
return map[val];
}

struct singleNucChange *sncNew(int chromStart, char refBase, char parBase, char newBase)
/* Alloc and return a new singleNucChange. */
{
struct singleNucChange *snc;
AllocVar(snc);
snc->chromStart = chromStart;
snc->refBase = toupper(refBase);
snc->parBase = toupper(parBase);
snc->newBase = toupper(newBase);
return snc;
}

struct parsimonyData *parsimonyDataFromProtobuf(struct protobuf *data)
/* Unpack the parsed protobuf into friendlier data structures (also converting [0123] to [ACGT]. */
{
struct parsimonyData *parData;
AllocVar(parData);
struct mutationList *nodeList = NULL;
struct hash *condensedNodes = hashNew(0);
struct protobufField *field;
int nodeIx = 0;
for (field = data->fields;  field != NULL;  field = field->next)
    {
    if (sameString(field->def->name, "newick"))
        parData->newick = field->value.vString;
    else if (sameString(field->def->name, "node_mutations"))
        {
        struct mutationList *mutList;
        AllocVar(mutList);
        int mutListIx;
        for (mutListIx = 0;  mutListIx < field->length;  mutListIx++)
            {
            struct protobuf *mutListMessage = field->value.vEmbedded[mutListIx];
            struct protobufField *mutListField;
            for (mutListField = mutListMessage->fields;  mutListField != NULL;
                 mutListField = mutListField->next)
                {
                if (differentString(mutListField->def->name, "mutation"))
                    errAbort("Expected field name mutation, got '%s'", mutListField->def->name);
                int mutIx;
                for (mutIx = 0;  mutIx < mutListField->length;  mutIx++)
                    {
                    struct singleNucChange *snc;
                    AllocVar(snc);
                    struct protobuf *mutMessage = mutListField->value.vEmbedded[mutIx];
                    struct protobufField *mutField;
                    for (mutField = mutMessage->fields;  mutField != NULL;
                         mutField = mutField->next)
                        {
                        char *mutFieldName = mutField->def->name;
                        if (sameString(mutFieldName, "position"))
                            snc->chromStart = mutField->value.vInt32 - 1;
                        else if (sameString(mutFieldName, "ref_nuc"))
                            snc->refBase = intToNt(mutField->value.vInt32);
                        else if (sameString(mutFieldName, "par_nuc"))
                            snc->parBase = intToNt(mutField->value.vInt32);
                        else if (sameString(mutFieldName, "mut_nuc"))
                            {
                            if (mutField->length != 1)
                                warn("node %d, pos %d: expected exactly one mut_nuc but got %d "
                                     "(ref %c)\n",
                                     nodeIx, snc->chromStart+1, mutField->length, snc->refBase);
                            snc->newBase = intToNt(mutField->value.vPacked[0].vInt32);
                            }
                        else if (sameString(mutFieldName, "chromosome"))
                            {
                            // Ignore -- not applicable for SARS-CoV-2
                            }
                        else
                            errAbort("Unrecognized field name '%s' in mutation message",
                                     mutFieldName);
                        }
                    // Int fields whose value is 0 are omitted
                    if (snc->refBase == '\0')
                        snc->refBase = intToNt(0);
                    if (snc->parBase == '\0')
                        snc->parBase = intToNt(0);
                    if (snc->newBase == '\0')
                        warn("node %d, pos %d: How did we get a node with a mutation message "
                             "but no mut_nuc field?",
                             nodeIx, snc->chromStart+1);
                    slAddHead(&mutList->muts, snc);
                    }
                }
            slReverse(&mutList->muts);
            }
        nodeIx++;
        slAddHead(&nodeList, mutList);
        }
    else if (sameString(field->def->name, "condensed_nodes"))
        {
        int cnIx;
        for (cnIx = 0;  cnIx < field->length;  cnIx++)
            {
            struct protobuf *condNodeMessage = field->value.vEmbedded[cnIx];
            char *cnName = NULL;
            struct protobufField *cnField;
            for (cnField = condNodeMessage->fields;  cnField != NULL;  cnField = cnField->next)
                {
                if (sameString(cnField->def->name, "node_name"))
                    {
                    if (cnName != NULL)
                        errAbort("Multiple node_name messages in condensed_node (%s, %s)",
                                 cnName, cnField->value.vString);
                    cnName = cnField->value.vString;
                    }
                else if (sameString(cnField->def->name, "condensed_leaves"))
                    {
                    if (cnName == NULL)
                        errAbort("Need more complex parsing code: expected node_name field to "
                                 "precede condensed_leaves field, but it did not.");
                    struct hashEl *hel = hashLookup(condensedNodes, cnName);
                    if (hel)
                        {
                        struct slName *clList = hel->val;
                        slNameAddTail(&clList, cnField->value.vString);
                        }
                    else
                        hashAdd(condensedNodes, cnName, slNameNew(cnField->value.vString));
                    }
                else
                    errAbort("Unexpected field name '%s' in condensed_nodes message",
                             cnField->def->name);
                }
            }
        }
    else
        errAbort("Unexpected field name '%s' in top-level message", field->def->name);
    }
slReverse(&nodeList);
parData->condensedNodes = condensedNodes;
// Make nodeMutations array from nodeList.
parData->nodeCount = slCount(nodeList);
AllocArray(parData->nodeMutations, parData->nodeCount);
struct mutationList *nodeMuts;
int i;
for (i = 0, nodeMuts = nodeList;  i < parData->nodeCount;  i++, nodeMuts = nodeMuts->next)
    parData->nodeMutations[i] = *nodeMuts;
return parData;
}

static void treeAddVariants(struct phyloTree *node, struct mutationList *nodeMutations,
                            int nodeCount, int *pNodeNum, int *pINodeNum, struct hash *nodeHash)
/* Annotate the nodes of tree from the array of per-node mutations -- use node->priv
 * to store the variants.  Also, if a node has no name, add the internal node number as the name. */
{
if (*pNodeNum < 0 || *pNodeNum >= nodeCount)
    errAbort("treeAddVariants: bad nodeNum %d (must be between 0 and %d)", *pNodeNum, nodeCount-1);
node->priv = nodeMutations[*pNodeNum].muts;
// Add internal node number as node name -- helps match up with mutation paths from usher.
if (isEmpty(node->ident->name) && node->numEdges > 0)
    {
    char iNodeName[16];
    safef(iNodeName, sizeof iNodeName, "%d", *pINodeNum);
    node->ident->name = cloneString(iNodeName);
    }
(*pNodeNum)++;
if (node->numEdges > 0)
    (*pINodeNum)++;
int i;
for (i = 0;  i < node->numEdges;  i++)
    treeAddVariants(node->edges[i], nodeMutations, nodeCount, pNodeNum, pINodeNum, nodeHash);
if (isNotEmpty(node->ident->name))
    hashAdd(nodeHash, node->ident->name, node);
}

struct mutationAnnotatedTree *parseParsimonyProtobuf(char *savedAssignmentsFile)
/* Return an annotated phylogenetic tree loaded from savedAssignments file.  Each node->priv
 * points to struct singleNucChange list (NULL if no change is associated with that node).
 * condensedNodes is a hash mapping names of condensed nodes to slName lists of
 * sample IDs that were condensed.  nodeHash is a hash mapping node names to nodes. */
{
FILE *f = NULL;
if (endsWith(savedAssignmentsFile, ".gz"))
    {
    static char *command[] = {"gzip", "-dc", NULL};
    struct pipeline *pl = pipelineOpen1(command, pipelineRead|pipelineSigpipe, savedAssignmentsFile, NULL, 0);
    f = pipelineFile(pl);
    }
else
    f = mustOpen(savedAssignmentsFile, "r");

// Hand-compiled from ~angie/github/yatish_strain_phylogenetics/parsimony.proto Aug. 10 2020...
// message mut {
//     int32 position = 1;
//     int32 ref_nuc = 2;
//     int32 par_nuc = 3;
//     repeated int32 mut_nuc = 4;
// }
struct protobufFieldDef chromField = { NULL, "chromosome", 5, pbdtString, NULL, FALSE };
struct protobufFieldDef mutNucField = { &chromField, "mut_nuc", 4, pbdtInt32, NULL, TRUE };
struct protobufFieldDef parNucField = { &mutNucField, "par_nuc", 3, pbdtInt32, NULL, FALSE };
struct protobufFieldDef refNucField = { &parNucField, "ref_nuc", 2, pbdtInt32, NULL, FALSE };
struct protobufFieldDef positionField = { &refNucField, "position", 1, pbdtInt32, NULL, FALSE };
struct protobufDef mutDef = { NULL, "mut", &positionField };
// message mutation_list {
//     repeated mut mutation = 1;
// }
struct protobufFieldDef mutationField = { NULL, "mutation", 1, pbdtEmbedded, &mutDef, TRUE };
struct protobufDef mutListDef = { NULL, "mutation_list", &mutationField };
// message condensed_node {
//     string node_name = 1;
//     repeated string condensed_leaves = 2;
// }
struct protobufFieldDef condLeavesField = { NULL, "condensed_leaves", 2, pbdtString, NULL, TRUE };
struct protobufFieldDef nodeNameField = { &condLeavesField, "node_name", 1, pbdtString, NULL, FALSE};
struct protobufDef condNodeDef = { NULL, "condensed_node", &nodeNameField };
// message data {
//     string newick = 1;
//     repeated mutation_list node_mutations = 2; 
//     repeated condensed_node condensed_nodes = 3;
// }
struct protobufFieldDef condNodeField = { NULL, "condensed_nodes", 3, pbdtEmbedded, &condNodeDef,
                                          TRUE };
struct protobufFieldDef nodeMutsField = { &condNodeField, "node_mutations", 2, pbdtEmbedded,
                                          &mutListDef, TRUE };
struct protobufFieldDef newickField = { &nodeMutsField, "newick", 1, pbdtString, NULL, FALSE };
struct protobufDef def = { NULL, "data", &newickField };
// Parse the protobuf file into protobuf data structures
long long bytesLeft = BIGLONGLONG;
struct protobuf *data = protobufParse(f, &def, &bytesLeft);
if (fgetc(f) != EOF)
    errAbort("Protobuf size exceeded %lx bytes, please report this error to "
             "genome-www@soe.ucsc.edu.", BIGLONGLONG);
carefulClose(&f);
// Convert the protobuf data structures into friendlier data structures
struct parsimonyData *parData = parsimonyDataFromProtobuf(data);
if (parData == NULL)
    return NULL;
struct mutationAnnotatedTree *tree;
AllocVar(tree);
tree->tree = phyloParseString(parData->newick);
// Annotate the tree (->priv) with parsimonious assignments and single-nucleotide changes.
tree->nodeHash = hashNew(digitsBaseTwo(parData->nodeCount) + 1);
int nodeNum = 0, iNodeNum = 1;
treeAddVariants(tree->tree, parData->nodeMutations, parData->nodeCount, &nodeNum, &iNodeNum,
                tree->nodeHash);
tree->condensedNodes = parData->condensedNodes;
// Mem leak since we don't free up protobuf data or packaging of singleNucChanges,
// but we only do this once.
return tree;
}
