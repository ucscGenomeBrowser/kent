/* chromInserts - this module helps handle centromeres, heterochromatic regions
 * and other large gaps in chromosomes. */

/* Copyright (C) 2003 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef CHROMINSERTS_H
#define CHROMINSERTS_H

struct bigInsert
/* Describes a big insertion. */
    {
    struct bigInsert *next;	/* Next in list. */
    char *ctgBefore;            /* Contig before insert (or NULL) */
    char *ctgAfter;             /* Contig after insert (or NULL) */
    int size;                   /* Insert size in bases. */
    char *type;			/* 'centromere' 'short_arm' etc. */
    struct chromInserts *chrom; /* Pointer to associated chromosome. */
    };

struct chromInserts
/* List of inserts in a chromosome. */
    {
    struct chromInserts *next;	  /* Next in list. */
    char *chrom;		  /* Chromosome name, allocated in hash. */
    struct bigInsert *terminal;   /* Insert to finish chromosome with or NULL. */
    struct bigInsert *insertList; /* List of contigs to insert before. */
    };

struct chromInserts *chromInsertsRead(char *fileName, struct hash *insertsHash);
/* Read in inserts file and process it. */

int chromInsertsGapSize(struct chromInserts *chromInserts, char *contig, boolean isFirst);
/* Return size of gap before next contig. */

void chromInsertsSetDefaultGapSize(int size);
/* Set default gap size. */

struct bigInsert *bigInsertBeforeContig(struct chromInserts *chromInserts, 
	char *contig);
/* Return the big insert (if any) before contig) */

#endif /*  CHROMINSERTS_H */

