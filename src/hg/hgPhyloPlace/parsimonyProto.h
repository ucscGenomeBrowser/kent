/* Parse a protobuf-serialized file created by Yatish Turakhia's usher program
 * with the --save-mutation-annotated-tree into a phylogenetic tree annotated with SNVs.
 * https://github.com/yatisht/usher/ file parsimony.proto defines the file's data structure. */

#ifndef _PARSIMONY_PROTO_H_
#define _PARSIMONY_PROTO_H_

#include "hash.h"
#include "phyloTree.h"

struct singleNucChange
/* Single-nucleotide change associated with a node of a phylogenetic tree; may be a change
 * from ref to alt, or a back-mutation from alt to ref. */
    {
    struct singleNucChange *next;
    int chromStart;                // 0-based position
    char refBase;                  // reference genome allele
    char parBase;                  // parsimonious assignment allele
    char newBase;                  // base that is substituted for the parent's base value;
                                   // may be ref or alt.
    };

struct mutationAnnotatedTree
/* Full information from parsimony.proto: Tree with mutations and mapping of condensed nodes
 * (representing a set of identical samples) with the sample names that were condensed.
 * Analogous to Mutation_Annotated_Tree in https://github.com/yatisht/usher/ C++ code. */
    {
    struct phyloTree *tree;        // Phylogenetic tree; node->priv is list of singleNucChange(s)
    struct hash *condensedNodes;   // Condensed node name --> slName list of sample names
    struct hash *nodeHash;         // Node name --> node
    };

struct singleNucChange *sncNew(int chromStart, char refBase, char parBase, char newBase);
/* Alloc and return a new singleNucChange. */

struct mutationAnnotatedTree *parseParsimonyProtobuf(char *savedAssignmentsFile);
/* Return an annotated phylogenetic tree loaded from savedAssignments file.  Each node->priv
 * points to struct singleNucChange list (NULL if no change is associated with that node).
 * condensedNodes is a hash mapping names of condensed nodes to slName lists of
 * sample IDs that were condensed.  nodeHash is a hash mapping node names to nodes. */

#endif // _PARSIMONY_PROTO_H_
