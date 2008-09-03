/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* geneGraph - stuff that represents the alt splicing patterns possible for a
 * gene based on the mRNA evidence as a graph. This is used by the altSplice
 * program.  See also src/hg/txGraph/txBedToGraph for a more recent, faster,
 * more memory efficient approach. */

#ifndef GENEGRAPH_H
#define GENEGRAPH_H

#ifndef GGMRNAALI_H
#include "ggMrnaAli.h"
#endif

#ifndef ALTGRAPH_H
#include "altGraph.h"
#endif

#ifndef ALTGRAPHX_H
#include "altGraphX.h"
#endif

#ifndef JKSQL_H
#include "jksql.h"
#endif

#ifndef GGTYPES_H
#include "ggTypes.h"
#endif

struct ggVertex
/* A vertex in our gene graph. */
    {
    int position;	/* Where it is in genomic sequence. */
    UBYTE type;		/* vertex type. */
    };

struct ggEvidence
/* An id of an mrna in mrnaRefs */
{
    struct ggEvidence *next;   /* Next in list. */
    int id;                    /* Index into mrnaRefs of supporting mRNA. */
    int start,end;	       /* Bounds of evidence in chrom coordinates. */
};

struct ggEdge
/* An edge of two vertices */
{
    struct ggEdge *next;       /* Next in list. */
    int vertex1;               /* Index of vertex 1 in vertices array. */
    int vertex2;               /* Index of vertex 2 in vertices array. */
    enum ggEdgeType type;      /* Type of vertex, ggExon, ggIntron, etc. */
};
    

struct ggAliInfo
/* mRNA alignment info as it pertains to graph generator. */
    {
    struct ggAliInfo *next;   /* Next in list. */
    struct ggVertex *vertices;   /* Splice sites or soft start/ends. */
    int vertexCount;             /* Vertex count. */
    struct ggMrnaAli *ma;	/* Associated mrnaInfo. */
    };

struct ggMrnaCluster
/* Input structure for gene grapher */
{
    struct ggMrnaCluster *next;             /* Next in list. */
    struct maRef *refList;                  /* Full cDNA alignments. */
    struct ggAliInfo *mrnaList;             /* List of compact cDNA alignments. */
    char *tName;                            /* target name, usually chrom. */
    int tStart,tEnd;                        /* Position in genomic DNA. */
    char strand[3];                         /* + or - depending on strand */
    struct dnaSeq *genoSeq;                 /* target sequence defined by coordinates */
};

struct geneGraph
/* A graph that represents the alternative splicing paths of a gene. */
{
    struct geneGraph *next;		/* Next in list. */
    struct ggVertex *vertices;          /* Splice sites or soft start/ends. */
    int vertexCount;                    /* Vertex count. */
    bool **edgeMatrix;	                /* Adjacency matrix for directed graph. */
    char *tName;                        /* name of target, usually chromosom */
    unsigned int tStart;                /* start of graph in target. */
    unsigned int tEnd;                  /* end of graph in target. */
    char strand[3];                     /* + or - */
    int mrnaRefCount;                   /* Count of mRNAs supporting this. */
    char **mrnaRefs;                    /* names/ids of mrnas supporting this. */
    char **mrnaTypes;			/* Types for mrnaRefs. */
    int *mrnaTissues;                   /* tissues from which mrnas originated, 0 if unknown. Indexes into tissue table */
    int *mrnaLibs;                       /* libraries from which mrnas originated, 0 if unknown, Indexes into library table */
    struct ggEvidence ***evidence;       /* Like adjacency matrix except each point is an linked list of supporting evidence */

};

void freeDenseAliInfo(struct ggAliInfo **pDa);
/* Free up a ggAliInfo */

void freeGeneGraph(struct geneGraph **pGg);
/* Free up a gene graph. */


/* The next three routines are the three main passes of
 * the algorithm that generates the graphs from mRNA
 * alignments.    In the first pass all the 
 * relevant mRNA alignments (perhaps all that align to
 * a BAC clone) are collected, and the ESTs that are
 * opposite reads of the same clone are paired.  Second
 * alignments that have overlapping exons are clustered
 * together.  Third a graph of exon endpoints for the
 * cluster is generated.  In alternative splicing
 * situations this graph can be traversed in multiple
 * ways that correspond to different possible transcripts.
 *
 * These three passes correspond to the next three
 * functions.  Generally you only need to look at
 * the details of the last pass, so the structures
 * they use are hidden */


struct ggMrnaInput *ggGetMrnaForBac(char *bacAcc);
/* Read in clustering input from hgap database. The bacAcc parameter
 * is more likely to be a chromosome these days. */

