/* oligoTm - calculate melting temperature of relatively short DNA sequences.
 * This is based on the nearest-neighbor thermodynamics of bases from Breslauer,
 * Frank, Bloecker, and Markey, Proc. Natl. Acad. Sci. USA, vol 83, page 3748,
 * and uses work from see Rychlik, Spencer, Roads, Nucleic Acids Research, vol 18, 
 * no 21.  This code was imported from the oligotm module of Whitehead Institute's
 * primer3 program, and adapted into UCSC conventions by Jim Kent.  Any redistribution
 * of this code should contain the following copyright notice from Whitehead:
 *
 * Copyright (c) 1996,1997,1998,1999,2000,2001,2004
 *         Whitehead Institute for Biomedical Research. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1.      Redistributions must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the  documentation
 * and/or other materials provided with the distribution.  Redistributions of
 * source code must also reproduce this information in the source code itself.
 * 
 * 2.      If the program is modified, redistributions must include a notice
 * (in the same places as above) indicating that the redistributed program is
 * not identical to the version distributed by Whitehead Institute.
 * 
 * 3.      All advertising materials mentioning features or use of this
 * software  must display the following acknowledgment:
 *         This product includes software developed by the
 *         Whitehead Institute for Biomedical Research.
 * 
 * 4.      The name of the Whitehead Institute may not be used to endorse or
 * promote products derived from this software without specific prior written
 * permission.
 * 
 * We also request that use of this software be cited in publications as 
 * 
 *   Rozen, S., Skaletsky, H.  \"Primer3 on the WWW for general users
 *   and for biologist programmers.\"  In S. Krawetz and S. Misener, eds.
 *   Bioinformatics Methods and Protocols in the series Methods in 
 *   Molecular Biology.  Humana Press, Totowa, NJ, 2000, pages 365-386.
 *   Code available at
 *   http://fokker.wi.mit.edu/primer3/.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE WHITEHEAD INSTITUTE ``AS IS'' AND  ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE  ARE
 * DISCLAIMED. IN NO EVENT SHALL THE WHITEHEAD INSTITUTE BE LIABLE  FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL  DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS  OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)  HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE. */



#include "common.h"
#include "oligoTm.h"

static char const rcsid[] = "$Id: oligoTm.c,v 1.2 2008/09/17 17:56:38 kent Exp $";

/* 
 * Tables of nearest-neighbor thermodynamics for DNA bases.  See Breslauer,
 * Frank, Bloecker, and Markey, Proc. Natl. Acad. Sci. USA, vol 83, page 3748,
 * table 2.
 */
#define S_A_A 240
#define S_A_C 173
#define S_A_G 208
#define S_A_T 239
#define S_A_N 215
  
#define S_C_A 129
#define S_C_C 266
#define S_C_G 278
#define S_C_T 208
#define S_C_N 220  
  
#define S_G_A 135
#define S_G_C 267
#define S_G_G 266
#define S_G_T 173
#define S_G_N 210
  
#define S_T_A 169
#define S_T_C 135
#define S_T_G 129
#define S_T_T 240
#define S_T_N 168
  
#define S_N_A 168
#define S_N_C 210
#define S_N_G 220
#define S_N_T 215
#define S_N_N 203


#define H_A_A  91
#define H_A_C  65
#define H_A_G  78
#define H_A_T  86
#define H_A_N  80

#define H_C_A  58
#define H_C_C 110
#define H_C_G 119
#define H_C_T  78
#define H_C_N  91

#define H_G_A  56
#define H_G_C 111
#define H_G_G 110
#define H_G_T  65
#define H_G_N  85

#define H_T_A  60
#define H_T_C  56
#define H_T_G  58
#define H_T_T  91
#define H_T_N  66

#define H_N_A  66
#define H_N_C  85
#define H_N_G  91
#define H_N_T  80
#define H_N_N  80

/* Delta G's of disruption * 1000. */
#define G_A_A  1900
#define G_A_C  1300
#define G_A_G  1600
#define G_A_T  1500
#define G_A_N  1575

#define G_C_A  1900 
#define G_C_C  3100
#define G_C_G  3600
#define G_C_T  1600
#define G_C_N  2550

#define G_G_A  1600
#define G_G_C  3100
#define G_G_G  3100
#define G_G_T  1300
#define G_G_N  2275

#define G_T_A   900
#define G_T_C  1600
#define G_T_G  1900
#define G_T_T  1900
#define G_T_N  1575

#define G_N_A  1575
#define G_N_C  2275
#define G_N_G  2550
#define G_N_T  1575
#define G_N_N  1994

#define A_CHAR 'A'
#define G_CHAR 'G'
#define T_CHAR 'T'
#define C_CHAR 'C'
#define N_CHAR 'N'

#define CATID5(A,B,C,D,E) A##B##C##D##E
#define CATID2(A,B) A##B
#define DO_PAIR(LAST,THIS)          \
  if (CATID2(THIS,_CHAR) == c) {    \
     dh += CATID5(H,_,LAST,_,THIS); \
     ds += CATID5(S,_,LAST,_,THIS); \
     goto CATID2(THIS,_STATE);      \
  }

#define STATE(LAST)     \
   CATID2(LAST,_STATE): \
   c = *s; s++;         \
   DO_PAIR(LAST,A)      \
   else DO_PAIR(LAST,T) \
   else DO_PAIR(LAST,G) \
   else DO_PAIR(LAST,C) \
   else DO_PAIR(LAST,N) \
   else if ('\0' == c)  \
             goto DONE; \
   else goto ERROR \

