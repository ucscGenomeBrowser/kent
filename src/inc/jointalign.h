/*****************************************************************************
 * Copyright (C) 2002 Ryan Weber.  This source code may be freely used       *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Ryan Weber (weber@soe.ucsc.edu) *
 *****************************************************************************/
/* jointalign.h - routines for printing a joint alignment in html. */

#ifndef JOINTALIGN_H
#define JOINTALIGN_H

void htmlPrintJointAlignment( char *seq1, char *seq2, int columnNum, 
        int start, int end, char *strand );
/* Print sequences 1 and 2 (assumed to be a joint alignment),
 * formatted for html output. Coordinates are printed based on
 * the start and end positions and oriented according to the
 * strand the sequences are on (+ or -).*/

boolean ucaseMatch( char a, char b );
/* Case insensitive character matching */

void validateSeqs( char *seq1, char *seq2 );
/*Make sure sequences are the same length*/

void htmlPrintJointAlignmentLine( char *seq1, char *seq2, int start, int end);
/* Prints one line of the joint alignment between seq1 and seq2,
 * from seq[start] to seq[end-1].*/


#endif /* JOINTALIGN */

