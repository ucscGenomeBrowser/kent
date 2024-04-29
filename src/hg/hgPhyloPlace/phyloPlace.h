/* Place SARS-CoV-2 sequences in phylogenetic tree using add_missing_samples program. */

#ifndef _PHYLO_PLACE_H_
#define _PHYLO_PLACE_H_

#include "common.h"
#include "cart.h"
#include "dnaseq.h"
#include "hash.h"
#include "linefile.h"
#include "memgfx.h"
#include "parsimonyProto.h"
#include "phyloTree.h"
#include "seqWindow.h"
#include "trackLayout.h"
#include "trashDir.h"

#define PHYLOPLACE_DATA_DIR "hgPhyloPlaceData"

// Allow users to upload a lot of sequences, but put limits on how much detail we'll show and
// how many custom tracks we'll create.
#define MAX_SUBTREE_SIZE 5000
#define MAX_MICROBETRACE_SUBTREE_SIZE 500
#define MAX_SUBTREE_BUTTONS 5
#define MAX_SEQ_DETAILS 1000
#define MAX_SUBTREE_CTS 10

// For usher's -K option (single subtree):
#define SINGLE_SUBTREE_SIZE "2000"
#define USHER_NUM_THREADS "16"
#define USHER_SERVER_CHILD_TIMEOUT "600"
#define USHER_DEDUP_PREFIX "uploaded_"

#define NEXTSTRAIN_DRAG_DROP_DOC "https://docs.nextstrain.org/projects/auspice/en/latest/advanced-functionality/drag-drop-csv-tsv.html"
#define NEXTSTRAIN_URL_PARAMS "?f_userOrOld=uploaded%20sample"
#define MICROBETRACE_URL_PARAMS ""
#define OUTBREAK_INFO_URLBASE "https://outbreak.info/situation-reports?pango="
#define PANGO_DESIGNATION_ISSUE_URLBASE "https://github.com/cov-lineages/pango-designation/issues/"

// usher now preprends "node_" to node numbers when parsing protobuf, although they're still stored
// numeric in the protobuf.
#define USHER_NODE_PREFIX "node_"

struct treeChoices
/* Phylogenetic tree versions for the user to choose from. */
{
    char **protobufFiles;      // Mutation annotated tree files in protobuf format for UShER
    char **metadataFiles;      // Sample metadata a la GISAID's nextmeta download option
    char **sources;            // GISAID or public
    char **descriptions;       // Menu labels to describe the options to the user
    char **aliasFiles;         // Two-column files associating IDs/aliases with full tree names
    char **sampleNameFiles;    // One-column files with full tree names
    int count;                 // Number of choices (and size of each array)
};

struct seqInfo
/* User sequences, alignments and statistics */
{
    struct seqInfo *next;
    struct dnaSeq *seq;                     // Uploaded sequence
    struct psl *psl;                        // Alignment to reference (if FASTA uploaded)
    struct singleNucChange *sncList;        // SNVs in seq
    struct singleNucChange *maskedSncList;  // SNVs that were masked (not used for placement)
    struct slRef *maskedReasonsList;        // Reason (from Problematic Sites) for masking each SNV
    uint nCountStart;                       // #Ns at beginning of seq
    uint nCountMiddle;                      // #Ns not at beginning or end of seq
    uint nCountEnd;                         // #Ns at end of seq
    uint ambigCount;                        // # ambiguous IUPAC bases
    char *insRanges;                        // ranges and sequences inserted into reference
    char *delRanges;                        // ranges and sequences deleted from reference
    uint insBases;                          // total #bases inserted into reference
    uint delBases;                          // total #bases deleted from reference
};

struct variantPathNode
/* A list of these gives a path from phylo tree root to some node (usually user seq leaf). */
    {
    struct variantPathNode *next;
    char *nodeName;                   // Either a numeric internal node ID or the user seq name
    struct singleNucChange *sncList;  // One or more single nucleotide changes associated with node
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
                                          // -- NOTE: runUsher used to make this from stderr of
                                          // usher compiled with -DDEBUG=1; now caller must add it.
                                          // (struct seqInfo sncList has the same info)
    struct variantPathNode *variantPath;  // Mutations assigned to nodes along path from root
    struct baseVal *imputedBases;         // Ambiguous bases imputed to ref/alt [ACGT]
    int parsimonyScore;                   // Parsimony cost of placing sample
    int bestNodeCount;                    // Number of equally parsimonious placements
    char *nextClade;                      // Nextstrain clade assigned by UShER
    char *pangoLineage;                   // Pango lineage assigned by UShER
    // Fields above are parsed out of usher result files; below are added on later.
    char *nearestNeighbor;                // Nearest neighbor in phylogenetic tree/NULL if not found
    char *neighborLineage;                // Lineage of nearest neighbor/NULL if not found
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
    struct subtreeInfo *singleSubtreeInfo;  // Comprehensive subtree with all uploaded samples
    struct subtreeInfo *subtreeInfoList; // For each subtree: tree, file, node info etc.
    };

