/*****************************************************************************
 * Copyright (C) 2002 Ryan Weber.  This source code may be freely used       *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Ryan Weber (weber@cse.ucsc.edu) *
 *****************************************************************************/
/* jointalign.h - routines for printing a joint alignment in html. */

#include "common.h"
#include "jointalign.h"

void htmlPrintJointAlignment( char *seq1, char *seq2, int columnNum, 
        int start, int end, char *strand )
/* Print sequences 1 and 2 (assumed to be a joint alignment),
 * formatted for html output. Coordinates are printed based on
 * the start and end positions and oriented according to the
 * strand the sequences are on (+ or -).*/
{

printf("<tt>%s</tt> &nbsp;&nbsp;<br>", seq1 );
printf("<tt>%s</tt> &nbsp;&nbsp;<br>", seq2 );

}
