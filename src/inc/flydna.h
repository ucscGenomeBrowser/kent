/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* flydna.h - routines for accessing fly genome and cDNA sequences. */

#ifndef FLYDNA_H
#define FLYDNA_H

void flyLoadNt4Genome(struct nt4Seq ***retNt4Seq, int *retNt4Count);
/* Load up entire packed fly genome into memory. */

void flyFreeNt4Genome(struct nt4Seq ***pNt4Seq);
/* Free up packed fly genome. */

void flyChromNames(char ***retNames, int *retNameCount);
/* Get list of fly chromosome names. */

boolean flyCdnaSeq(char *name, struct dnaSeq **retDna, struct wormCdnaInfo *retInfo);
/* Get a single fly cDNA sequence. Optionally (if retInfo is non-null) get additional
 * info about the sequence. */

char *flyFeaturesDir();
/* Return the features directory. (Includes trailing slash.) */

FILE *flyOpenGoodAli();
/* Opens good alignment file and reads signature. 
 * (You can then cdaLoadOne() it.) */

#endif /* FLYDNA_H */