struct ggMrnaCluster *ggClusterMrna(struct ggMrnaInput *ci);
/* Make a list of clusters from ci. */

struct geneGraph *ggGraphCluster(char *genomeDb, struct ggMrnaCluster *mc, struct ggMrnaInput *ci);
/* Make up a gene transcript graph out of the ggMrnaCluster. */

struct geneGraph *ggGraphConsensusCluster(char *genomeDb, struct ggMrnaCluster *mc, struct ggMrnaInput *ci, 
					  struct hash *tissLibHash, boolean fillInEvidence);
/* Make up a gene transcript graph out of the ggMrnaCluster. Only
 extending truncated exons to consensus splice sites. */

struct ggMrnaCluster *ggMrnaSoftClusterOfOne(struct ggMrnaAli *ma, 
	struct dnaSeq *genoSeq);
/* Make up a ggMrnaCluster with just one thing on it. */
/* Defined in ggCluster. */

struct ggMrnaCluster *ggMrnaSoftFilteredClusterOfOne(struct ggMrnaAli *ma,
        struct dnaSeq *genoSeq, int minExonSize, int minNonSpliceExon);
/* Make up a ggMrnaCluster with just one thing on it.
 * All edges here will be soft (not intended for alt-splicing use) */

void ggMrnaClusterMerge(struct ggMrnaCluster *aMc, struct ggMrnaCluster *bMc);
/* Merge bMc into aMc.  Afterwords aMc will be bigger and
 * bMc will be gone. */



void freeGgMrnaInput(struct ggMrnaInput **pCi);
/* Free up a ggMrnaInput. */

void ggFreeMrnaClusterList(struct ggMrnaCluster **pMcList);
/* Free a list of mrna clusters. */

typedef void (*GgTransOut)(struct ggAliInfo *da, int cStart, int cEnd); 
/* Typedef for function called to output constraint as graph is traversed.
 * "da" parameter represents a single transcript off of graph. */

void dumpGgAliInfo(struct ggAliInfo *da, int start, int end);
/* Display list of dense Alis.  This may be used for a simple text
 * output format in traverseGeneGraph. */

void traverseGeneGraph(struct geneGraph *gg, int cStart, int cEnd, GgTransOut output);
/* Transverse graph and call output for each transcript. */

void ggTabOut(struct geneGraph *gg, FILE *f);
/* Convert into database format and save as line in tab-delimited file. */

struct geneGraph *ggFromRow(char **row);
/* Create a geneGraph from a row in altGraph table. */

struct altGraph *ggToAltGraph(struct geneGraph *gg);
/* convert a gene graph to an altGraph data structure */

boolean isSameGeneGraph(struct geneGraph *gg1, struct geneGraph *gg2);
/* returns true if the gene graphs are the same, otherwise returns false */

struct altGraphX *ggToAltGraphX(struct geneGraph *gg);
/* convert a gene graph to an altGraph data structure */

struct geneGraph *altGraphXToGG(struct altGraphX *ag);
/* Convert an altGraphX to a geneGraph. Free with freeGeneGraph */

void ggEvidenceFree(struct ggEvidence **pEl);
/* Free a single dynamically allocated ggEvidence */

void ggEvidenceFreeList(struct ggEvidence **pList);
/* Free a list of dynamically allocated ggEvidence's */

void ggEvidenceDebug(struct geneGraph *gg);
/* dump out the edge matrix and evidence matrix for a genegraph */

boolean matricesInSync(struct geneGraph *gg);
/* return TRUE if edge and evidence matrices are in synch, FALSE otherwise */

boolean checkEvidenceMatrix(struct geneGraph *gg);
/* check to make sure that no edge has more weight than possible */

void ggFillInTissuesAndLibraries(struct geneGraph *gg, struct hash *tissLibHash,
				 struct sqlConnection *conn);
/* Load up the library and tissue information for mrnas from the mrna table. 
   If the tissLibHash != NULL use it to find the library and tissue. They
   will be stored as an slInt list keyed by the accessions. */

struct ggEdge *ggFindCassetteExons(struct geneGraph *gg);
/* return a list of edges that appear to be cassette exons */

struct ggEdge *ggCreateEdge(int v1, int v2, int type);
/* create and return and graph edge, free with freez(). */

boolean ggIsCassetteExonEdge(struct geneGraph *gg, int vertex1, int vertex2);
/* Return TRUE if there is evidence that this exon is optional
 * or FALSE otherwise.  */

void ggEvAddHeadWrapper(struct ggEvidence **list, struct ggEvidence **el);
/* Wrapper to avoid getting more than maxEvidence pieces of evidence on an
   edge. maxEvidence is enough that I believe it.*/

#endif /* GENEGRAPH_H */

