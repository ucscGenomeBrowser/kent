/*  File: readseq.c
 *  Author: Richard Durbin (rd@sanger.ac.uk)
 *  Copyright (C) R Durbin, 1994
 *-------------------------------------------------------------------
 * Description: generic code to read Pearson format files (fasta)
 		>header line
		conv[x] is the internal code for char 'x'
		conv[x] == -1 means ignore. conv[x] < -1 means error.
		will work on fil == stdin
 * Exported functions: readSequence
 * HISTORY:
 * Last edited: Apr 26 14:44 1994 (rd)
 * * Dec 29 23:35 1993 (rd): now works off FILE*, returns id and desc
 * Created: Tue Jan 19 21:14:35 1993 (rd)
 *-------------------------------------------------------------------
 */

#include "stdio.h"
#include "stdlib.h"
#include "ctype.h"
#include <stdlib.h>
#include <string.h>

static char *messalloc (int n)
{
  char *result ;

  if (!(result = (char*) malloc (n)))
    { fprintf (stderr, "MALLOC failure reqesting %d bytes - aborting\n", n) ;
      exit (-1) ;
    }
  return result ;
}

#define messfree(x) free(x)

static void add (char c, char* *buf, int *buflen, int n)
{
  if (!buf)
    return ;
  if (n >= *buflen)
    { if (*buflen < 0)
	{ *buflen = -*buflen ;
	  *buf = (char*) messalloc (*buflen) ;
	}
      else
	{ char *newbuf ;
	  *buflen *= 2 ;
	  newbuf = (char*) messalloc (*buflen) ;
	  memcpy (newbuf, *buf, n) ;
	  messfree (*buf) ;
	  *buf = newbuf ;
	}
    }
  (*buf)[n] = c ;	  
}

int readSequence (FILE *fil, int *conv,
		  char **seq, char **id, char **desc, int *length)
{
  int c ;
  int n ;
  static FILE *oldFil = 0 ;
  static int line ;
  int buflen ;

  if (fil != oldFil)
    { line = 1 ;
      oldFil = fil ;
    }
  
/* get id, descriptor */
  c = fgetc (fil) ;
  if (c == '>')			/* header line */
    { c = fgetc(fil) ;

      n = 0 ;			/* id */
      buflen = -16 ;
      while (!feof (fil) && c != ' ' && c != '\n' && c != '\t')
	{ add (c, id, &buflen, n++) ;
	  c = fgetc (fil) ;
	}
      add (0, id, &buflen, n++) ;

				/* white space */
      while (!feof (fil) && (c == ' ' || c == '\t'))
	c = fgetc (fil) ;

      n = 0 ;			/* desc */
      buflen = -32 ;
      while (!feof (fil) && c != '\n')
	{ add (c, desc, &buflen, n++) ;
	  c = fgetc (fil) ;
	}
      add (0, desc, &buflen, n++) ;

      ++line ;
    }
  else
    { ungetc (c, fil) ;		/* no header line */
      if (id) 
	*id = "" ;
      if (desc)
	*desc = "" ;
    }

  /* ensure whitespace ignored */

  conv[' '] = conv['\t'] = conv['\n'] = -1 ;

  n = 0 ;			/* sequence */
  buflen = -1024 ;
  while (!feof (fil))
    { c = fgetc (fil) ;
      if (c == '>')
	{ ungetc (c, fil) ;
	  break ;
	}
      if (c == EOF)
	break ;
      if (c == '\n')
	++line ;
      if (conv[c] < -1)
	{ if (id) 
	    fprintf (stderr, "Bad char 0x%x = '%c' at line %d, base %d, sequence %s\n",
		     c, c, line, n, *id) ;
	  else
	    fprintf (stderr, "Bad char 0x%x = '%c' at line %d, base %d\n",
		     c, c, line, n) ;
	/*	  return 0 ;*/
	}
      if (conv[c] >= 0)
	add (conv[c], seq, &buflen, n++) ;
    }
  add (0, seq, &buflen, n) ;
  
  if (length)
    *length = n ;

  return n ;
}

/*****************************************************/

int seqConvert (char *seq, int *length, int *conv)
{
  int i, n = 0 ;
  int c ;

  for (i = 0 ; seq[i] ; ++i)
    { c = seq[i] ;
      if (length && i >= *length)
	break ;
      if (conv[c] < -1)
	{ fprintf (stderr, "Bad char 0x%x = '%c' at base %d in seqConvert\n", c, c, n) ;
	/*	  return 0 ;*/
	}
      if (conv[c] >= 0)
	seq[n++] = conv[c] ;
    }
  if (n < i)
    seq[n] = 0 ;

  if (length)
    *length = n ;
  return n ;
}

/*********** standard conversion tables **************/

int dna2textConv[] = {
  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2, 
  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2, 
  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2, 
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -2,  -2,  -2,  -2,  -2,  -2,  /* ignore digits */
  -2, 'A',  -2, 'C',  -2,  -2,  -2, 'G',  -2,  -2,  -2,  -2,  -2,  -2, 'N',  -2,
  -2,  -2,  -2,  -2, 'T',  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,
  -2, 'A',  -2, 'C',  -2,  -2,  -2, 'G',  -2,  -2,  -2,  -2,  -2,  -2, 'N',  -2,
  -2,  -2,  -2,  -2, 'T',  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,
} ;

int dna2indexConv[] = {
  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2, 
  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2, 
  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2, 
  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -2,  -2,  -2,  -2,  -2,  -2,  /* ignore digits */
  -2,   0,  -2,   1,  -2,  -2,  -2,   2,  -2,  -2,  -2,  -2,  -2,  -2,   4,  -2,
  -2,  -2,  -2,  -2,   3,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,
  -2,   0,  -2,   1,  -2,  -2,  -2,   2,  -2,  -2,  -2,  -2,  -2,  -2,   4,  -2,
  -2,  -2,  -2,  -2,   3,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,  -2,
} ;
  
/**************** end of file ***************/
