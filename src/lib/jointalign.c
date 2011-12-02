/* jointalign.c - routines for printing a joint alignment in html. 
 *
 * This file is copyright 2002 Ryan Weber, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "errabort.h"
#include "jointalign.h"



void htmlPrintJointAlignment( char *seq1, char *seq2, int columnNum, 
        int start, int end, char *strand )
/* Print sequences 1 and 2 (assumed to be a joint alignment),
 * formatted for html output. Coordinates are printed based on
 * the start and end positions and oriented according to the
 * strand the sequences are on (+ or -). (NO COORDINATES YET)*/
{
int i;
validateSeqs( seq1, seq2 );

/*print the sequences with lines connecting identical residues
 *in columns of size columnNum*/
for( i=0; i<strlen(seq1); i += columnNum )
    htmlPrintJointAlignmentLine(seq1, seq2, i, min(i+columnNum, strlen(seq1)));

/*printf( "<tt><hr><br>%s<br>%s<br></tt>", seq1, seq2 );*/

}

void htmlPrintJointAlignmentLine( char *seq1, char *seq2, int start, int end)
/* Prints one line of the joint alignment between seq1 and seq2,
 * from seq[start] to seq[end-1].*/
{

int i;
printf("<tt>");
for( i=start; i<end; i++ )
    printf("%c",seq1[i]);
printf("<br>");
for( i=start; i<end; i++ )
    {
    if(ucaseMatch( seq1[i], seq2[i] ))
        printf("|");
    else
        printf("&nbsp;");
    }
printf("<br>");
for( i=start; i<end; i++ )
    printf("%c",seq2[i]);
printf("</tt>");
printf("<br><br>");
   

}

boolean ucaseMatch( char a, char b )
/* Case insensitive character matching */
{
if( toupper( a ) == toupper( b ) )
    return( TRUE );
else 
    return( FALSE );
}

void validateSeqs( char *seq1, char *seq2 )
/*Make sure sequences are the same length*/
{
if( strlen(seq1) != strlen(seq2) )
    {
    printf("%s<br>%s<br>", seq1, seq2 );
    errAbort("The sequences are not properly aligned (different lengths)<br>\n"); 
    }
}
