/* alignSeqSizes - Parse and store query and target sequence sizes for use
 * when converting or parsing alignments */

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef ALIGNSEQSIZES_H
#define ALIGNSEQSIZES_H

struct alignSeqSizes;

struct alignSeqSizes* alignSeqSizesNew(char *querySizesFile, char *querySizesStr,
                                       char *targetSizesFile, char *targetSizesStr);
/* Allocate a new object.  If *File arguments are not null, it is a tab
 * separated file of name and size.  If sizesStr is not null, it is a string
 * with whitespace-seperated pairs of name and size. */

void alignSeqSizesFree(struct alignSeqSizes** assPtr);
/* free an alignSeqSizes object */

int alignSeqSizesMustGetQuery(struct alignSeqSizes* ass, char *name);
/* Find size of a query sequence, or error if not found */

int alignSeqSizesMustGetTarget(struct alignSeqSizes* ass, char *name);
/* Find size of a query sequence, or error if not found */

#endif
