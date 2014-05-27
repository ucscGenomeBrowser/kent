/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */


/*  Last edited: Jun 23 19:33 1995 (gos) */
/* 
  cpg.c: CpG island finder. Larsen et al Genomics 13 1992 p1095-1107
  "usually defined as >200bp with %GC > 50% and obs/exp CpG >
  0.6". Here use running sum rather than window to score: if not CpG
  at postion i, then decrement runSum counter, but if CpG then runSum
  += CPGSCORE.     Spans > threshold are searched
  for recursively.
  
  filename: fasta format concatenated sequence file: 
*/

/*********************************************************


put in options: -p debug to switch on commented out print statements
                -l length threshold
                -t score threshold
                -s to specify score
		-d score (-ve) if not found : default = -1.
		
Really want it to take pattern file:
     Pattern  score_if_found  score_if_not_found

How best to give #pattern hits, %GC in span, o/e ?
Cleanest:
Provide using separate scripts called using output.

Add -g or -m option for "max gap" as in Qpatch.c - score not increased
if reaches this threshold.  This means that patches which are separated
by more than the threshold are never joined.

*********************************************************/

#include "stdio.h"
#include "math.h"
#include "readseq.h"

/********************* CONTROLS ********************/
#define CPGSCORE  17   /* so that can compare with old cpg - this
			  had CPGSCORE 27, but allowed score to reach
			  0 once without reporting */
#define MALLOCBLOCK 200000
#define MAXNAMELEN 512  /*"length 0, title > MAXNAMELEN or starts with bad character." if too small */

/*------------------------------------------------------*/
int readSequence (FILE *fil, int *conv,
                         char **seq, char **id, char **desc, int *length) ;

int findspans ( int start, int end, char *seq, char *seqname ) ;

void getstats ( int start, int end, char *seq, char *seqname, int *ncpg, int *ngpc, int *ngc ) ;

void usage (void)
{ 
  fprintf (stderr, "Usage: cpg seqfilename\n") ;
  exit (-1) ;
}
/*------------------------------------------------------*/
void main (int argc, char **argv)
{ 
  
  char *seq, *seqname, *desc ;
  int conv[] =   { 0, 1, 2, 3, 4 } ;
  
  int length ;
  int i ;
  static FILE *fil ;

  char c, *cp ;
  extern char* malloc() ;

  /*------------------------------------------------------*/  
  switch ( argc )
    {
    default: if (argc != 2)
      usage () ;
    }
     if (!(seqname = malloc (MAXNAMELEN+1)))
     { fprintf (stderr, "Couldn't malloc %d bytes", MAXNAMELEN) ;
     exit (-1) ;
     }
     
     if (!(seq = malloc (MALLOCBLOCK+1)))
       { fprintf (stderr, "Couldn't malloc %d bytes", MALLOCBLOCK) ;
	 exit (-1) ;
       }

  if (!(fil = fopen ( argv[1], "r" ))) 
    usage ();
  
  while ( readSequence(fil, dna2textConv, &seq, &seqname, &desc, &length) ) 
    /* once through per sequence */
    { 
      i = 0 ;
      while ( seqname[i] != ' ' && seqname[i] != '\0' && i < 256 )
	++i ;
      seqname[i] = '\0' ;

      findspans ( 0, length, seq, seqname ) ;
    }

  exit (0);
}

int findspans ( int start, int end, char *seq, char *seqname )
{
  int i ;
  int sc = 0 ;
  int lsc = 0 ; 
  int imn = -1 ;  /* else sequences which start with pattern reported badly */
  int imx = 0 ;
  int mx = 0 ;

  int ncpg, ngpc, ngc ;  

  i = start ;
  while ( i < end )  
    {
      lsc = sc ;
      sc = ( end-1-i && seq[i]=='C' && seq[i+1]=='G' ) ? sc += CPGSCORE : --sc ;
      sc = sc < 0 ? 0 : sc ;
/*      printf("%d \t %d \t%d \t %d \t%d \t%d\n", i, sc, lsc, imn, imx, mx) ; */
      if ( sc == 0 && lsc )  /* should threshold here */
	{
	  /* imn+1 is the start of the match. 
	     imx+1 is the end of the match, given pattern-length=2.
	     fetchdb using reported co-ordinates will return a sequence
	     which both starts and ends with a complete pattern.
	     Further +1 adjusts so that start of sequence = 1 
	  */

	  getstats ( imn+1, imx+2, seq, seqname, &ncpg, &ngpc, &ngc ) ;
	  printf("%s\t %d\t %d\t %d\t CpG: %d\t %.1f\t %.1f\n", seqname, imn+2, imx+2, mx, ncpg, ngc*100.0/(imx+1-imn), 1.0*ncpg/ngpc) ; 
	  
/* 	  printf("%s \t %d\t %d\t %d \n", seqname, imn+2, imx+2, mx ) ; 
  */
	  /* Recursive call searches from one position after the end of the 
	     last match to the current position */
	  findspans( imx+2, i, seq, seqname ) ;
	  sc = lsc = imn = imx =  mx = 0 ;
	}
      imx = sc > mx ? i : imx ;
      mx = sc > mx ? sc : mx ;
      imn = sc == 0 ? i : imn ;
      ++i ;
    }
  if ( sc != 0 )  /* should threshold here */
    {
/*      printf("%d \t %d \t%d \t %d \t%d \t%d\n", i, sc, lsc, imn, imx, mx) ;  */
      
      getstats ( imn+1, imx+2, seq, seqname, &ncpg, &ngpc, &ngc ) ;
      printf("%s\t %d\t %d\t %d\t CpG: %d\t %.1f\t %.2f\n", seqname, imn+2, imx+2, mx, ncpg, ngc*100.0/(imx+1-imn), 1.0*ncpg/ngpc) ; 
      
/*      printf("%s \t %d\t %d\t %d \n", seqname, imn+2, imx+2, mx ) ; 
   */
      findspans( imx+2, end, seq, seqname ) ;
    }
}

void getstats ( int start, int end, char *seq, char *seqname, int *ncpg, int *ngpc, int *ngc )
{

int i ;
*ncpg = *ngpc = *ngc = 0 ;

for ( i = start ; i < end ; ++i ) 
  {
   if ( end-1-i && seq[i]=='C' && seq[i+1]=='G' ) ++*ncpg ; 
   if ( end-1-i && seq[i]=='G' && seq[i+1]=='C' ) ++*ngpc ; 
   if ( seq[i]=='C' || seq[i]=='G' ) ++*ngc ; 
  }
}
