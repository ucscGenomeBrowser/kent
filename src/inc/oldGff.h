/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* oldGff.h - This reads a *special* type of .GFF file.  
 * Some of the routines herein expect the gff file to include
 * both DNA and gene/intron/exon info.  They expect the
 * genes to be named by the group field. */
#ifndef OLDGFF_H
#define OLDGFF_H

struct gff
/* This is the structure that holds info on a gff file.
 */
    {
    char fileName[256];
    FILE *file;
    long fileSize;
    char buf[256];
    int bufSize;
    int bytesInBuf;
    int readIx;
    int lineNumber;
    struct lm *memPool;
    char *dna;
    long dnaSize;
    char dnaName[128];
    struct gffGene *genes;
    };

/* An intron or exon - just an offset into a DNA array. */
struct gffSegment
    {
    struct gffSegment *next;	/* This must be first field! */
    long start, end;
    int frame;
    };
typedef struct gffSegment GffIntron;
typedef struct gffSegment GffExon;

struct gffGene
/* At the end of a GFF file are a number of genes, each of which 
 * is a list of exons/introns. */
    {
    struct gffGene *next;	/* This must be first field! */
    long start, end;
    int frame;
    char strand;	/* + or - */
    char name[64];
    GffExon *exons;
    GffIntron *introns;
    DNA *dna;
    long dnaSize;
    };


boolean gffOpen(struct gff *gff, char *fileName);
/* Initialize gff structure and open file for it. */

boolean gffOpenAndRead(struct gff *gff, char *fileName);
/* Open up gff file and read everything in it. */

void gffClose(struct gff *gff);
/* Close down gff structure. */

boolean gffReadDna(struct gff *gff);
/* Read all the DNA in a file. */

struct gffGene *gffFindGene(struct gff *gff, char *geneName);
/* Find gene with given name.  Case sensitive. */

struct gffGene *gffFindGeneIgnoreCase(struct gff *gff, char *geneName);
/* Find gene with given name.  Not case sensitive. */

void gffPrintInfo(struct gff *gff, FILE *out);
/* Print summary info about file. */

boolean gffReadGenes(struct gff *gff);
/* Read all the gene (as opposed to base) info in file. */

struct gffGene *gffDupeGene(struct gff *gff, struct gffGene *oldGene);
/* Make a duplicate of gene (with it's own DNA). gffFreeGene it when done. */

struct gffGene *gffDupeGeneAndSurrounds(struct gff *gff, struct gffGene *oldGene,
    int leftExtra, int rightExtra);
/* Make a duplicate of gene with extra DNA around coding region. 
 * gffFreeGene it when done. */

struct gffGene *gffGeneWithOwnDna(struct gff *gff, char *geneName);
/* Find gene with given name.  Case sensitive. */

void gffFreeGene(struct gffGene **pGene);
/* Free a gene returned with dupeGene or geneWithOwnDna. 
 * (You don't want to free the ones returned by findGene,
 * they are still owned by the gff.)
 */

struct dnaSeq *gffReadDnaSeq(char *fileName);
/* Open gff file and read DNA sequence from it. */

#endif /* GFF_H */

