/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* geneGraph - stuff that represents the alt splicing patterns possible for a
 * gene based on the mRNA evidence as a graph. */

#ifndef GENEGRAPH_H
#define GENEGRAPH_H

#ifndef GGMRNAALI_H
#include "ggMrnaAli.h"
#endif

#ifndef HGRELATE_H
#include "hgRelate.h"
#endif 

#ifndef ALTGRAPH_H
#include "altGraph.h"
#endif

 enum ggVertexType
 /* Classifies a vertex.  */
    {
    ggSoftStart,  /* First vertex - exact position unknown. */
    ggHardStart,  /* Start of a middle exon, position known. */      
    ggSoftEnd,    /* Last vertex - exact position unknown. */
    ggHardEnd,    /* End of a middle exon, position known. */
    ggUnused,     /* Vertex no longer part of graph. */
    };

struct ggVertex
/* A vertex in our gene graph. */
    {
    int position;	/* Where it is in genomic sequence. */
    UBYTE type;		/* vertex type. */
    };

struct ggAliInfo
/* mRNA alignment info as it pertains to graph generator. */
    {
    struct ggAliInfo *next;   /* Next in list. */
    struct ggVertex *vertices;   /* Splice sites or soft start/ends. */
    int vertexCount;             /* Vertex count. */
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
    char **mrnaRefs;                    /* names/ids of mrnas supporting this. */
    int mrnaRefCount;                   /* Count of mRNAs supporting this. */
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
 * the details of the last pass, so the structure
 * they use are hidden */


struct ggMrnaInput *ggGetMrnaForBac(char *bacAcc);
/* Read in clustering input from hgap database. */

struct ggMrnaCluster *ggClusterMrna(struct ggMrnaInput *ci);
/* Make a list of clusters from ci. */

struct geneGraph *ggGraphCluster(struct ggMrnaCluster *mc, struct ggMrnaInput *ci);
/* Make up a gene transcript graph out of the ggMrnaCluster. */

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

#endif /* GENEGRAPH_H */