struct sampleMetadata
/* Information about a virus sample. */
{
    size_t columnCount;    // Number of metadata columns
    char **columnNames;    // Metadata column names (e.g. date, genbank_accession, pangolin_lineage)
                           // -- shared by all metadata rows, not allocated for each struct
    char **columnValues;   // Metadata column values -- allocated for each struct
    };

struct geneInfo
/* Information sufficient to determine whether a genome change causes a coding change. */
    {
    struct geneInfo *next;
    struct psl *psl;        // Alignment of transcript to genome
    struct dnaSeq *txSeq;   // Transcript sequence
    struct genbankCds *cds; // CDS (for those few pathogens that have transcript UTRs)
    int cdsStart;           // genePred cdsStart (genome coord, really cds end if - strand)
    int cdsEnd;             // genePred cdsEnd (genome coord, really cds start if - strand)
    };

struct tempName *vcfFromFasta(struct lineFile *lf, char *org, char *db, struct dnaSeq *refGenome,
                              struct slName **maskSites, struct hash *treeNames,
                              struct slName **retSampleIds, struct seqInfo **retSeqInfo,
                              struct slPair **retFailedSeqs, struct slPair **retFailedPsls,
                              int *pStartTime);
/* Read in FASTA from lf and make sure each item has a reasonable size and not too high
 * percentage of N's.  Align to reference, extract SNVs from alignment, and save as VCF
 * with sample genotype columns. */

struct usherResults *runUsher(char *org, char *usherPath, char *usherAssignmentsPath, char *vcfFile,
                              int subtreeSize, struct slName **pUserSampleIds,
                              struct treeChoices *treeChoices, int *pStartTime);
/* Open a pipe from Yatish Turakhia's usher program, save resulting big trees and
 * subtrees to trash files, return list of slRef to struct tempName for the trash files
 * and parse other results out of stderr output.  The usher-sampled version of usher might
 * modify userSampleIds, adding a prefix if a sample with the same name is already in the tree. */

struct usherResults *runMatUtilsExtractSubtrees(char *org, char *matUtilsPath, char *protobufPath,
                                                int subtreeSize, struct slName *sampleIds,
                                                struct treeChoices *treeChoices, int *pStartTime);
/* Open a pipe from Yatish Turakhia and Jakob McBroome's matUtils extract to extract subtrees
 * containing sampleIds, save resulting subtrees to trash files, return subtree results.
 * Caller must ensure that sampleIds are names of leaves in the protobuf tree. */

boolean serverIsConfigured(char *org);
/* Return TRUE if all necessary configuration settings are in place to run usher-sampled-server. */

boolean serverIsRunning(char *org, FILE *errFile);
/* Return TRUE if we can find a PID for server and that PID looks alive according to /proc. */

boolean startServer(char *org, struct treeChoices *treeChoices, FILE *errFile);
/* Start up an usher-sampled-server process to run in the background. */

void serverReloadProtobufs(char *org, struct treeChoices *treeChoices);
/* Send a reload command and list of protobufs for org to usher server. */

void serverStop(char *org);
/* Send stop command to usher server. */

void serverSetThreadCount(char *org, int val);
/* Send thread command and value to usher server. */

void serverSetTimeout(char *org, int val);
/* Send timeout command and value (in seconds) to usher server. */

char *microbeTraceHost();
/* Return the MicrobeTrace hostname from an hg.conf param, or NULL if missing. Do not free result. */

struct slPair *getAaMutations(struct singleNucChange *sncList, struct singleNucChange *ancestorMuts,
                              struct geneInfo *geneInfoList, struct seqWindow *gSeqWin);
/* Given lists of SNVs and genes, return a list of pairs of { gene name, AA change list }. */

