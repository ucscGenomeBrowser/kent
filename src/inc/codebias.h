/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* codebias.h - stuff for managing codon bias and finding frame. */
#ifndef CODEBIAS_H
#define CODEBIAS_H

#ifndef DNAUTIL_H
#include "dnautil.h"
#endif


struct codonBias
/* Tendency of codons to occur in a particular region. 
 * Tables are in scaled log probability format. */
    {
    int mark0[64];
    int mark1[64][64];
    };

struct codonBias *codonLoadBias(char *fileName);
/* Create scaled log codon bias tables based on .cod file.  
 * You can freeMem it when you're done. */

 int codonFindFrame(DNA *dna, int dnaSize, struct codonBias *forBias);
/* Assuming a stretch of DNA is an exon, find most likely frame that it's in. 
 * Beware this routine will replace N's with T's in the input dna.*/


#endif /* CODEBIAS_H */