double oligoTm(char *dna, double DNA_nM, double K_mM)
/* Calculate melting point of short DNA sequence given DNA concentration in 
 * nanomoles, and salt concentration in millimoles.  This is calculated using eqn
 * (ii) in Rychlik, Spencer, Roads, Nucleic Acids Research, vol 18, no 21, page
 * 6410, with tables of nearest-neighbor thermodynamics for DNA bases as
 * provided in Breslauer, Frank, Bloecker, and Markey,
 * Proc. Natl. Acad. Sci. USA, vol 83, page 3748. */
{
    register int dh = 0, ds = 108;
    register char c;
    char *dupe = cloneString(dna);
    char *s = dupe;
    double delta_H, delta_S;

    touppers(s);
    /* Use a finite-state machine (DFA) to calucluate dh and ds for s. */
    c = *s; s++;
    if (c == 'A') goto A_STATE;
    else if (c == 'G') goto G_STATE;
    else if (c == 'T') goto T_STATE;
    else if (c == 'C') goto C_STATE;
    else if (c == 'N') goto N_STATE;
    else goto ERROR;
    STATE(A);
    STATE(T);
    STATE(G);
    STATE(C);
    STATE(N);

    DONE:  /* dh and ds are now computed for the given sequence. */
    delta_H = dh * -100.0;  /* 
			     * Nearest-neighbor thermodynamic values for dh
			     * are given in 100 cal/mol of interaction.
			     */
    delta_S = ds * -0.1;     /*
			      * Nearest-neighbor thermodynamic values for ds
			      * are in in .1 cal/K per mol of interaction.
			      */

    /* 
     * See Rychlik, Spencer, Roads, Nucleic Acids Research, vol 18, no 21,
     * page 6410, eqn (ii).
     */
    freeMem(dupe);
    return delta_H / (delta_S + 1.987 * log(DNA_nM/4000000000.0))
	- 273.15 + 16.6 * log10(K_mM/1000.0);

    ERROR:  /* 
	  * length of s was less than 2 or there was an illegal character in
	  * s.
	  */
    freeMem(dupe);
    errAbort("Not a valid oligo in oligoTm.");
    return 0;
}
#undef DO_PAIR

#define DO_PAIR(LAST,THIS)          \
  if (CATID2(THIS,_CHAR) == c) {    \
     dg += CATID5(G,_,LAST,_,THIS); \
     goto CATID2(THIS,_STATE);      \
  }

double oligoDg(char *dna)
/* Calculate dg (change in Gibb's free energy) from melting oligo
 * the nearest neighbor model. Seq should be relatively short, given 
 * the characteristics of the nearest neighbor model (36 bases or less
 * is best). */
{
    register int dg = 0;
    register char c;
    char *dupe = cloneString(dna);
    char *s = dupe;

    /* Use a finite-state machine (DFA) to calculate dg s. */
    c = *s; s++;
    if (c == 'A') goto A_STATE;
    else if (c == 'G') goto G_STATE;
    else if (c == 'T') goto T_STATE;
    else if (c == 'C') goto C_STATE;
    else if (c == 'N') goto N_STATE;
    else goto ERROR;
    STATE(A);
    STATE(T);
    STATE(G);
    STATE(C);
    STATE(N);

    DONE:  /* dg is now computed for the given sequence. */
    freeMem(dupe);
    return dg / 1000.0;

    ERROR:
    freeMem(dupe);
    errAbort("Not a valid oligo in oligoDg.");
    return 0;
}


double longSeqTm(char *s, int start, int len, double salt_conc)
/* Calculate the melting temperature of substr(seq, start, length) using the
 * formula from Bolton and McCarthy, PNAS 84:1390 (1962) as presented in
 * Sambrook, Fritsch and Maniatis, Molecular Cloning, p 11.46 (1989, CSHL
 * Press).
 *
 * Tm = 81.5 + 16.6(log10([Na+])) + .41*(%GC) - 600/length
 *
 * Where [Na+] is the molar sodium concentration, (%GC) is the percent of Gs
 * and Cs in the sequence, and length is the length of the sequence.
 *
 * A similar formula is used by the prime primer selection program in GCG
 * (http://www.gcg.com), which instead uses 675.0 / length in the last term
 * (after F. Baldino, Jr, M.-F. Chesselet, and M.E.  Lewis, Methods in
 * Enzymology 168:766 (1989) eqn (1) on page 766 without the mismatch and
 * formamide terms).  The formulas here and in Baldino et al. assume Na+ rather
 * than K+.  According to J.G. Wetmur, Critical Reviews in BioChem. and
 * Mol. Bio. 26:227 (1991) 50 mM K+ should be equivalent in these formulae to .2
 * M Na+.
 *
 * This function takes salt_conc to be the millimolar (mM) concentration,
 * since mM is the usual units in PCR applications.  */
{
  int GC_count = 0;
  char *p, *end;

  if(start + len > strlen(s) || start < 0 || len <= 0) 
	errAbort("bad input to longSeqTm");
  end = &s[start + len];
  /* Length <= 0 is nonsensical. */
  for (p = &s[start]; p < end; p++) {
    if ('G' == *p || 'g' == *p || 'C' == *p || 'c' == *p)
      GC_count++;
  }

  return
    81.5
    + (16.6 * log10(salt_conc / 1000.0))
    + (41.0 * (((double) GC_count) / len))
    - (600.0 / len);

}

double seqTm(char *seq, double dna_conc, double salt_conc)
/* Figure out melting temperature of sequence of any length given
 * dna and salt concentration. */
{
  int len = strlen(seq);
  return (len > 36)
    ? longSeqTm(seq, 0, len, salt_conc) : oligoTm(seq, dna_conc, salt_conc);
}
