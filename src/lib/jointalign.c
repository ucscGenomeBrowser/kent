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


void flyFaCommentIntoInfo(char *faComment, struct wormCdnaInfo *retInfo)
/* Process line from .fa file containing information about cDNA into binary
 * structure. */
{
if (retInfo)
    {
    char *s;
    zeroBytes(retInfo, sizeof(*retInfo));
    /* Separate out first word and use it as name. */
    s = strchr(faComment, ' ');
    if (s != NULL)
	    *s++ = 0;
    retInfo->name = faComment+1;
    retInfo->motherString = faComment;
	s = strrchr(retInfo->name, '.');
	retInfo->orientation = '+';
	if (s != NULL)
		retInfo->orientation = (s[1] == '3' ? '-' : '+');
    }
}

