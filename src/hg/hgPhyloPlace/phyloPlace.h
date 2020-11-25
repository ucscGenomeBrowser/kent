/* Place SARS-CoV-2 sequences in phylogenetic tree using add_missing_samples program. */

#ifndef _PHYLO_PLACE_H_
#define _PHYLO_PLACE_H_

#include "common.h"
#include "dnaseq.h"
#include "hash.h"
#include "linefile.h"
#include "parsimonyProto.h"
#include "phyloTree.h"
#include "trashDir.h"

#define PHYLOPLACE_DATA_DIR "hgPhyloPlaceData"

struct seqInfo
/* User sequences, alignments and statistics */
{
    struct seqInfo *next;
    struct dnaSeq *seq;
    struct psl *psl;
    struct singleNucChange *sncList;
    struct singleNucChange *maskedSncList;
    struct slRef *maskedReasonsList;
    uint nCountStart;
    uint nCountMiddle;
    uint nCountEnd;
    uint ambigCount;
};

struct variantPathNode
/* A list of these gives a path from phylo tree root to some node (usually user seq leaf). */
    {
    struct variantPathNode *next;
    char *nodeName;                   // Either a numeric internal node ID or the user seq name
    struct singleNucChange *sncList;  // One or more single nucleotide changes associated with node
    };

struct bestNodeInfo
    {
    struct bestNodeInfo *next;
    char *name;                           // Node name
    struct variantPathNode *variantPath;  // Mutations assigned to nodes along path from root->node
    boolean isSibling;                    // Placement would be as sibling of node (not child)
    };

struct baseVal
/* List of imputed base positions and values */
    {
    struct baseVal *next;
    int chromStart;         // 0-based position
    char *val;              // nucleotide base(s)
    };

struct placementInfo
/* Info about a sample's mutations and placement in the phylo tree */
    {
    char *sampleId;                       // Sample name from FASTA or VCF header
    struct slName *sampleMuts;            // Differences with the reference genome
    struct variantPathNode *variantPath;  // Mutations assigned to nodes along path from root
    struct bestNodeInfo *bestNodes;       // Other nodes identified as equally parsimonious
    struct baseVal *imputedBases;         // Ambiguous bases imputed to ref/alt [ACGT]
    int parsimonyScore;                   // Parsimony cost of placing sample
    int bestNodeCount;                    // Number of equally parsimonious placements
    };

struct subtreeInfo
/* Parsed subtree from usher and derivative products. */
    {
    struct subtreeInfo *next;
    struct tempName *subtreeTn;           // Newick file from usher (may have condensed nodes)
    struct phyloTree *subtree;            // Parsed subtree (#*** with annotated muts?  shoudl we?_
    struct hash *subtreeIdToIx;           // Map of subtree nodes to VCF output sample order
    struct slName *subtreeUserSampleIds;  // List of user-uploaded samples in this subtree
    struct slName *subtreeNameList;       // List of leaf names with nicer names for cond. nodes
    };

struct usherResults
/* Tree+samples download file, sample placements, and subtrees parsed from usher output. */
    {
    struct tempName *bigTreePlusTn;      // Newick file: original tree plus user's samples
    struct hash *samplePlacements;       // Info about each sample's placement in the tree
    struct subtreeInfo *subtreeInfoList; // For each subtree: tree, file, node info etc.
    };

struct tempName *vcfFromFasta(struct lineFile *lf, char *db, struct dnaSeq *refGenome,
                              boolean *informativeBases, struct slName **maskSites,
                              struct slName **retSampleIds, struct seqInfo **retSeqInfo,
                              struct slPair **retFailedSeqs, struct slPair **retFailedPsls,
                              int *pStartTime);
/* Read in FASTA from lf and make sure each item has a reasonable size and not too high
 * percentage of N's.  Align to reference, extract SNVs from alignment, and save as VCF
 * with sample genotype columns. */

struct usherResults *runUsher(char *usherPath, char *usherAssignmentsPath, char *vcfFile,
                              int subtreeSize, struct slName *userSampleIds,
                              struct hash *condensedNodes, int *pStartTime);
/* Open a pipe from Yatish Turakhia's usher program, save resulting big trees and
 * subtrees to trash files, return list of slRef to struct tempName for the trash files
 * and parse other results out of stderr output. */

void treeToAuspiceJson(struct subtreeInfo *sti, char *db, struct dnaSeq *ref,
                       char *bigGenePredFile, char *metadataFile, char *jsonFile);
/* Write JSON for tree in Nextstrain's Augur/Auspice V2 JSON format
 * (https://github.com/nextstrain/augur/blob/master/augur/data/schema-export-v2.json). */

struct tempName *writeCustomTracks(struct tempName *vcfTn, struct usherResults *ur,
                                   struct slName *sampleIds, struct phyloTree *bigTree,
                                   int fontHeight, int *pStartTime);
/* Write one custom track per subtree, and one custom track with just the user's uploaded samples. */


char *epiIdFromSampleName(char *sampleId);
/* If an EPI_ISL_# ID is present somewhere in sampleId, extract and return it, otherwise NULL. */

char *lineageForSample(char *db, char *sampleId);
/* Look up sampleId's lineage in epiToLineage file. Return NULL if we don't find a match. */

struct phyloTree *phyloPruneToIds(struct phyloTree *node, struct slName *sampleIds);
/* Prune all descendants of node that have no leaf descendants in sampleIds. */

char *phyloPlaceDbSetting(char *db, char *settingName);
/* Return a setting from hgPhyloPlaceData/<db>/config.ra or NULL if not found. */

char *phyloPlaceDbSettingPath(char *db, char *settingName);
/* Return path to a file named by a setting from hgPhyloPlaceData/<db>/config.ra,
 * or NULL if not found.  (Append hgPhyloPlaceData/<db>/ to the beginning of relative path) */

void reportTiming(int *pStartTime, char *message);
/* Print out a report to stderr of how much time something took. */

boolean hgPhyloPlaceEnabled();
/* Return TRUE if hgPhyloPlace is enabled in hg.conf and db wuhCor1 exists. */

char *phyloPlaceSamples(struct lineFile *lf, char *db, boolean doMeasureTiming, int subtreeSize,
                        int fontHeight);
/* Given a lineFile that contains either FASTA or VCF, prepare VCF for add_missing_samples;
 * if that goes well then run add_missing_samples, report results, make custom track files
 * and return the top-level custom track file; otherwise return NULL. */

#endif //_PHYLO_PLACE_H_