struct geneInfo *getGeneInfoList(char *bigGenePredFile, struct dnaSeq *refGenome);
/* If config.ra has a source of gene annotations, then return the gene list. */

void treeToAuspiceJson(struct subtreeInfo *sti, char *org, char *db, struct geneInfo *geneInfoList,
                       struct seqWindow *gSeqWin, struct hash *sampleMetadata,
                       struct hash *sampleUrls, struct hash *samplePlacements,
                       char *jsonFile, char *source);
/* Write JSON for tree in Nextstrain's Augur/Auspice V2 JSON format
 * (https://github.com/nextstrain/augur/blob/master/augur/data/schema-export-v2.json). */

struct tempName *writeCustomTracks(char *org, char *ref, char *db,
                                   struct tempName *vcfTn, struct usherResults *ur,
                                   struct slName *sampleIds, char *source, int fontHeight,
                                   struct phyloTree *sampleTree, int *pStartTime);
/* Write one custom track per subtree, and one custom track with just the user's uploaded samples. */


struct sampleMetadata *metadataForSample(struct hash *sampleMetadata, char *sampleId);
/* Look up sampleId in sampleMetadata, by accession if sampleId seems to include an accession. */

struct phyloTree *phyloPruneToIds(struct phyloTree *node, struct slName *sampleIds);
/* Prune all descendants of node that have no leaf descendants in sampleIds. */

struct slPair *phyloPlaceOrgList(struct cart *cart);
/* Each subdirectory of PHYLOPLACE_DATA_DIR that contains an organism.ra file is a collection of
 * reference sequences that uploaded sequences will be matched against using nextclade sort.
 * Some of those references might also be dbs or track hub names (without the hub_number_ prefix).
 * Each subdirectory of PHYLOPLACE_DATA_DIR that contains a config.ra file contains a single
 * reference which might also be a db or track hub name (without the hub_number_ prefix).
 * Return a list of {name, label} pairs, SARS-CoV-2 first, combining the two categories. */

char *phyloPlaceOrgSetting(char *org, char *settingName);
/* Return cloned setting value if found in hgPhyloPlaceData/<org>/organism.ra or
 * old-style hgPhyloPlaceData/<org>/config.ra, or NULL if not found. */

char *phyloPlaceOrgSettingPath(char *org, char *settingName);
/* Return cgi-bin-relative path to a file named by a setting for org, or NULL if not found. */

char *phyloPlaceRefSetting(char *org, char *ref, char *settingName);
/* Return cloned setting value if found in hgPhyloPlaceData/<org>/<ref>/reference.ra or
 * old-style hgPhyloPlaceData/<ref>/config.ra, or NULL if not found. */

char *phyloPlaceRefSettingPath(char *org, char *ref, char *settingName);
/* Return cgi-bin-relative path to a file named by a setting from
 * hgPhyloPlaceData/<org>/<ref>/reference.ra or old-style hgPhyloPlaceData/<ref>/config.ra,
 * or NULL if not found. */

char *getNextcladePath();
/* Return hgPhyloPlaceData/nextclade if it exists, else errAbort. Do not free the returned value. */

struct treeChoices *loadTreeChoices(char *org, char *db);
/* If config specifies a treeChoices file, load it up, else return NULL. */

boolean isInternalNodeName(char *nodeName, int minNewNode);
/* Return TRUE if nodeName looks like an internal node ID from the protobuf tree, i.e. is numeric
 * or <USHER_NODE_PREFIX>_<number> and, if minNewNode > 0, number is less than minNewNode. */

void reportTiming(int *pStartTime, char *message);
/* Print out a report to stderr of how much time something took. */

boolean hgPhyloPlaceEnabled();
/* Return TRUE if hgPhyloPlace is enabled in hg.conf and db wuhCor1 exists. */

boolean phyloPlaceSamples(struct lineFile *lf, char *db, char *refName, char *defaultProtobuf,
                          boolean doMeasureTiming, int subtreeSize, struct trackLayout *tl,
                          struct cart *cart);
/* Given a lineFile that contains either FASTA, VCF, or a list of sequence names/ids:
 * If FASTA/VCF, then prepare VCF for usher; if that goes well then run usher, report results,
 * make custom track files.
 * If list of seq names/ids, then attempt to find their full names in the protobuf, run matUtils
 * to make subtrees, show subtree results.
 * Return TRUE if we were able to get at least some results for the user's input. */

#endif //_PHYLO_PLACE_H_
