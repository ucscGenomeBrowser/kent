/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */


/*  Last edited: Jun 23 19:33 1995 (gos) */
/*  date unknown (LaDeana Hillier) */
/*  3/23/04 (Angie Hinrichs): use obs/exp from G-G & F '87*/
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
#include <stdlib.h>

/********************* CONTROLS ********************/
#define CPGSCORE  17   /* so that can compare with old cpg - this
			  had CPGSCORE 27, but allowed score to reach
			  0 once without reporting */
#define MALLOCBLOCK 200000
#define MAXNAMELEN 512  /*"length 0, title > MAXNAMELEN or starts with bad character." if too small */

/*------------------------------------------------------*/
int readSequence (FILE *fil, int *conv,
                         char **seq, char **id, char **desc, int *length) ;

void findspans ( int start, int end, char *seq, char *seqname ) ;

void getstats ( int start, int end, char *seq, char *seqname, int *ncpg, int *ngpc, int *ng, int *nc ) ;

void usage (void)
{ 
  fprintf(stderr, "cpglh - calculate CpG Island data for cpgIslandExt tracks\n");
  fprintf(stderr, "usage:\n    cpglh <sequence.fa>\n") ;
  fprintf(stderr, "where <sequence.fa> is fasta sequence, must be more than\n");
  fprintf(stderr, "   200 bases of legitimate sequence, not all N's\n");
  fprintf(stderr, "\nTo process the output into the UCSC bed file format:\n\n"
"cpglh fastaInput.fa \\\n"
" | awk '{$2 = $2 - 1; width = $3 - $2;\n"
"   printf(\"%%s\\t%%d\\t%%s\\t%%s %%s\\t%%s\\t%%s\\t%%0.0f\\t%%0.1f\\t%%s\\t%%s\\n\",\n"
"    $1, $2, $3, $5, $6, width, $6, width*$7*0.01, 100.0*2*$6/width, $7, $9);}' \\\n" \
"     | sort -k1,1 -k2,2n > output.bed\n");

  fprintf(stderr, "\nThe original cpg.c was written by Gos Miklem from the Sanger Center.\n"
"LaDeana Hillier added some modifications --> cpg_lh.c, and UCSC hass\n"
"added some further modifications to cpg_lh.c, so that its expected\n"
"number of CpGs in an island is calculated as described in\n"
"  Gardiner-Garden, M. and M. Frommer, 1987\n"
"  CpG islands in vertebrate genomes. J. Mol. Biol. 196:261-282\n"
"\n"
"    Expected = (Number of C's * Number of G's) / Length\n"
"\n"
"Instead of a sliding-window search for CpG islands, this cpg program\n"
"uses a running-sum score where a 'C' followed by a 'G' increases the\n"
"score by 17 and anything else decreases the score by 1.  When the\n"
"score transitions from positive to 0 (and at the end of the sequence),\n"
"the sequence in the current span is evaluated to see if it qualifies\n"
"as a CpG island (>200 bp length, >50%% GC, >0.6 ratio of observed CpG\n"
"to expected).  Then the search recurses on the span from the position\n"
"with the max running score up to the current position.\n");

  exit (-1);
}
/*------------------------------------------------------*/
int main (int argc, char **argv)
{ 
  
  char *seq, *seqname, *desc ;
  
  int length ;
  int i ;
  static FILE *fil ;

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

void findspans ( int start, int end, char *seq, char *seqname )
{
  int i ;
  int sc = 0 ;
  int lsc = 0 ; 
  int imn = -1 ;  /* else sequences which start with pattern reported badly */
  int imx = 0 ;
  int mx = 0 ;
  int winlen = 0;
  float expect, obsToExp;

  int ncpg, ngpc, ngc, ng, nc;  

  i = start ;
  while ( i < end )  
    {
      lsc = sc ;
      sc += ( end-1-i && seq[i]=='C' && seq[i+1]=='G' ) ? CPGSCORE : -1 ;
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

	  getstats ( imn+1, imx+2, seq, seqname, &ncpg, &ngpc, &ng, &nc ) ;
	  ngc = ng + nc;
      if (((imx+2)-(imn+2))>199 && (ngc*100.0/(imx+1-imn))>50.0 ) {
	/* old gos estimate	  printf("%s\t %d\t %d\t %d\t CpG: %d\t %.1f\t %.1f\n", seqname, imn+2, imx+2, mx, ncpg, ngc*100.0/(imx+1-imn), 1.0*ncpg/ngpc) ; */
	winlen=imx+1-imn;
	/* ASH 3/23/04: expected val from Gardiner-Garden & Frommer '87: */
	expect = (float)(nc * ng) / (float)winlen;
	obsToExp = (float)ncpg / expect;
	if ( obsToExp > 0.60 )
           printf("%s\t %d\t %d\t %d\t CpG: %d\t %.1f\t %.2f\t %.2f\n",
		  seqname, imn+2, imx+2, mx, ncpg, ngc*100.0/(imx+1-imn),
		  1.0*ncpg/ngpc, obsToExp) ; 
      }
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
      
      /* ASH 3/23/04: Make this test & output consistent w/above. */
      getstats ( imn+1, imx+2, seq, seqname, &ncpg, &ngpc, &ng, &nc ) ;
      ngc = nc + ng;
      if (((imx+2)-(imn+2))>199 && (ngc*100.0/(imx+1-imn))>50.0 ) {
	winlen=imx+1-imn;
	expect = (float)(nc * ng) / (float)winlen;
	obsToExp = (float)ncpg / expect;
	if ( obsToExp > 0.60 )
           printf("%s\t %d\t %d\t %d\t CpG: %d\t %.1f\t %.2f\t %.2f\n",
		  seqname, imn+2, imx+2, mx, ncpg, ngc*100.0/(imx+1-imn),
		  1.0*ncpg/ngpc, obsToExp) ; 
      }
      
/*      printf("%s \t %d\t %d\t %d \n", seqname, imn+2, imx+2, mx ) ; 
   */
      findspans( imx+2, end, seq, seqname ) ;
    }
}

/* ASH 3/23/04: Return separate counts of G's and C's for expected val calc. */
void getstats ( int start, int end, char *seq, char *seqname, int *ncpg, int *ngpc, int *ng, int *nc )
{

int i ;
*ncpg = *ngpc = *ng = *nc = 0 ;

for ( i = start ; i < end ; ++i ) 
  {
   if ( end-1-i && seq[i]=='C' && seq[i+1]=='G' ) ++*ncpg ; 
   if ( end-1-i && seq[i]=='G' && seq[i+1]=='C' ) ++*ngpc ; 
   if ( seq[i]=='G' ) ++*ng ; 
   if ( seq[i]=='C' ) ++*nc ; 
  }
}
