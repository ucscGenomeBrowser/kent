/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* nib.h - interface to nucleotides stored 4 bits per base (so have
 * room for N. */
#ifndef NIB_H
#define NIB_H

#ifndef DNAUTIL_H
#include "dnautil.h"
#endif 

#ifndef HASH_H
#include "hash.h"
#endif

/** Options for controlling masking  */
#define NIB_MASK_MIXED    0x01 /* Read uses case to indicate masking.
                                * Write sets mask bit for lower-case */
#define NIB_MASK_MAP      0x02 /* Read builds dnaSeq->mask bit map. Write
                                * uses mask to set mask bases.  Note: the
                                * bit map indicates which bases are not repeats
                                */
#define NIB_BASE_NAME     0x04 /* Return a sequence name that is the base name
                                * the file. */

void nibOpenVerify(char *fileName, FILE **retFile, int *retSize);
/* Open file and verify it's in good nibble format. */

struct dnaSeq *nibLoadPart(char *fileName, int start, int size);
/* Load part of an .nib file. */

struct dnaSeq *nibLoadPartMasked(int options, char *fileName, int start, int size);
/* Load part of an .nib file, with control over handling of masked positions */

struct dnaSeq *nibLdPart(char *fileName, FILE *f, int seqSize, int start, int size);
/* Load part of an open .nib file. */

struct dnaSeq *nibLdPartMasked(int options, char *fileName, FILE *f, int seqSize, int start, int size);
/* Load part of an open .nib file, with control over handling of masked
 * positions. */

struct dnaSeq *nibLoadAll(char *fileName);
/* Load all of a nib file. */

struct dnaSeq *nibLoadAllMasked(int options, char *fileName);
/* Load part of a .nib file, with control over handling of masked
 * positions. Subranges of nib files may specified in the file name
 * using the syntax:
 *    /path/file.nib:seqid:start-end
 * or\n"
 *    /path/file.nib:start-end
 * With the first form, seqid becomes the id of the subrange, with the second
 * form, a sequence id of file:start-end will be used.
 */

void nibWrite(struct dnaSeq *seq, char *fileName);
/* Write out file in format of four bits per nucleotide. */

void nibWriteMasked(int options, struct dnaSeq *seq, char *fileName);
/* Write out file in format of four bits per nucleotide, with control over
 * handling of masked positions. */

boolean nibIsFile(char *fileName);
/* Return TRUE if file is a nib file. */

boolean nibIsRange(char *fileName);
/* Return TRUE if file specifies a subrange of a nib file. */

void nibParseName(unsigned options, char *fileSpec, char *filePath,
                         char *name, unsigned *start, unsigned *end);
/* Parse the nib name, getting the file name, seq name to use, and
 * optionally the start and end positions. Zero is return for start
 * and end if they are not specified. Return the path to the file
 * and the name to use for the sequence. */

struct nibStream *nibStreamOpen(char *fileName);
/* Create a new nib stream.  Open file and stuff. */

void nibStreamClose(struct nibStream **pNs);
/* Close a nib stream.  Flush last nibble if need be.  Fix up header. */

void nibStreamOne(struct nibStream *ns, DNA base);
/* Write out one base to nibStream. */

void nibStreamMany(struct nibStream *ns, DNA *dna, int size);
/* Write many bases to nibStream. */

struct nibInfo
/* Info on a nib file. */
    {
    struct nibInfo *next;
    char *fileName;	/* Name of nib file. */
    int size;		/* Number of bases in nib. */
    FILE *f;		/* Open file. */
    };

struct nibInfo *nibInfoNew(char *path);
/* Make a new nibInfo with open nib file. */

void nibInfoFree(struct nibInfo **pNib);
/* Free up nib info and close file if open. */

struct nibInfo *nibInfoFromCache(struct hash *hash, char *nibDir, char *nibName);
/* Get nibInfo on nibDir/nibName.nib from cache, filling cache if need be. */

int nibGetSize(char* nibFile);
/* Get the number of nucleotides in a nib */

#endif /* NIB_H */

