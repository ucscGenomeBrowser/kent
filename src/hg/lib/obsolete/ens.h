/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* ens.h - Interface to ensEMBL database. */
#ifndef ENS_H
#define ENS_H

#ifndef DNAUTIL_H
#include "dnautil.h"
#endif 

#ifndef DLIST_H
#include "dlist.h"
#endif 

#ifndef UNFIN_H
#include "unfin.h"
#endif

struct ensAnalysis
/* A category of a feature. */
    {
    struct ensAnalysis *next;	/* Next in list */
    int id;			/* Unique id for this feature type. */
    char *db;			/* Database used. */
    char *dbVersion;		/* Version of database. */
    char *program;		/* Program used. */
    char *programVersion;	/* Version of program. */
    char *gffSource;		/* Source field from GFF. */
    char *gffFeature;		/* Feature field from GFF. */
    char *shortName;		/* 15 letter summary. */
    };

struct ensFeature
/* An ensemble feature. */
    {
    struct ensFeature *next;	   /* Next in list. */
    struct contigTree *tContig;    /* Name of target (genomic) sequence */
    int tStart, tEnd;              /* Position in genomic sequence. */
    int score;                     /* Score (I don't know units) */
    int orientation;               /* +1 or -1.  Strand relative to contig. */
    int type;                      /* Index into analysis table describing type of feature. */
    char *typeName;                /* Subtype of type really. May be NULL. Not alloced here. */ 
    int qStart, qEnd;              /* Query (cDNA, protein, etc.) sequence position. */
    char *qName;                   /* Query sequence name. */
    };

struct ensExon
/* An ensemble exon.  Since multiple transcripts can
 * use the same exon, this is stored as a reference on
 * a dlList in the transcript and as an instance in the
 * slList in the gene. */
    {
    struct ensExon *next;		/* Next in list (in ensGene) */
    char *id;				/* Ensemble ID (not allocated here). */
    struct contigTree *contig;	        /* Contig within clone this is in. (Not allocated here).*/
    char phase;				/* AKA Frame - codon position of 1st base. */
    char endPhase;                      /* Codon position of last base. */
    int orientation;                    /* +1 or -1. Strand relative to contig. */
    int seqStart;			/* Start position. */
    int seqEnd;				/* End position. */
    };

struct ensTranscript
/* A transcript (isoform) of a gene. */
    {
    struct ensTranscript *next;		/* Next in list. */
    char *id;				/* Ensemble ID. */
    struct dlList *exonList;		/* Ordered list of exon references. */
    struct ensExon *startExon;          /* Reference to first coding exon. */
    struct ensExon *endExon;            /* Reference to last coding exon. */
    int startSeq, endSeq;               /* Start, end of coding region. */
    };

struct ensGene
/* A gene.  A collection of exons and how they
 * are put together. */
    {
    struct ensGene *next;		  /* Next in list. */
    char *id;				  /* Ensemble ID with many zeroes. */
    struct ensTranscript *transcriptList; /* List of ways to transcribe and splice. */
    struct hash *exonIdHash;		  /* Fast lookup of exons from exon ids. */
    struct ensExon *exonList;		  /* Total exons in all transcripts. */
    };

void ensGetAnalysisTable(struct ensAnalysis ***retTable, int *retCount);
/* Returns analysis table (array of different things a feature can be). 
 * No need to free this, it's managed by system. */

struct dnaSeq *ensDnaInBacRange(char *clone, int start, int end, enum dnaCase dnaCase);
/* Get DNA for range of clone in browser coordinates, including NNNs between contigs. */

struct dnaSeq *ensDnaInBac(char *clone, enum dnaCase dnaCase);
/* Get DNA for clone in browser coordinates, including NNNs between contigs. */


struct ensFeature *ensGetFeature(char *featureId);
/* Get a single feature of the given ID.  Returns NULL if no such feature.  */

struct ensFeature *ensFeaturesInBac(char *clone);
/* Get list of features associated with BAC clone. */

struct ensFeature *ensFeaturesInBacRange(char *clone, int start, int end);
/* Get list of features associated a section of BAC clone. */

void ensFreeFeature(struct ensFeature **pFeature);
/* Free up a single feature. */

void ensFreeFeatureList(struct ensFeature **pFeatureList);
/* Free up a list of features. */



struct slName *ensGeneNamesInBac(char *bacName);
/* Get list of all gene names in bac. */

struct ensGene *ensGetGene(char *geneName);
/* Get named gene. This can also be viewed as a list of one genes. */

struct ensGene *ensGenesInBac(char *bacName);
/* Get list of all genes in bac. */

struct ensGene *ensGenesInBacRange(char *bacName, int start, int end);
/* Get list of genes in a section of a BAC clone.  The start/end are
 * in browser coordinates. */

void ensFreeGene(struct ensGene **pGene);
/* Free up a single gene. */

void ensFreeGeneList(struct ensGene **pGeneList);
/* Free up a list of genes. */



void ensParseContig(char *combined, char retBac[32], int *retContig);
/* Parse combined bac.contig into two separate values. */

int ensBrowserCoordinates(struct contigTree *contig, int x);
/* Return x in browser coordinates. */

int ensSubmitCoordinates(struct contigTree *contig, int x);
/* Return x in GenBank/EMBL submission coordinates. */

int ensBacBrowserLength(char *clone);
/* Return size of clone in browser coordinate space. */

int ensBacSubmitLength(char *clone);
/* Return size of clone in GenBank/EMBL submission  coordinate space. */

struct contigTree *ensBacContigs(char *bacId);
/* Return contigTree rooted at Bac.  Do not free this or modify it, 
 * the system takes care of it. */

struct contigTree *ensGetContig(char *contigId);
/* Return contig associated with contigId. Do not free this, system
 * takes care of it. */

void ensTranscriptBounds(struct ensTranscript *trans, int *retStart, int *retEnd);
/* Find beginning and end of transcript in browser coordinates. */

void ensGeneBounds(struct ensGene *gene, int *retStart, int *retEnd);
/* Find beginning and end of gene in browser coordinates. */

#endif /* ENS_H */


