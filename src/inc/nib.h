/* nib.h - interface to nucleotides stored 4 bits per base (so have
 * room for N. */
#ifndef NIB_H
#define NIB_H

#ifndef DNAUTIL_H
#include "dnautil.h"
#endif 

void nibOpenVerify(char *fileName, FILE **retFile, int *retSize);
/* Open file and verify it's in good nibble format. */

struct dnaSeq *nibLoadPart(char *fileName, int start, int size);
/* Load part of an .nib file. */

struct dnaSeq *nibLdPart(char *fileName, FILE *f, int seqSize, int start, int size);
/* Load part of an open .nib file. */

struct dnaSeq *nibLoadAll(char *fileName);
/* Load all of a nib file. */

void nibWrite(struct dnaSeq *seq, char *fileName);
/* Write out file in format of four bits per nucleotide. */

boolean isNib(char *fileName);
/* Return TRUE if file is a nib file. */
#endif /* NIB_H */

