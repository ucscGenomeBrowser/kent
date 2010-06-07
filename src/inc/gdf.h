/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* gdf - Intronerator Gene Description File. */

#ifndef GDF_H
#define GDF_H

#ifndef DNAUTIL_H
#include "dnautil.h"
#endif

struct gdfDataPoint
/* This stores data at each exon/intron boundary. */
    {
    int start;
    };

struct gdfGene
/* One structure of these for each gene (each isoform of each gene
 * actually. */
    {
    struct gdfGene *next;
    char *name;
    int dataCount;
    struct gdfDataPoint *dataPoints;
    char strand;
    UBYTE chromIx;
    };

struct gdfGene *newGdfGene(char *name, int nameSize, int exonCount, char strand, UBYTE chromIx);
/* Return a new gene. */

void gdfFreeGene(struct gdfGene *gene);
/* Free a gene. */

void gdfFreeGeneList(struct gdfGene **pList);
/* Free a whole list of genes. */

struct gdfGene *gdfReadOneGene(FILE *f);
/* Read one entry from a Gdf file.  Assumes that the file pointer
 * is in the right place. */

void gdfGeneExtents(struct gdfGene *gene, long *pMin, long *pMax);
/* Figure out first and last base in gene. */

void gdfOffsetGene(struct gdfGene *gene, int offset);
/* Add offset to each point in gene */

void gdfRcGene(struct gdfGene *gene, int size);
/* Flip gene to other strand. Assumes dataPoints are already
 * moved into range from 0-size */

void gdfUpcExons(struct gdfGene *gene, int geneOffset, DNA *dna, int dnaSize, int dnaOffset);
/* Uppercase exons in DNA. */


#endif /* GDF_H */

