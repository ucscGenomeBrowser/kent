/* FASTA I/O for genbank update.  This supports:
 *   - reading and writting compressed files
 *   - tracking offsets of records in files
 *   - optimized selective copying of records.
 */

/* Copyright (C) 2005 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef GBFA_H
#define GBFA_H

struct gbFa
/* struct associated with an open fasta file */
{
    char fileName[PATH_LEN];   /* name of file */
    FILE *fh;                  /* open file */
    void* fhBuf;               /* stdio buffer */
    char mode[2];              /* reading or writing? */
    off_t recOff;              /* offset of record in file */
    unsigned recLineNum;       /* line number of record */
    off_t off;                 /* current offset in file */
    char *id;                  /* sequence id */
    char *comment;             /* comment from header */
    char *headerBuf;           /* buffer for chopped header line */
    unsigned headerCap;        /* memory capacity of headerBuf */
    char *seq;                 /* sequence string, NULL if not read */
    unsigned seqLen;           /* length of data in seq */
    char *seqBuf;              /* buffer for seq */
    unsigned seqCap;           /* memory capacity of seqBuf */
};

struct gbFa* gbFaOpen(char *fileName, char* mode);
/* open a fasta file for reading or writing.  Uses atomic file
 * creation for write.*/

void gbFaClose(struct gbFa **gbPtr);
/* close a fasta file, check for any undetected I/O errors. */

boolean gbFaReadNext(struct gbFa *fa);
/* read the next fasta record header. The sequence is not read until
 * gbFaGetSeq is called */

char* gbFaGetSeq(struct gbFa *fa);
/* Get the sequence for the current record, reading it if not already
 * buffered */

void gbFaWriteSeq(struct gbFa *fa, char *id, char *comment, char *seq, int len);
/* write a sequence, comment maybe null, len is the length to write, or -1
 * for all of seq. */

void gbFaWriteFromFa(struct gbFa *fa, struct gbFa *inFa, char *newHdr);
/* Write a fasta sequence that is buffered gbFa object.  If newHdr is not
 * null, then it is a replacement header, not including the '>' or '\n'.  If
 * null, the header in the inFa is used as-is.
 */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
