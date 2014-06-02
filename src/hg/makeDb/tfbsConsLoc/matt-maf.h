// matt-maf.h - Multiple alignment format, header file stolen from Jim Kent and
// abused

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include <stdio.h>

struct mafFile
/* A file full of multiple alignments. */
    {
    struct mafFile *next;
    int version;		// Required
    char *scoring;		// Name of scoring scheme.
    struct mafAli *alignments;	// Possibly empty list of alignments.
    char *fileName;
    int line_nbr;
    FILE *fp; 			// Open file if any. NULL except while parsing.
    };

void mafFileFree(struct mafFile **pObj);
/* Free up a maf file including closing file handle if necessary. */

struct mafAli
/* A multiple alignment. */
    {
    struct mafAli *next;
    double score;
    struct mafComp *components;	/* List of components of alignment */
    int textSize;	 /* Size of text in each component. */
    };

void mafAliFree(struct mafAli **pObj);
/* Free up a maf alignment. */

struct mafComp
/* A component of a multiple alignment. */
    {
    struct mafComp *next;
    char *src;	 /* Name of sequence source.  */
    int srcSize; /* Size of sequence source.  */
    char strand; /* Strand of sequence.  Either + or -*/
    int start;	 /* Start within sequence. Zero based. If strand is - is relative to src end. */
    int size;	 /* Size in sequence (does not include dashes).  */
    char *text;  /* The sequence including dashes. */
    };

void mafCompFree(struct mafComp **pObj);
/* Free up a maf component. */

struct mafFile *mafOpen(char *fileName);
/* Open up a .maf file for reading.  Read header and
 * verify. Prepare for subsequent calls to mafNext().
 * Prints error message and aborts if there's a problem. */

struct mafAli *mafNext(struct mafFile *mafFile);
/* Return next alignment in file or NULL if at end. 
 * This will close the open file handle at end as well. */

struct mafFile *mafReadAll(char *fileName);
/* Read in full maf file */

void mafWriteStart(FILE *f, char *scoring);
/* Write maf header and scoring scheme name (may be null) */

void mafWrite(FILE *f, struct mafAli *maf);
/* Write next alignment to file. */







