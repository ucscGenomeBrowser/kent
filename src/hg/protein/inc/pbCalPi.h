/* pbCalPi - Calculate pI values from a list of protein IDs */

/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hdb.h"

#define PH_MIN 0 /* minimum pH value */
#define PH_MAX 14 /* maximum pH value */
#define MAXLOOP 2000 /* maximum number of iterations */
#define EPSI 0.0001 /* desired precision */
#define ALPHABET_LEN 26 /* number of cPk values */

/* Note: This table comes directly from SwissProt and is copied verbatim, with new comments */
/* Note: Ambiguity codes from http://virology.wisc.edu/acp/CommonRes/SingleLetterCode.html */

/* Table of pk values : */
/*  Note: the current algorithm does not use the last two columns. Each */
/*  row corresponds to an amino acid starting with Ala. J, O and U are */
/*  inexistent, but here only in order to have the complete alphabet. */

/*     Ct    Nt   Sm     Sc     Sn */

static float cPk[ALPHABET_LEN][5] = {
    {3.55, 7.59, 0.   , 0.   , 0.   } ,  /* A; Ala */
    {3.55, 7.50, 0.   , 0.   , 0.   } ,  /* B; Asx: N/D Asp/Asn */
    {3.55, 7.50, 9.00 , 9.00 , 9.00 } ,  /* C; Cys */
    {4.55, 7.50, 4.05 , 4.05 , 4.05 } ,  /* D; Asp */
    {4.75, 7.70, 4.45 , 4.45 , 4.45 } ,  /* E; Glu */
    {3.55, 7.50, 0.   , 0.   , 0.   } ,  /* F; Phe */
    {3.55, 7.50, 0.   , 0.   , 0.   } ,  /* G; Gly */
    {3.55, 7.50, 5.98 , 5.98 , 5.98 } ,  /* H; His */
    {3.55, 7.50, 0.   , 0.   , 0.   } ,  /* I; Ile */
    {0.00, 0.00, 0.   , 0.   , 0.   } ,  /* J; Not used */
    {3.55, 7.50, 10.00, 10.00, 10.00} ,  /* K; Lys */
    {3.55, 7.50, 0.   , 0.   , 0.   } ,  /* L; Leu */
    {3.55, 7.00, 0.   , 0.   , 0.   } ,  /* M; Met */
    {3.55, 7.50, 0.   , 0.   , 0.   } ,  /* N; Asn */
    {0.00, 0.00, 0.   , 0.   , 0.   } ,  /* O; Not used */
    {3.55, 8.36, 0.   , 0.   , 0.   } ,  /* P; Pro */
    {3.55, 7.50, 0.   , 0.   , 0.   } ,  /* Q; Qln */
    {3.55, 7.50, 12.0 , 12.0 , 12.0 } ,  /* R; Arg */
    {3.55, 6.93, 0.   , 0.   , 0.   } ,  /* S; Ser */
    {3.55, 6.82, 0.   , 0.   , 0.   } ,  /* T; Thr */
    {0.00, 0.00, 0.   , 0.   , 0.   } ,  /* U; Not used */
    {3.55, 7.44, 0.   , 0.   , 0.   } ,  /* V; Val */
    {3.55, 7.50, 0.   , 0.   , 0.   } ,  /* W; Trp */
    {3.55, 7.50, 0.   , 0.   , 0.   } ,  /* X; fully degenerate */
    {3.55, 7.50, 10.00, 10.00, 10.00} ,  /* Y; Tyr */
    {3.55, 7.50, 0.   , 0.   , 0.   }    /* Z; Glx: Q/E Glu/Gln */
};

